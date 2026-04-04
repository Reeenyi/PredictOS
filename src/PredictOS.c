/**
 * @file PredictOS.c
 * @author Yi Ren
 * @brief Source file of Predict OS
 */

#include <test_env.h>
#include "systick.h"
#include "PredictOS.h"
#include <string.h>

#define ICSR_ADDR (0xE000ED04U)
#define ICSR_REG (*((volatile uint32_t *)ICSR_ADDR))
#define PENDSVSET_OFFSET (28U)
#define PENDSVSET_BITMASK (1U << PENDSVSET_OFFSET)

#define SHPR3_ADDR (0xE000ED20U)
#define SHPR3_REG (*((volatile uint32_t *)SHPR3_ADDR))
#define PENDSV_PRIO_OFFSET (16U)
#define PENDSV_PRIO_BITMASK (0xFFU << PENDSV_PRIO_OFFSET)

#define SET_PENDSV_PRIO(prio) \
			(SHPR3_REG = (SHPR3_REG & ~PENDSV_PRIO_BITMASK) |	\
            ((((uint32_t)(prio)) << PENDSV_PRIO_OFFSET) &		\
            PENDSV_PRIO_BITMASK))

#define GET_TASK_INDEX(x) (32U - __builtin_clz(x))

#if MAX_TASK_NUM > 32U
#error "MAX_TASK_NUM cannot exceed 32"
#endif

// log event
typedef enum _PdOSLogEventType
{
	PDOS_ENTER_READ 	= 0U,
	PDOS_EXIT_READ 		= 1U,
	PDOS_ENTER_EXECUTE 	= 2U,
	PDOS_EXIT_EXECUTE 	= 3U,
	PDOS_ENTER_WRITE 	= 4U,
	PDOS_EXIT_WRITE 	= 5U
} PdOSLogEventType;

// task phases
typedef enum _PdOSTaskPhaseType
{
	PDOS_IDLE_PHASE  	= 0U,
	PDOS_READ_PHASE 	= 1U,
	PDOS_EXECUTE_PHASE 	= 2U,
	PDOS_WRITE_PHASE 	= 3U
} PdOSTaskPhaseType;

// Task Control Block
struct PdOSTaskControlBlock
{
	void *psp;            	// stack pointer
	uint32_t wkUpTime;    	// next wake up time
	uint8_t prio;         	// priority
	uint8_t currPhase;		// current phase
	uint8_t preserve[2];  	// align by 4 bytes
	uint32_t memStartIndex;	// start index in memory pool
	uint32_t memSize;		// memory size
};

extern uint32_t SystemCoreClock;

// all these variables should be placed in core-local memory
PDOS_DTCM static struct PdOSTaskControlBlock tcbPool[MAX_TASK_NUM + 1U] = {};
PDOS_DTCM static PdOSTaskHandle tasks[MAX_TASK_NUM + 1U] = {};
PDOS_DTCM static volatile PdOSTaskHandle currTask = NULL;
PDOS_DTCM static volatile PdOSTaskHandle nextTask = NULL;
PDOS_DTCM static uint32_t readySet = 0U;
PDOS_DTCM static uint32_t blockedSet = 0U;
PDOS_DTCM static uint32_t idleTaskStack[32];
PDOS_DTCM static volatile uint32_t systime = 0U;

// memory pool in main memory (monotonic allocated)
static uint8_t memPool[MEM_POOL_SIZE];
PDOS_DTCM static uint32_t mainMemFreeIndex = 0U;
// local memory for task execution
PDOS_DTCM static uint8_t localMem[MAX_LOCAL_MEM_SIZE];
PDOS_DTCM static uint32_t localMemFreeIndex = 0U;

// circular buffer for log storage
static uint32_t logBuffer[MAX_LOG_NUM * 2] = {};
PDOS_DTCM static uint32_t localLogBuffer[MAX_LOG_NUM * 2] = {};
PDOS_DTCM static uint32_t logIndex = 0U;

