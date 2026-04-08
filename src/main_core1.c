/**
 * @file main_core1.c
 * @author Yi Ren
 * @brief Application code for core 1
 */

#include <test_env.h>
#include "PredictOS.h"

#define TASK1_MEM (100U)
#define TASK2_MEM (200U)
#define TASK3_MEM (100U)

PDOS_DTCM uint32_t task1Stack[128];
PDOS_DTCM uint32_t task2Stack[128];
PDOS_DTCM uint32_t task3Stack[128];

PdOSTaskHandle task1Handle;
PdOSTaskHandle task2Handle;
PdOSTaskHandle task3Handle;

PDOS_ITCM void task1_func(void)
{
	uint32_t i;
	uint32_t prevWkUpTime = PdOS_get_systime();
	uint8_t *localMem;
	uint8_t magicNum;
	uint8_t prevMagicNum = 66U;

	localMem = PdOS_read(0U);
	for (i = 0U; i < TASK1_MEM; i++)
	{
		localMem[i] = prevMagicNum;
	}
	PdOS_write(0U);

	while (1)
	{
		localMem = PdOS_read(2U);
		
		magicNum = prevMagicNum + 1U;
		for (i = 0U; i < TASK1_MEM; i++)
		{
			if (localMem[i] != prevMagicNum)
			{
				// data unexpected
				while (1);
			}
			localMem[i] = magicNum;
		}
		prevMagicNum = magicNum;
		PdOS_busy_wait(5U);
		
		PdOS_write(2U);

		PdOS_delayUntil(&prevWkUpTime, 30U);
	}
}

PDOS_ITCM void task2_func(void)
{
	uint32_t i;
	uint32_t prevWkUpTime = PdOS_get_systime();
	uint8_t *localMem;
	uint8_t magicNum;
	uint8_t prevMagicNum = 12U;

	localMem = PdOS_read(0U);
	for (i = 0U; i < TASK2_MEM; i++)
	{
		localMem[i] = prevMagicNum;
	}
	PdOS_write(0U);

	while (1)
	{
		localMem = PdOS_read(6U);

		magicNum = prevMagicNum + 2U;
		for (i = 0U; i < TASK2_MEM; i++)
		{
			if (localMem[i] != prevMagicNum)
			{
				// data unexpected
				while (1);
			}
			localMem[i] = magicNum;
		}
		prevMagicNum = magicNum;
		PdOS_busy_wait(10U);

		PdOS_write(5U);

		PdOS_delayUntil(&prevWkUpTime, 120U);
	}
}

PDOS_ITCM void task3_func(void)
{
	uint32_t i;
	uint32_t prevWkUpTime = PdOS_get_systime();
	uint8_t *localMem;
	uint8_t magicNum;
	uint8_t prevMagicNum = 25U;

	localMem = PdOS_read(0U);
	for (i = 0U; i < TASK3_MEM; i++)
	{
		localMem[i] = prevMagicNum;
	}
	PdOS_write(0U);

	while (1)
	{
		localMem = PdOS_read(8U);

		magicNum = prevMagicNum + 2U;
		for (i = 0U; i < TASK3_MEM; i++)
		{
			if (localMem[i] != prevMagicNum)
			{
				// data unexpected
				while (1);
			}
			localMem[i] = magicNum;
		}
		prevMagicNum = magicNum;
		PdOS_busy_wait(20U);

		PdOS_write(10U);

		PdOS_delayUntil(&prevWkUpTime, 200U);
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

	task1Handle = PdOS_create_task(&task1_func, 5U, task1Stack, sizeof(task1Stack), TASK1_MEM);
	task2Handle = PdOS_create_task(&task2_func, 3U, task2Stack, sizeof(task2Stack), TASK2_MEM);
	task3Handle = PdOS_create_task(&task3_func, 2U, task3Stack, sizeof(task3Stack), TASK3_MEM);

	PdOS_run();
}
