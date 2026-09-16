#ifndef APP_MAIN_H
#define APP_MAIN_H

#include <stdint.h>

typedef enum
{
  APP_FAULT_NONE = 0,
  APP_FAULT_SYSTEM_INIT,
  APP_FAULT_PWM_CONFIGURATION,
  APP_FAULT_PWM_SET_DUTY,
  APP_FAULT_PWM_START,
  APP_FAULT_UART_TX,
  APP_FAULT_BUTTON_START
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
} app_diagnostics_t;

extern volatile app_diagnostics_t g_app_diagnostics;

_Noreturn void app_system_fault(uint32_t system_status);
_Noreturn void app_run(void);

#endif