// Add an entry to log.
void PdOS_add_log(uint8_t prio, PdOSLogEventType logEvent)
{
	/* 
	  log format:
		systime(32 bits) - 0(16 bits) - priority(8 bits) - event(8 bits) 
	*/
	localLogBuffer[logIndex * 2] = systime;
	localLogBuffer[logIndex * 2 + 1] = (uint32_t)((prio << 8) | ((uint8_t)logEvent));
	logIndex = (logIndex + 1 < MAX_LOG_NUM) ? (logIndex + 1) : 0;
}

// Create a new task and return its handle.
// Return NULL on failure.
PdOSTaskHandle PdOS_create_task(PdOSTaskFunction taskFunction, uint8_t prio, void *stkSto, uint32_t stkSize, uint32_t memSize)
{
	PdOSTaskHandle h;
	uint32_t *psp;

	// stack size needs to be at least 64 bytes to store one frame
	if ((stkSto == NULL) || (stkSize < 64U))
	{
		return NULL;
	}

	// requires an unique priority no greater than MAX_TASK_NUM
	if ((prio > MAX_TASK_NUM) || (tasks[prio] != NULL))
	{
		return NULL;
	}

	// ensure enough memory space
	if ((memSize > MAX_LOCAL_MEM_SIZE) || (mainMemFreeIndex + memSize > MEM_POOL_SIZE))
	{
		return NULL;
	}

	// stack top, aligned by 8 bytes
	psp = (uint32_t *)(((uint32_t)stkSto + stkSize) & ~7U);

	// hardware-stacked registers, corresponds to ARMv7-M
	*(--psp) = (1U << 24);  // xPSR, THUMB bit set
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
	h->currPhase = (uint8_t)PDOS_IDLE_PHASE;
	h->wkUpTime = 0U;

	// allocate space in main memory
	h->memSize = memSize;
	h->memStartIndex = mainMemFreeIndex;
	mainMemFreeIndex += memSize;
	
	tasks[prio] = h;

	if (prio > 0U)
	{
		// set the new task ready to run
		readySet |= (1U << (prio - 1U));
	}

	return h;
}

