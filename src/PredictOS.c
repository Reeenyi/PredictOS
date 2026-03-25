/*
 * PredictOS.c
 */

#include <test_env.h>
#include "systick.h"
#include "PredictOS.h"

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

#define LOG2(x) (32U - __builtin_clz(x))

#if MAX_TASK_NUM > 32U
#error "MAX_TASK_NUM cannot exceed 32"
#endif

// Task Control Block
struct PdOSTaskControlBlock
{
	void *psp;            // stack pointer
	uint32_t wkUpTime;    // next wake up time
	uint8_t prio;         // priority
	uint8_t preserve[3];  // align by 4 bytes
};

extern uint32_t SystemCoreClock;

static struct PdOSTaskControlBlock tcbPool[MAX_TASK_NUM + 1U] = {};
static PdOSTaskHandle tasks[MAX_TASK_NUM + 1U] = {};
static volatile PdOSTaskHandle currTask = NULL;
static volatile PdOSTaskHandle nextTask = NULL;
static uint32_t readySet = 0U;
static uint32_t blockedSet = 0U;
static volatile uint32_t systime = 0U;

// Default weak function on idle.
__attribute__((weak)) void PdOS_on_idle(void)
{
}

// Idle task function.
static void PdOS_idle_task_func(void)
{
	while (1)
	{
		PdOS_on_idle();
	}
}

// Initialize OS.
PdOSErrCode PdOS_init(void *idleStkSto, uint32_t idleStkSize)
{
	PdOSTaskHandle idleTaskHandle;

	// set PendSV priority to 0xFF
	SET_PENDSV_PRIO(0xFFU);
	// create the idle task
	idleTaskHandle = PdOS_create_task(&PdOS_idle_task_func, 0U, idleStkSto, idleStkSize);
	return (idleTaskHandle != NULL) ? PDOS_OK : PDOS_ERROR;
}

// Create a new task and return its handle.
// Return NULL on failure.
PdOSTaskHandle PdOS_create_task(PdOSTaskFunction taskFunction, uint8_t prio, void *stkSto, uint32_t stkSize)
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
	h->wkUpTime = 0U;
	
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
	if (readySet == 0U)
	{
		nextTask = tasks[0];  // idle task
	}
	else
	{
		nextTask = tasks[LOG2(readySet)];
	}

	if (nextTask != currTask)
	{
		// trigger the PendSV interrupt
		ICSR_REG |= PENDSVSET_BITMASK;
	}
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

// Delay for a relative time.
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

// Delay until a specific absolute time.
void PdOS_delayUntil(uint32_t *prevWkUpTime, uint32_t period)
{
	uint32_t nextWkUpTime;

	// handle illegal cases
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
		h = tasks[LOG2(tmpBlockedSet)];
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
	__disable_irq();
	PdOS_sched();
	__enable_irq();
}

// Initialize systick.
static void PdOS_set_systick(void)
{
	systick_init(&DRV_SYSTICK);
	systick_set_prio(&DRV_SYSTICK, 0U);  // set systick to the highest priority
	systick_set_relval(&DRV_SYSTICK, (SystemCoreClock / TICKS_PER_SECOND));  // frequency set to 1000Hz
	systick_set_cb(&DRV_SYSTICK, PdOS_systick_callback);
	systick_start(&DRV_SYSTICK);
}

// Set OS to run.
void PdOS_run()
{
	PdOS_set_systick();

	// call the scheduler explicitly
	// to transfer control to the RTOS
	__disable_irq();	
	PdOS_sched();
	__enable_irq();

	// the following code should never execute
	while (1);
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
