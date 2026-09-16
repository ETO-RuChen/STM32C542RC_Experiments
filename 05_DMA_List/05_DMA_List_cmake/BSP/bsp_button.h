#ifndef BSP_BUTTON_H
#define BSP_BUTTON_H

#include "stm32_hal.h"

typedef void (*bsp_button_callback_t)(void);

typedef struct
{
  uint32_t raw_edges;
  uint32_t accepted_events;
  uint32_t rejected_edges;
  uint32_t last_edge_ms;
  uint32_t debounce_restarts;
} bsp_button_diagnostics_t;

extern volatile bsp_button_diagnostics_t g_button_diagnostics;

/* Callback runs in EXTI13; it must not block or perform UART transmission. */
hal_status_t bsp_button_start(bsp_button_callback_t callback);

#endif
