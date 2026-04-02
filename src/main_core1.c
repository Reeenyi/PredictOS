/**
 * @file main_core1.c
 * @author Yi Ren
 * @brief Application code for core 1
 */

#include <test_env.h>
#include "PredictOS.h"

PDOS_DTCM uint32_t blinky1Stack[32];
PDOS_DTCM uint32_t blinky2Stack[32];

PdOSTaskHandle blinky1Handle;
PdOSTaskHandle blinky2Handle;

PDOS_ITCM void task1_func(void)
{
	uint32_t i;
	uint32_t prevWkUpTime = PdOS_get_systime();
	uint8_t *localMem = (uint8_t *)PdOS_get_local_mem_addr();

	while (1)
	{
		PdOS_add_log(PDOS_ENTER_READ);
		PdOS_read();
		PdOS_busy_wait(2);
		PdOS_add_log(PDOS_EXIT_READ);

		PdOS_add_log(PDOS_ENTER_EXECUTE);
		for (i = 0; i < 100U; i++)
		{
			localMem[i]++;
		}
		PdOS_busy_wait(5);
		PdOS_add_log(PDOS_EXIT_EXECUTE);

		PdOS_add_log(PDOS_ENTER_WRITE);
		PdOS_write();
		PdOS_busy_wait(3);
		PdOS_add_log(PDOS_EXIT_WRITE);

		PdOS_delayUntil(&prevWkUpTime, 50);
	}
}

PDOS_ITCM void task2_func(void)
{
	uint32_t i;
	uint32_t prevWkUpTime = PdOS_get_systime();
	uint8_t *localMem = (uint8_t *)PdOS_get_local_mem_addr();

	while (1)
	{
		PdOS_add_log(PDOS_ENTER_READ);
		PdOS_read();
		PdOS_busy_wait(5);
		PdOS_add_log(PDOS_EXIT_READ);

		PdOS_add_log(PDOS_ENTER_EXECUTE);
		PdOS_busy_wait(10);
		for (i = 0; i < 200U; i++)
		{
			localMem[i]++;
		}
		PdOS_add_log(PDOS_EXIT_EXECUTE);

		PdOS_add_log(PDOS_ENTER_WRITE);
		PdOS_write();
		PdOS_busy_wait(5);
		PdOS_add_log(PDOS_EXIT_WRITE);

		PdOS_delayUntil(&prevWkUpTime, 100);
	}
}

int main(void)
{
    test_env_init((TestInit_t)
                  (TEST_INIT_CLOCK    |
                   TEST_INIT_GPIO     |
                   TEST_INIT_BOARD    |
                   TEST_INIT_IRQ));

    /* Switch-off user leds.*/
	USER_LED_SWITCH_OFF(USER_LED_A);
	USER_LED_SWITCH_OFF(USER_LED_B);


	PdOS_init();

	blinky1Handle = PdOS_create_task(&task1_func, 5U, blinky1Stack, sizeof(blinky1Stack), 100U);
	blinky2Handle = PdOS_create_task(&task2_func, 2U, blinky2Stack, sizeof(blinky2Stack), 200U);

	PdOS_run();
}
