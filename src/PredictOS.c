/**
 * @file PredictOS.c
 * @author Yi Ren
 * @brief Source file of Predict OS
 */

#include <test_env.h>
#include "systick.h"
#include "PredictOS.h"
#include <string.h>

// ICSR register for triggering PendSV interrupt
#define ICSR_ADDR           (0xE000ED04U)
#define ICSR_REG            (*((volatile uint32_t *)ICSR_ADDR))
#define PENDSVSET_OFFSET    (28U)
#define PENDSVSET_BITMASK   (1U << PENDSVSET_OFFSET)
// SHPR3 register for setting PenSV interrupt priority
#define SHPR3_ADDR          (0xE000ED20U)
#define SHPR3_REG           (*((volatile uint32_t *)SHPR3_ADDR))
#define PENDSV_PRIO_OFFSET  (16U)
#define PENDSV_PRIO_BITMASK (0xFFU << PENDSV_PRIO_OFFSET)
// hardware semaphore
#define HSEM2_BASE          (0x44002C00UL)
#define HSEM2_R0            (*(volatile uint32_t *)(HSEM2_BASE + 0x000))
#define HSEM2_RLR0          (*(volatile uint32_t *)(HSEM2_BASE + 0x080))
// specified HSEM core id
#define HSEM_COREID_CORE1   (0x8U << 8)
#define HSEM_COREID_CORE2   (0x1U << 8)
#define HSEM_LOCK_BIT       (1UL  << 31)

#define GET_MAX_PRIO(x)     (32U - __builtin_clz(x))

#define likely(x)           __builtin_expect(!!(x), 1)
#define unlikely(x)         __builtin_expect(!!(x), 0)

#if MAX_TASK_NUM > 32U
#error "MAX_TASK_NUM cannot exceed 32"
#endif

#if (!defined(PDOS_CURR_CORE_ID)) || ((PDOS_CURR_CORE_ID != 1) && (PDOS_CURR_CORE_ID != 2))
#error "core ID invalid"
#endif

#if PDOS_CURR_CORE_ID == 1
#define PDOS_MAIN_MEM_BASE PDOS_CORE1_MAIN_MEM_BASE
#define PDOS_MAIN_MEM_SIZE PDOS_CORE1_MAIN_MEM_SIZE
#else
#define PDOS_MAIN_MEM_BASE PDOS_CORE2_MAIN_MEM_BASE
#define PDOS_MAIN_MEM_SIZE PDOS_CORE2_MAIN_MEM_SIZE
#endif

#if MEM_POOL_SIZE + MAX_LOG_NUM * 2 > PDOS_MAIN_MEM_SIZE
#error "memory requirement too high"
#endif

// task phases
typedef enum _PdOSTaskPhaseType
{
    PDOS_IDLE_PHASE     = 0U,
    PDOS_READ_PHASE     = 1U,
    PDOS_EXECUTE_PHASE  = 2U,
    PDOS_WRITE_PHASE    = 3U
} PdOSTaskPhaseType;

// log event
typedef enum _PdOSLogEventType
{
    PDOS_ENTER_READ     = 0U,
    PDOS_EXIT_READ      = 1U,
    PDOS_ENTER_EXECUTE  = 2U,
    PDOS_EXIT_EXECUTE   = 3U,
    PDOS_ENTER_WRITE    = 4U,
    PDOS_EXIT_WRITE     = 5U
} PdOSLogEventType;

// task control block
typedef struct _PdOSTaskControlBlock
{
    void     *psp;            // stack pointer
    uint32_t  wkUpTime;       // next wake up time
    uint8_t   prio;           // priority
    uint8_t   threshold;      // preemption threshold
    uint8_t   phase;          // current phase
    uint8_t   preserve;       // align by 4 bytes
    uint32_t  memStartIndex;  // start index in memory pool
    uint32_t  memSize;        // memory size
} PdOSTaskControlBlock;

// task handle
typedef PdOSTaskControlBlock *PdOSTaskHandle;

// arbiter data structure
typedef struct
{
    volatile uint8_t req[2];    // request of two cores
    volatile uint8_t prio[2];   // priorities
} ArbiterType;

#define ARBITER ((volatile ArbiterType *)PDOS_ARB_SHM_BASE)

