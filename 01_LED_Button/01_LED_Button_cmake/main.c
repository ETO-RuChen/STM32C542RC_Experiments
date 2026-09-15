/**
  ******************************************************************************
  * file           : main.c
  * brief          : 主程序入口。
  *                  完成系统初始化后进入应用主循环。
  ******************************************************************************
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* 头文件 --------------------------------------------------------------------*/
#include "main.h"

#include "mx_led.h"
#include "mx_button.h"
/* 私有类型定义 --------------------------------------------------------------*/
/* 私有宏定义 ----------------------------------------------------------------*/
#define APP_TASK_PERIOD_MS       10u
#define BUTTON_DEBOUNCE_TIME_MS  30u
#define LED_BLINK_NORMAL_MS      500u
#define LED_BLINK_PRESSED_MS     100u
/* 私有宏 --------------------------------------------------------------------*/
/* 私有变量 ------------------------------------------------------------------*/
/* 私有函数声明 --------------------------------------------------------------*/
static void app_delay_ms(uint32_t delay_ms);
static button_state_t app_button_get_debounced_state(button_t *button);
static void app_led_update(button_state_t button_state);

/**
  * @brief  应用程序入口。
  * @retval 按 C99 标准返回 int，正常运行时不会退出。
  */
int main(void)
{
  /** 系统初始化：
    * 该函数由 CubeMX2 生成，负责 HAL、时钟和 GPIO 等基础外设初始化。
    */
  if (mx_system_init() != SYSTEM_OK)
  {
    return (-1);
  }
  else
  {
    button_t *button = mx_button_0_getobject();

    if (button_init(button, BUTTON_0) != BUTTON_OK)
    {
      return (-1);
    }

    led_off(LED_0);

    while (1)
    {
      const button_state_t button_state = app_button_get_debounced_state(button);

      app_led_update(button_state);
      app_delay_ms(APP_TASK_PERIOD_MS);
    }
  }
} /* end main */

/**
  * @brief 应用层毫秒延时封装。
  * @param delay_ms 延时时间，单位 ms。
  */
static void app_delay_ms(uint32_t delay_ms)
{
  HAL_Delay(delay_ms);
}

/**
  * @brief 通过生成的 Button part driver 读取 BUTTON_0，并做软件去抖。
  * @param button 由 mx_button_0_getobject() 获取的按键对象。
  * @retval 去抖后的稳定按键状态。
  */
static button_state_t app_button_get_debounced_state(button_t *button)
{
  static button_state_t stable_state = BUTTON_UNPRESSED;
  static button_state_t last_sample = BUTTON_UNPRESSED;
  static uint32_t stable_time_ms = 0u;

  const button_state_t sample = button_get_state(button);

  if (sample != last_sample)
  {
    last_sample = sample;
    stable_time_ms = 0u;
  }
  else if (stable_time_ms < BUTTON_DEBOUNCE_TIME_MS)
  {
    stable_time_ms += APP_TASK_PERIOD_MS;
  }
  else
  {
    stable_state = sample;
  }

  return stable_state;
}

/**
  * @brief 通过生成的 LED part driver 控制 LED_0 闪烁。
  * @param button_state 去抖后的按键状态，用于选择 LED 闪烁速度。
  */
static void app_led_update(button_state_t button_state)
{
  static uint32_t blink_elapsed_ms = 0u;
  const uint32_t blink_period_ms =
    (button_state == BUTTON_PRESSED) ? LED_BLINK_PRESSED_MS : LED_BLINK_NORMAL_MS;

  blink_elapsed_ms += APP_TASK_PERIOD_MS;

  if (blink_elapsed_ms >= blink_period_ms)
  {
    blink_elapsed_ms = 0u;
    led_toggle(LED_0);
  }
}
