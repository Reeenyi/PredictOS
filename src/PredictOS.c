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

extern uint32_t SystemCoreClock;

PdOSTaskHandle * volatile currTask = NULL;
PdOSTaskHandle * volatile nextTask = NULL;

static PdOSTaskHandle *tasks[MAX_TASK_NUM + 1U] = {};
static uint32_t readySet = 0U;
static uint32_t blockedSet = 0U;

PdOSTaskHandle idleTaskHandle;

void idle_task_func(void)
{
	while (1)
	{
		PdOS_on_idle();
	}
}

__attribute__((weak)) void PdOS_on_idle(void)
{
	// default weak function
}

void PdOS_tick(void);

// Callback function for systick. Used privately.
void _PdOS_systick_callback(systick_driver_t *sdp)
{
	(void)sdp;
	PdOS_tick();
	__disable_irq();
	PdOS_sched();
	__enable_irq();
}

// Initialize systick. Used privately.
void _PdOS_set_systick(void)
{
	systick_init(&DRV_SYSTICK);
	systick_set_prio(&DRV_SYSTICK, 0U);  // set systick to the highest priority
	systick_set_relval(&DRV_SYSTICK, (SystemCoreClock / TICKS_PER_SECOND));  // frequency set to 1000Hz
	systick_set_cb(&DRV_SYSTICK, _PdOS_systick_callback);
	systick_start(&DRV_SYSTICK);
}

PdOSErrCode PdOS_init(void *idleStkSto, uint32_t idleStkSize)
{
	// set PendSV priority to 0xFF
	SET_PENDSV_PRIO(0xFFU);
	// create the idle task
	return PdOS_create_task(&idleTaskHandle, &idle_task_func, 0U, idleStkSto, idleStkSize);
}

void PdOS_run()
{
	_PdOS_set_systick();

	// call the scheduler explicitly
	// to transfer control to the RTOS
	__disable_irq();	
	PdOS_sched();
	__enable_irq();

	// the following code should never execute
	while (1);
}

PdOSErrCode PdOS_create_task(PdOSTaskHandle *h, PdOSTaskFunction taskFunction, uint8_t prio, void *stkSto, uint32_t stkSize)
{
	// requires an unique priority no greater than MAX_TASK_NUM
	if ((prio > MAX_TASK_NUM) || (tasks[prio] != NULL))
	{
		return PDOS_INVALID_PARAM;
	}

	// stack top, aligned by 8 bytes
	uint32_t *psp = (uint32_t *)((((uint32_t)stkSto + stkSize) / 8) * 8);

	// stack frame, corresponds to ARMv7-M
	*(--psp) = (1U << 24);  // xPSR, THUMB bit set
	*(--psp) = (uint32_t)taskFunction;  // PC
	*(--psp) = 0x0000000EU;  // LR
	*(--psp) = 0x0000000CU;  // R12
	*(--psp) = 0x00000003U;  // R3
	*(--psp) = 0x00000002U;  // R2
	*(--psp) = 0x00000001U;  // R1
	*(--psp) = 0x00000000U;  // R0
	// additional registers
	*(--psp) = 0x0000000BU;  // R11
	*(--psp) = 0x0000000AU;  // R10
	*(--psp) = 0x00000009U;  // R9
	*(--psp) = 0x00000008U;  // R8
	*(--psp) = 0x00000007U;  // R7
	*(--psp) = 0x00000006U;  // R6
	*(--psp) = 0x00000005U;  // R5
	*(--psp) = 0x00000004U;  // R4

	h->psp = psp;
	tasks[prio] = h;
	h->prio = prio;

	if (prio > 0U)
	{
		// set the new task ready to run
		readySet |= (1U << (prio - 1U));
	}

	return PDOS_OK;
}

// Schedule the next task to run.
// needs to be called in a critical section
void PdOS_sched(void)
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

void PdOS_delay(uint32_t ticks)
{
	uint32_t bitmask;

	// idle task should not be blocked
	// ticks should not be zero, otherwise underflow will occur
	if ((currTask == tasks[0]) || (ticks == 0U))
	{
		return;
	}

	__disable_irq();
	
	bitmask = 1U << (currTask->prio - 1U);
	currTask->timeout = ticks;
	readySet &= ~bitmask;
	blockedSet |= bitmask;
	PdOS_sched();  // switch away from the current task

	__enable_irq();
}

void PdOS_tick(void)
{
	PdOSTaskHandle *currH;
	uint32_t bitmask;
	uint32_t tmpBlockedSet = blockedSet;
	while (tmpBlockedSet != 0U)
	{
		currH = tasks[LOG2(tmpBlockedSet)];
		bitmask = (1U << (currH->prio - 1U));
		currH->timeout--;
		if (currH->timeout == 0U)
		{
			readySet |= bitmask;
			blockedSet &= ~bitmask;
		}
		tmpBlockedSet &= ~bitmask;
	}
	// no need to call the scheduler explicitly,
	// as it is called at the end of every systick
}

// assembly function for context switch
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