// Schedule the next task to run.
// Needs to be called in a critical section.
static void PdOS_sched(void)
{
	PdOSTaskHandle candidateTask;

	// on init
	if (currTask == NULL)
	{
		nextTask = (readySet == 0U) ? tasks[0] : tasks[GET_TASK_INDEX(readySet)];
		ICSR_REG |= PENDSVSET_BITMASK;
		return;
	}

	// memory phases cannot be preempted
	if ((currTask->currPhase == (uint8_t)PDOS_READ_PHASE) || (currTask->currPhase == (uint8_t)PDOS_WRITE_PHASE))
	{
		return;
	}

	if (readySet == 0U)
	{
		// set idle task to run
		nextTask = tasks[0];
	}
	else
	{
		candidateTask = tasks[GET_TASK_INDEX(readySet)];
		if (candidateTask == currTask)
		{
			return;
		}
		
		// CPU yield
		if (candidateTask->prio < currTask->prio)
		{
			
			nextTask = candidateTask;
			if ((nextTask != tasks[0]) && (nextTask->currPhase == (uint8_t)PDOS_EXECUTE_PHASE))
			{
				// add log: preempted task resumes execution
				PdOS_add_log(nextTask->prio, PDOS_ENTER_EXECUTE);
			}
		}
		// preemption
		else
		{
			// preemption allowed only when having enough space in core-local memory
			if (localMemFreeIndex + candidateTask->memSize < MAX_LOCAL_MEM_SIZE)
			{
				nextTask = candidateTask;
				if (currTask != tasks[0])
				{
					// add log: preempted task suspends execution
					PdOS_add_log(currTask->prio, PDOS_EXIT_EXECUTE);
				}
			}
		}
	}

	if (nextTask != currTask)
	{
		// trigger the PendSV interrupt
		ICSR_REG |= PENDSVSET_BITMASK;
	}
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

// Initialize OS.
PdOSErrCode PdOS_init(void)
{
	PdOSTaskHandle idleTaskHandle;

	// set PendSV priority to 0xFF
	SET_PENDSV_PRIO(0xFFU);
	// create idle task
	idleTaskHandle = PdOS_create_task(&PdOS_idle_task_func, 0U, idleTaskStack, sizeof(idleTaskStack), 0U);
	return (idleTaskHandle != NULL) ? PDOS_OK : PDOS_ERROR;
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
	if ((currTask == tasks[0]) || (ticks == 0U))
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
	if ((currTask == tasks[0]) || (period == 0U) || (prevWkUpTime == NULL))
	{
		return;
	}

	__disable_irq();

	nextWkUpTime = *prevWkUpTime + period;
	*prevWkUpTime = nextWkUpTime;

	// deadline already missed
	if ((int32_t)(systime - nextWkUpTime) >= 0)
	{
		__enable_irq();
		return;
	}
	
	PdOS_block_curr_task(nextWkUpTime);

	__enable_irq();
}

// Maintain systime and ready/blocked set
static void PdOS_tick(void)
{
	PdOSTaskHandle h;
	uint32_t bitmask;
	uint32_t tmpBlockedSet = blockedSet;

	systime++;
	
	while (tmpBlockedSet != 0U)
	{
		h = tasks[GET_TASK_INDEX(tmpBlockedSet)];
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
	
	currTask->currPhase = (uint8_t)PDOS_READ_PHASE;
	PdOS_add_log(currTask->prio, PDOS_ENTER_READ);

	currLocalMem = localMem + localMemFreeIndex;
	localMemFreeIndex += currTask->memSize;
	memcpy((void *)currLocalMem, (void *)&memPool[currTask->memStartIndex], currTask->memSize);
	PdOS_busy_wait(extraDelayTime);
	
	PdOS_add_log(currTask->prio, PDOS_EXIT_READ);

	currTask->currPhase = (uint8_t)PDOS_EXECUTE_PHASE;
	PdOS_add_log(currTask->prio, PDOS_ENTER_EXECUTE);

	return (void *)currLocalMem;
}

// Copy data from core-local memory back to main memory.
void PdOS_write(uint32_t extraDelayTime)
{
	uint8_t *currLocalMem;
	
	currTask->currPhase = (uint8_t)PDOS_WRITE_PHASE;
	PdOS_add_log(currTask->prio, PDOS_EXIT_EXECUTE);
	PdOS_add_log(currTask->prio, PDOS_ENTER_WRITE);

	localMemFreeIndex -= currTask->memSize;
	currLocalMem = localMem + localMemFreeIndex;
	memcpy((void *)&memPool[currTask->memStartIndex], (void *)currLocalMem, currTask->memSize);
	// also copy log to main memory in WRITE phase
	memcpy((void *)logBuffer, (void *)localLogBuffer, sizeof(localLogBuffer));
	PdOS_busy_wait(extraDelayTime);

	currTask->currPhase = (uint8_t)PDOS_IDLE_PHASE;
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

        /* save context */
        "  MRS           r0, psp             \n"  // read PSP
        "  STMDB         r0!, {r4-r11}       \n"  // store R4~R11 according to PSP

        "  STR           r0, [r1, #0]        \n"  // currTask->psp = PSP now

        "PendSV_restore:                     \n"
        /* restore context */
        "  LDR           r2, =nextTask       \n"
        "  LDR           r1, [r2, #0]        \n"  // r1 = nextTask
        "  LDR           r0, [r1, #0]        \n"  // r0 = nextTask->psp

        "  LDMIA         r0!, {r4-r11}       \n"  // restore R4~R11 according to PSP
        "  MSR           psp, r0             \n"  // update PSP

        /* currTask = nextTask */
        "  LDR           r2, =currTask       \n"
        "  STR           r1, [r2, #0]        \n"

        "  CPSIE         I                   \n"  // enable interrupt
        "  BX            lr                  \n"
    );
}
