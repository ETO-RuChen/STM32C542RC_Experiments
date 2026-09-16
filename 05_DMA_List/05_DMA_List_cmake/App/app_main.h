#ifndef APP_MAIN_H
#define APP_MAIN_H

#include <stdint.h>
#include "stm32_hal.h"

typedef enum
{
  APP_FAULT_NONE = 0,
  APP_FAULT_SYSTEM_INIT,
  APP_FAULT_PWM_CONFIGURATION,
  APP_FAULT_PWM_SET_DUTY,
  APP_FAULT_PWM_START,
  APP_FAULT_UART_TX,
  APP_FAULT_BUTTON_START,
  APP_FAULT_DMA_CONFIG,
  APP_FAULT_DMA_START,
  APP_FAULT_DMA_RUNTIME,
  APP_FAULT_DMA_TIMEOUT,
  APP_FAULT_DMA_VERIFY,
  APP_FAULT_UART_RUNTIME,
  APP_FAULT_NODE_MEMORY,
  APP_FAULT_NODE_BUILD,
  APP_FAULT_QUEUE_BUILD,
  APP_FAULT_WAVEFORM
} app_fault_t;

typedef struct
{
  app_fault_t fault;
  uint32_t fault_detail;
  uint32_t system_status;
  uint32_t tim_kernel_hz;
  uint32_t button_events;
  uint32_t processed_events;
  uint32_t duty_percent;
  uint32_t uart_messages;
  uint32_t ready;
  uint32_t sleep_wakeups;
} app_diagnostics_t;

extern volatile app_diagnostics_t g_app_diagnostics;

_Noreturn void app_system_fault(uint32_t system_status);
_Noreturn void app_fault(app_fault_t fault, uint32_t detail);
void app_check_status(hal_status_t status, app_fault_t fault);
_Noreturn void app_run(void);

#endif
