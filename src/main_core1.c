#include <test_env.h>
#include "PredictOS.h"

uint32_t idleTaskStack[40];
uint32_t blinky1Stack[40];
uint32_t blinky2Stack[40];

PdOSTaskHandle blinky1Handle;
PdOSTaskHandle blinky2Handle;

void blinky1_func(void)
{
	while (1)
	{
		gpio_toggle_pin(USER_LED_A);
		PdOS_delay(500);
	}
}

void blinky2_func(void)
{
	while (1)
	{
		gpio_toggle_pin(USER_LED_B);
		PdOS_delay(300);
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

	PdOS_create_task(&blinky1Handle, &blinky1_func, blinky1Stack, sizeof(blinky1Stack));
	PdOS_create_task(&blinky2Handle, &blinky2_func, blinky2Stack, sizeof(blinky2Stack));

	PdOS_run();
}
