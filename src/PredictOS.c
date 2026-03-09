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

extern uint32_t SystemCoreClock;

PdOSTaskHandle * volatile currTask = NULL;
PdOSTaskHandle * volatile nextTask = NULL;

// Callback function for systick. Used privately.
void __PdOS_systick_callback(systick_driver_t *sdp)
{
	(void)sdp;
	__disable_irq();
	PdOS_sched();
	__enable_irq();
}

// Initialize systick. Used privately.
void __PdOS_set_systick(void)
{
	systick_init(&DRV_SYSTICK);
	systick_set_prio(&DRV_SYSTICK, 0U);  // set systick to the highest priority
	systick_set_relval(&DRV_SYSTICK, (SystemCoreClock / 1000U));  // frequency set to 1000Hz
	systick_set_cb(&DRV_SYSTICK, __PdOS_systick_callback);
	systick_start(&DRV_SYSTICK);
}

void PdOS_init(void)
{
	// set PendSV priority to 0xFF
	SET_PENDSV_PRIO(0xFFU);
}

void PdOS_run()
{
	__PdOS_set_systick();
}

void PdOS_create_task(PdOSTaskHandle *h, PdOSTaskFunction taskFunction, void *stkSto, uint32_t stkSize)
{
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
}

//
// needs to be called in a critical section
void PdOS_sched(void)
{
	if (nextTask != currTask)
	{
		// trigger the PendSV interrupt
		ICSR_REG |= PENDSVSET_BITMASK;
	}
}

// assembly function for context switch
__attribute__ ((naked)) void PendSV_Handler(void)
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