extern uint32_t SystemCoreClock;

// all variables should be placed in core-local memory
PDOS_DTCM static PdOSTaskControlBlock tcbPool[MAX_TASK_NUM + 1U] = {};
PDOS_DTCM static PdOSTaskHandle tasks[MAX_TASK_NUM + 1U] = {};
PDOS_DTCM static volatile PdOSTaskHandle currTask = NULL;
PDOS_DTCM static volatile PdOSTaskHandle nextTask = NULL;
PDOS_DTCM static uint32_t readySet = 0U;
PDOS_DTCM static uint32_t blockedSet = 0U;
PDOS_DTCM static uint32_t idleTaskStack[32U];
PDOS_DTCM static volatile uint32_t systime = 0U;

// memory in local and main memory
PDOS_DTCM static uint8_t localMem[MAX_LOCAL_MEM_SIZE];  // local memory for task execution
PDOS_DTCM static uint32_t localMemFreeIndex = 0U;
PDOS_DTCM static uint8_t *memPool = (uint8_t *)PDOS_MAIN_MEM_BASE;  // memory pool in main memory (monotonic allocated)
PDOS_DTCM static uint32_t mainMemFreeIndex = 0U;

// circular buffer for log storage
PDOS_DTCM static uint32_t localLogBuffer[MAX_LOG_NUM * 2U] = {};
PDOS_DTCM static uint32_t logIndex = 0U;
PDOS_DTCM static uint32_t *logBuffer = (uint32_t *)(PDOS_MAIN_MEM_BASE + MEM_POOL_SIZE);  // log buffer in main memory

// Add an entry to log.
static void PdOS_add_log(uint8_t prio, PdOSLogEventType logEvent)
{
    /* 
      log format:
        systime(32 bits) - currIterTime(16 bits) - priority(8 bits) - event(8 bits)
     --> release time can be caculated by systime - currIterTime
    */
    uint32_t currIterTime;
    
    localLogBuffer[logIndex * 2U] = systime;
    localLogBuffer[logIndex * 2U + 1U] = (uint32_t)((prio << 8U) | ((uint8_t)logEvent));
    
    currIterTime = systime - tasks[prio]->wkUpTime;
    localLogBuffer[logIndex * 2U + 1U] |= (currIterTime << 16U);
    
    logIndex = (logIndex + 1U < MAX_LOG_NUM) ? (logIndex + 1U) : 0U;
}

// Create a new task.
PdOSErrCode PdOS_create_task(PdOSTaskFunction taskFunction, uint8_t prio, uint8_t threshold, void *stkSto, uint32_t stkSize, uint32_t memSize)
{
    PdOSTaskHandle h;
    uint32_t *psp;

    // stack size needs to be at least 64 bytes to store one frame
    if (unlikely((stkSto == NULL) || (stkSize < 64U)))
    {
        return PDOS_INVALID_PARAM;
    }

    // requires an unique priority no greater than MAX_TASK_NUM
    if (unlikely((prio > MAX_TASK_NUM) || (tasks[prio] != NULL)))
    {
        return PDOS_INVALID_PARAM;
    }

    // preemption threshold needs to be no less than priority
    if (unlikely(threshold < prio))
    {
        return PDOS_INVALID_PARAM;
    }

    // ensure enough memory space
    if (unlikely((memSize > MAX_LOCAL_MEM_SIZE) || (mainMemFreeIndex + memSize > MEM_POOL_SIZE)))
    {
        return PDOS_ERROR;
    }

    // stack top, aligned by 8 bytes
    psp = (uint32_t *)(((uint32_t)stkSto + stkSize) & ~7U);

    // hardware-stacked registers, corresponds to ARMv7-M
    *(--psp) = (1U << 24U);  // xPSR, THUMB bit set
    *(--psp) = (uint32_t)taskFunction;  // PC
    *(--psp) = 0x0000000EU;  // LR
    *(--psp) = 0x0000000CU;  // R12
    *(--psp) = 0x00000003U;  // R3
    *(--psp) = 0x00000002U;  // R2
    *(--psp) = 0x00000001U;  // R1
    *(--psp) = 0x00000000U;  // R0
    // software-saved registers
    *(--psp) = 0x0000000BU;  // R11
    *(--psp) = 0x0000000AU;  // R10
    *(--psp) = 0x00000009U;  // R9
    *(--psp) = 0x00000008U;  // R8
    *(--psp) = 0x00000007U;  // R7
    *(--psp) = 0x00000006U;  // R6
    *(--psp) = 0x00000005U;  // R5
    *(--psp) = 0x00000004U;  // R4

    // save TCB to pool
    h = &tcbPool[prio];

    h->psp = psp;
    h->prio = prio;
    h->threshold = threshold;
    h->phase = (uint8_t)PDOS_IDLE_PHASE;
    h->wkUpTime = 0U;

    // allocate space in main memory
    h->memSize = memSize;
    h->memStartIndex = mainMemFreeIndex;
    mainMemFreeIndex += memSize;
    
    tasks[prio] = h;

    if (likely(prio > 0U))
    {
        // set the new task ready to run
        readySet |= (1U << (prio - 1U));
    }

    return PDOS_OK;
}

