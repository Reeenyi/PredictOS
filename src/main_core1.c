/**
 * @file main_core1.c
 * @author Yi Ren
 * @brief Application code for core 1
 */

#include <test_env.h>
#include "PredictOS.h"

PDOS_DTCM uint32_t task1Stack[32];
PDOS_DTCM uint32_t task2Stack[32];

PdOSTaskHandle task1Handle;
PdOSTaskHandle task2Handle;

PDOS_ITCM void task1_func(void)
{
	uint32_t i;
	uint32_t prevWkUpTime = PdOS_get_systime();
	uint8_t *localMem;
	uint8_t magicNum;
	uint8_t prevMagicNum = 66;

	localMem = PdOS_read(0);
	for (i = 0; i < 100U; i++)
	{
		localMem[i] = prevMagicNum;
	}
	PdOS_write(0);

	while (1)
	{
		localMem = PdOS_read(2);
		
		magicNum = prevMagicNum + 1;
		for (i = 0; i < 100U; i++)
		{
			if (localMem[i] != prevMagicNum)
			{
				// data unexpected
				while (1);
			}
			localMem[i] = magicNum;
		}
		prevMagicNum = magicNum;
		PdOS_busy_wait(5);
		
		PdOS_write(2);

		PdOS_delayUntil(&prevWkUpTime, 30);
	}
}

PDOS_ITCM void task2_func(void)
{
	uint32_t i;
	uint32_t prevWkUpTime = PdOS_get_systime();
	uint8_t *localMem;
	uint8_t magicNum;
	uint8_t prevMagicNum = 12;

	localMem = PdOS_read(0);
	for (i = 0; i < 200U; i++)
	{
		localMem[i] = prevMagicNum;
	}
	PdOS_write(0);

	while (1)
	{
		localMem = PdOS_read(5);

		magicNum = prevMagicNum + 2;
		for (i = 0; i < 200U; i++)
		{
			if (localMem[i] != prevMagicNum)
			{
				// data unexpected
				while (1);
			}
			localMem[i] = magicNum;
		}
		prevMagicNum = magicNum;
		PdOS_busy_wait(45);

		PdOS_write(10);

		PdOS_delayUntil(&prevWkUpTime, 160);
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

	task1Handle = PdOS_create_task(&task1_func, 5U, task1Stack, sizeof(task1Stack), 100U);
	task2Handle = PdOS_create_task(&task2_func, 2U, task2Stack, sizeof(task2Stack), 200U);

	PdOS_run();
}
