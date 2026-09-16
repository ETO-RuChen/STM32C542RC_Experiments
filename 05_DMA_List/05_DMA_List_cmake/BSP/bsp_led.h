#ifndef BSP_LED_H
#define BSP_LED_H

#include "stm32_hal.h"

/* P1 only: select a fixed duty, never a periodic software animation. */
hal_status_t bsp_led_set_duty(uint32_t percent);
hal_status_t bsp_led_start(void);

#endif
