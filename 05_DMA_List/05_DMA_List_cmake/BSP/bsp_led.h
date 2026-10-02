#ifndef BSP_LED_H
#define BSP_LED_H

#include "stm32_hal.h"

/** P1 使用：设置 0～100% 固定占空比，不做软件周期动画。 */
hal_status_t bsp_led_set_duty(uint32_t percent);
/** 启动 TIM2_CH1 PWM 输出和 TIM2 计数器。 */
hal_status_t bsp_led_start(void);

#endif