// Find available task with the highest nominal priority.
static PdOSTaskHandle PdOS_find_hi_prio_avail_task(void)
{
    uint32_t tmpReadySet = readySet;
    uint32_t maxPrio;
    PdOSTaskHandle t;

    while (tmpReadySet != 0U)
    {
        maxPrio = GET_MAX_PRIO(tmpReadySet);
        t = tasks[maxPrio];

        // if a task has already started, no need to check memory availability
        if (t->phase == (uint8_t)PDOS_EXECUTE_PHASE)
        {
            return t;
        }

        // a task is allowed to run only when there is enough space in core-local memory
        if (localMemFreeIndex + t->memSize <= MAX_LOCAL_MEM_SIZE)
        {
            return t;
        }

        tmpReadySet &= ~(1U << (maxPrio - 1U));
    }

    // return idle task when no task is available
    return tasks[0U];
}

// Find pending task with the highest threshold.
// No need to check memory availability here, as pending tasks are already in core-local memory.
static PdOSTaskHandle PdOS_find_hi_thres_pending_task(void)
{
    uint32_t tmpReadySet = readySet;
    uint32_t currIndex;
    uint32_t maxIndex = 0U;
    PdOSTaskHandle t;

    while (tmpReadySet != 0U)
    {
        currIndex = GET_MAX_PRIO(tmpReadySet);
        t = tasks[currIndex];

        // when threshold equals, comparation is based on prio
        // (use > not >= so that lo-prio task will not be maxIndex)
        if ((t->phase == (uint8_t)PDOS_EXECUTE_PHASE) && (t->threshold > tasks[maxIndex]->threshold))
        {
            maxIndex = currIndex;
        }

        tmpReadySet &= ~(1U << (currIndex - 1U));
    }

    return tasks[maxIndex];
}

// Schedule the next task to run.
// Needs to be called in a critical section.
static void PdOS_sched(void)
{
    PdOSTaskHandle pendingTask;

    // on init
    if (unlikely(currTask == NULL))
    {
        nextTask = PdOS_find_hi_prio_avail_task();
        ICSR_REG |= PENDSVSET_BITMASK;
        return;
    }

    // memory phases cannot be preempted
    if ((currTask->phase == (uint8_t)PDOS_READ_PHASE) || (currTask->phase == (uint8_t)PDOS_WRITE_PHASE))
    {
        return;
    }

    // assume nextTask is the task with highest nominal priority and adjust later
    nextTask = PdOS_find_hi_prio_avail_task();
    if (nextTask == currTask)
    {
        return;
    }

    // nextTask wants to switch context
    // preemption path    
    if (nextTask->prio > currTask->prio)
    {
        // preemption not allowed when priority <= threshold
        if ((currTask->phase == (uint8_t)PDOS_EXECUTE_PHASE) && (nextTask->prio <= currTask->threshold))
        {
            // return and pendSV will not be triggered
            return;
        }
    }
    // CPU yield path
    else
    {
        // prio of running tasks are escalated to their thresholds.
        // thus nextTask = max{nominal prio of all tasks, threshold of running tasks}.
        pendingTask = PdOS_find_hi_thres_pending_task();
        if ((nextTask != pendingTask) && (pendingTask->threshold >= nextTask->prio))
        {
            nextTask = pendingTask;
        }
        // nextTask != currTask in all cases of CPU yield. so no need to add if statement afterward
    }

    // record log of preemption / resume
    if ((nextTask != tasks[0U]) && (currTask != tasks[0U]))
    {
        // preemption path
        if (nextTask->prio > currTask->prio)
        {
            if (currTask->phase == (uint8_t)PDOS_EXECUTE_PHASE)
            {
                // add log: preempted task suspends execution
                PdOS_add_log(currTask->prio, PDOS_EXIT_EXECUTE);
            }
        }
        // CPU yield path
        else
        {
            if (nextTask->phase == (uint8_t)PDOS_EXECUTE_PHASE)
            {
                // add log: preempted task resumes execution
                PdOS_add_log(nextTask->prio, PDOS_ENTER_EXECUTE);
            }
        }
    }

    // trigger the PendSV interrupt
    ICSR_REG |= PENDSVSET_BITMASK;
}

