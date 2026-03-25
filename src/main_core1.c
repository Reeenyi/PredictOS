#include <test_env.h>
#include "PredictOS.h"

uint32_t idleTaskStack[32];
uint32_t blinky1Stack[32];
uint32_t blinky2Stack[32];

PdOSTaskHandle blinky1Handle;
PdOSTaskHandle blinky2Handle;

void blinky1_func(void)
{
	uint32_t i;
	uint32_t prevWkUpTime = PdOS_get_systime();

	while (1)
	{
		for (i = 0U; i < 1000U; i++)
		{
			gpio_toggle_pin(USER_LED_A);
		}
		PdOS_delayUntil(&prevWkUpTime, 1);
	}
}

void blinky2_func(void)
{
	uint32_t i;
	uint32_t prevWkUpTime = PdOS_get_systime();

	while (1)
	{
		for (i = 0U; i < 3000U; i++)
		{
			gpio_toggle_pin(USER_LED_B);
		}
		PdOS_delayUntil(&prevWkUpTime, 50);
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


	PdOS_init(idleTaskStack, sizeof(idleTaskStack));

	blinky1Handle = PdOS_create_task(&blinky1_func, 5U, blinky1Stack, sizeof(blinky1Stack));
	blinky2Handle = PdOS_create_task(&blinky2_func, 2U, blinky2Stack, sizeof(blinky2Stack));

	PdOS_run();
}
