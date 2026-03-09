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

uint32_t blinky1_stack[40];
PdOSTaskHandle blinky1_handle;
void blinky1_func(void)
{
	int i;
	while (1)
	{
		gpio_toggle_pin(USER_LED_A);
		for (i = 0; i < 1e6; i++);
	}
}

uint32_t blinky2_stack[40];
PdOSTaskHandle blinky2_handle;
void blinky2_func(void)
{
	int i;
	while (1)
	{
		gpio_toggle_pin(USER_LED_B);
		for (i = 0; i < 1e6; i++);
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

	PdOS_create_task(&blinky1_handle, &blinky1_func, blinky1_stack, sizeof(blinky1_stack));
	PdOS_create_task(&blinky2_handle, &blinky2_func, blinky2_stack, sizeof(blinky2_stack));

	PdOS_run();


    irq_enable_all();

    while (1);
}