// Idle task function.
static void PdOS_idle_task_func(void)
{
    while (1)
    {
        // yield CPU when having task ready
        // used in fully non-preemptive mode
        if (readySet != 0U)
        {
            __disable_irq();
            PdOS_sched();
            __enable_irq();
        }
    }
}

// Set priority of PendSV interrupt
static inline void PdOS_set_pendsv_prio(uint8_t prio)
{
    uint32_t regVal = SHPR3_REG;

    // clear pendSV prio bits
    regVal &= ~PENDSV_PRIO_BITMASK;
    // set new prio
    regVal |= ((uint32_t)prio << PENDSV_PRIO_OFFSET);

    SHPR3_REG = regVal;
}

// Initialize OS.
PdOSErrCode PdOS_init(void)
{
    // set PendSV priority to 0xFF
    PdOS_set_pendsv_prio(0xFFU);

    // create idle task
    return PdOS_create_task(&PdOS_idle_task_func, 0U, 0U, idleTaskStack, sizeof(idleTaskStack), 0U);
}

// Get current system time.
uint32_t PdOS_get_systime(void)
{
    return systime;
}

// Block current task and switch away.
// Needs to be called in a critical section.
static inline void PdOS_block_curr_task(uint32_t nextWkUpTime)
{
    uint32_t bitmask;

    bitmask = 1U << (currTask->prio - 1U);
    currTask->wkUpTime = nextWkUpTime;
    readySet &= ~bitmask;
    blockedSet |= bitmask;
    PdOS_sched();  // switch away from the current task
}

// Busy wait for a relative time (keep CPU).
void PdOS_busy_wait(uint32_t ticks)
{
#if USE_BUSY_WAIT_DUMMY == 1U
    // busy wait by executing dummy cycles
    uint32_t i;
    for (i = 0U; i < ticks * DUMMY_CYC_PER_SEC; i++)
    {
        /* busy wait */
    }

#else
    // busy wait by waiting systick
    uint32_t releaseTime = systime + ticks;
    while ((int32_t)(releaseTime - systime) >= 0)
    {
        /* busy wait */
    }

#endif
}

// Delay for a relative time (yield CPU).
void PdOS_delay(uint32_t ticks)
{
    // idle task should not be blocked
    // ticks should not be zero, otherwise underflow will occur
    if (unlikely((currTask == tasks[0U]) || (ticks == 0U)))
    {
        return;
    }

    __disable_irq();
    PdOS_block_curr_task(systime + ticks);
    __enable_irq();
}

// Delay until a specific absolute time (yield CPU).
void PdOS_delayUntil(uint32_t *prevWkUpTime, uint32_t period)
{
    uint32_t nextWkUpTime;

    // handle illegal cases. similar with above
    if (unlikely((currTask == tasks[0U]) || (period == 0U) || (prevWkUpTime == NULL)))
    {
        return;
    }

    __disable_irq();

    nextWkUpTime = *prevWkUpTime + period;
    *prevWkUpTime = nextWkUpTime;

    // deadline already missed
    if (unlikely((int32_t)(systime - nextWkUpTime) >= 0))
    {
        __enable_irq();
        return;
    }
    
    PdOS_block_curr_task(nextWkUpTime);

    __enable_irq();
}

