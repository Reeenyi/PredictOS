/****************************************************************************
 *
 * Copyright (c) 2022 STMicroelectronics - All Rights Reserved
 *
 * License terms: STMicroelectronics Proprietary in accordance with licensing
 * terms SLA0098 at www.st.com.
 *
 * THIS SOFTWARE IS DISTRIBUTED "AS IS," AND ALL WARRANTIES ARE DISCLAIMED,
 * INCLUDING MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 *****************************************************************************/

#include <test_env.h>
#include "PredictOS.h"

uint32_t blinky1Stack[40];
PdOSTaskHandle blinky1Handle;
void blinky1_func(void)
{
	int i;
	while (1)
	{
		gpio_toggle_pin(USER_LED_A);
		for (i = 0; i < 5e5; i++);
	}
}

uint32_t blinky2Stack[40];
PdOSTaskHandle blinky2Handle;
void blinky2_func(void)
{
	int i;
	while (1)
	{
		gpio_toggle_pin(USER_LED_B);
		for (i = 0; i < 3e5; i++);
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

	PdOS_create_task(&blinky1Handle, &blinky1_func, blinky1Stack, sizeof(blinky1Stack));
	PdOS_create_task(&blinky2Handle, &blinky2_func, blinky2Stack, sizeof(blinky2Stack));

	PdOS_run();
}
