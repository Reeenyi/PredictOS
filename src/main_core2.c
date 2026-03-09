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

int main(void)
{
    /* Enable interrupts.*/
    osal_sys_unlock();

    test_env_init((TestInit_t)
                  (TEST_INIT_IRQ      |
                   TEST_INIT_OSAL));

    /* Application main loop */
    for ( ; ; ) {
        /* Blink USER_LED_B.*/
        gpio_toggle_pin(USER_LED_B);
        osal_delay_millisec(500U);
    }
}