// Lock hardware semaphore.
static inline void PdOS_HSEM_lock(void)
{
    uint32_t hsmCoreId = (PDOS_CURR_CORE_ID == 1) ? HSEM_COREID_CORE1 : HSEM_COREID_CORE2;
    uint32_t expected;

    // try to lock HSEM
    expected = HSEM_LOCK_BIT | hsmCoreId;
    while ((HSEM2_RLR0 & (HSEM_LOCK_BIT | 0xF00U)) != expected);
}

// Unlock hardware semaphore.
static inline void PdOS_HSEM_unlock(void)
{
    uint32_t hsmCoreId = (PDOS_CURR_CORE_ID == 1) ? HSEM_COREID_CORE1 : HSEM_COREID_CORE2;

    HSEM2_R0 = hsmCoreId;
    __DSB();
}

// Acquire arbitration for shared memory access.
static void PdOS_arbiter_acquire(uint8_t prio)
{
    uint8_t other = (PDOS_CURR_CORE_ID == 1) ? 2U : 1U;   
    uint32_t i;

    // write request and priority to arbiter
    // write req after prio to ensure prio updated
    ARBITER->prio[PDOS_CURR_CORE_ID - 1U] = prio;
    __DSB();
    ARBITER->req[PDOS_CURR_CORE_ID - 1U] = 1U;
    __DSB();

    for (i = 0U; i < PDOS_ARBITER_WAIT_WINDOW_ITER; i++)
    {
        if ((ARBITER->req[other - 1U] != 0U))
        {
            break;
        }
        __DSB();
    }

    // wait if the other core is requesting with a higher priority
    while ((ARBITER->req[other - 1U] != 0U) && (ARBITER->prio[other - 1U] > prio))
    {
        // delay for a while to lower bus access frequency
        for (i = 0; i < PDOS_ARBITER_BACKOFF_ITER; i++);

        __DSB();
    }

    PdOS_HSEM_lock();
}

// Release shared memory access.
static void PdOS_arbiter_release(void)
{
    PdOS_HSEM_unlock();

    ARBITER->req[PDOS_CURR_CORE_ID - 1U] = 0U;
    __DSB();
}

// Init arbiter for shared memory access.
// should be called by the main core before starting the other core.
void PdOS_arbiter_init(void)
{
#if PDOS_CURR_CORE_ID == 1

    // clear arbiter data
    ARBITER->req[0]  = 0U;
    ARBITER->req[1]  = 0U;
    ARBITER->prio[0] = 0U;
    ARBITER->prio[1] = 0U;

    // enable HSEM clock
    RCC->AHB1LENR |= RCC_AHB1LENR_HSEM;

    // lock HSEM for synchronizing systick on both cores
    PdOS_HSEM_lock();

#endif
}

// Maintain systime and ready/blocked set
static void PdOS_tick(void)
{
    PdOSTaskHandle h;
    uint32_t bitmask;
    uint32_t tmpBlockedSet = blockedSet;

#if PDOS_USE_STOP_TIME == 1U
    if (unlikely(systime >= PDOS_STOP_TIME))
    {
        // stop maintaining systime and halt the kernel
        return;
    }
#endif

    systime++;
    
    while (tmpBlockedSet != 0U)
    {
        h = tasks[GET_MAX_PRIO(tmpBlockedSet)];
        bitmask = (1U << (h->prio - 1U));

        // convert to signed integer to handle systime overflow
        if ((int32_t)(systime - h->wkUpTime) >= 0)
        {
            readySet |= bitmask;
            blockedSet &= ~bitmask;
        }
        tmpBlockedSet &= ~bitmask;
    }
    
    // no need to call the scheduler explicitly,
    // as it is called at the end of every systick
}

// Callback function for systick.
static void PdOS_systick_callback(systick_driver_t *sdp)
{
    (void)sdp;
    PdOS_tick();

// call scheduler at every systick only when preemptions are allowed
#if ALLOW_PREEMPTION == 1U
    __disable_irq();
    PdOS_sched();
    __enable_irq();
#endif

}

