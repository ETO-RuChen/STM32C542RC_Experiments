#ifndef BSP_BUTTON_H
#define BSP_BUTTON_H

#include "stm32_hal.h"

typedef void (*bsp_button_callback_t)(void);

/** 按键原始边沿、消抖和最终事件计数，供 GDB 验证消抖行为。 */
typedef struct
{
  uint32_t raw_edges;         /**< 触发沿匹配后的原始 EXTI 次数。 */
  uint32_t accepted_events;   /**< 通过消抖并通知应用的次数。 */
  uint32_t rejected_edges;    /**< 落在消抖窗口内而丢弃的次数。 */
  uint32_t debounce_restarts; /**< 每次重新启动 TIM6 窗口的次数。 */
} bsp_button_diagnostics_t;

extern volatile bsp_button_diagnostics_t g_button_diagnostics;

/* 回调运行在 EXTI13 中断上下文，禁止阻塞、延时或执行 UART 发送。 */
hal_status_t bsp_button_start(bsp_button_callback_t callback);

#endif