// Initialize systick.
static void PdOS_set_systick(void)
{
    systick_init(&DRV_SYSTICK);
    systick_set_prio(&DRV_SYSTICK, 0U);  // set systick to the highest priority
    systick_set_relval(&DRV_SYSTICK, (SystemCoreClock / TICKS_PER_SECOND));
    systick_set_cb(&DRV_SYSTICK, PdOS_systick_callback);

    // use HSEM to synchronize systick on two cores
#if PDOS_CURR_CORE_ID == 1
    // wait for a while to make sure that core 2 has started and is blocked by HSEM
    PdOS_busy_wait(1U);
    PdOS_HSEM_unlock();
#else
    // core 2 tries to acquire HSEM, and will be blocked.
    PdOS_HSEM_lock();
    PdOS_HSEM_unlock();
#endif

    // two cores should be synchronized here
    systick_start(&DRV_SYSTICK);
}

// Set OS to run.
void PdOS_run()
{
    PdOS_set_systick();

    // call the scheduler explicitly to transfer control to the RTOS
    __disable_irq();
    PdOS_sched();
    __enable_irq();

    // the following code should never execute
    while (1);
}

// Copy data from main memory to core-local memory.
void *PdOS_read(uint32_t extraDelayTime)
{
    uint8_t *currLocalMem;
    
    // acquire access to shared memory
    PdOS_arbiter_acquire(currTask->prio);

    currTask->phase = (uint8_t)PDOS_READ_PHASE;
    PdOS_add_log(currTask->prio, PDOS_ENTER_READ);

    currLocalMem = localMem + localMemFreeIndex;
    localMemFreeIndex += currTask->memSize;
    memcpy((void *)currLocalMem, (void *)&memPool[currTask->memStartIndex], currTask->memSize);
    PdOS_busy_wait(extraDelayTime);
    
    PdOS_add_log(currTask->prio, PDOS_EXIT_READ);

    // finish access to shared memory
    PdOS_arbiter_release();

    currTask->phase = (uint8_t)PDOS_EXECUTE_PHASE;
    PdOS_add_log(currTask->prio, PDOS_ENTER_EXECUTE);

    return (void *)currLocalMem;
}

// Copy data from core-local memory back to main memory.
void PdOS_write(uint32_t extraDelayTime)
{
    uint8_t *currLocalMem;

    // acquire access to shared memory
    PdOS_arbiter_acquire(currTask->prio);
    
    currTask->phase = (uint8_t)PDOS_WRITE_PHASE;
    PdOS_add_log(currTask->prio, PDOS_EXIT_EXECUTE);
    PdOS_add_log(currTask->prio, PDOS_ENTER_WRITE);

    localMemFreeIndex -= currTask->memSize;
    currLocalMem = localMem + localMemFreeIndex;
    memcpy((void *)&memPool[currTask->memStartIndex], (void *)currLocalMem, currTask->memSize);
    // also copy log to main memory in WRITE phase
    memcpy((void *)logBuffer, (void *)localLogBuffer, sizeof(localLogBuffer));
    PdOS_busy_wait(extraDelayTime);

    // finish access to shared memory
    PdOS_arbiter_release();

    currTask->phase = (uint8_t)PDOS_IDLE_PHASE;
    PdOS_add_log(currTask->prio, PDOS_EXIT_WRITE);
}

// Assembly function for context switch.
__attribute__((naked)) void PendSV_Handler(void)
{
    __asm volatile (
        "  CPSID         I                   \n"  // disable interrupt

        "  LDR           r2, =currTask       \n"
        "  LDR           r1, [r2, #0]        \n"  // r1 = currTask
        "  CBZ           r1, PendSV_restore  \n"  // if null, skip save

        // save context
        "  MRS           r0, psp             \n"  // read PSP
        "  STMDB         r0!, {r4-r11}       \n"  // store R4~R11 according to PSP

        "  STR           r0, [r1, #0]        \n"  // currTask->psp = PSP now

        "PendSV_restore:                     \n"
        // restore context
        "  LDR           r2, =nextTask       \n"
        "  LDR           r1, [r2, #0]        \n"  // r1 = nextTask
        "  LDR           r0, [r1, #0]        \n"  // r0 = nextTask->psp

        "  LDMIA         r0!, {r4-r11}       \n"  // restore R4~R11 according to PSP
        "  MSR           psp, r0             \n"  // update PSP

        // currTask = nextTask
        "  LDR           r2, =currTask       \n"
        "  STR           r1, [r2, #0]        \n"

        "  CPSIE         I                   \n"  // enable interrupt
        "  BX            lr                  \n"
    );
}
