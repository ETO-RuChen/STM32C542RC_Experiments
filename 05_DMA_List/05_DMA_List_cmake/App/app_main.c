#include "app_main.h"
#include "app_config.h"
#include "bsp_button.h"
#include "bsp_led.h"
#include "bsp_vcp.h"
#include "mx_tim2.h"
#include "bringup_dma.h"
#include "dma_graph.h"

volatile app_diagnostics_t g_app_diagnostics;

#if APP_PHASE == 1
typedef struct
{
  uint32_t duty_percent;
  const char *message;
  uint32_t message_size;
} bringup_step_t;

static const char banner[] =
  "[P1] Hardware bring-up: TIM2_CH1=1kHz, USART2=115200 8N1\r\n"
  "[P1] USER button selects fixed PWM: 10% -> 50% -> 100% -> 10%\r\n";
static const char duty_10[] = "[P1] PWM=10%, CCR1=100\r\n";
static const char duty_50[] = "[P1] PWM=50%, CCR1=500\r\n";
static const char duty_100[] = "[P1] PWM=100%, CCR1=1000\r\n";
static const bringup_step_t steps[] =
{
  {10U, duty_10, sizeof(duty_10) - 1U},
  {50U, duty_50, sizeof(duty_50) - 1U},
  {100U, duty_100, sizeof(duty_100) - 1U}
};
#endif

_Noreturn void app_fault(app_fault_t fault, uint32_t detail)
{
  g_app_diagnostics.ready = 0U;
  g_app_diagnostics.fault_detail = detail;
  g_app_diagnostics.fault = fault;
  __DSB();
  /* Preserve first-fault evidence even when UART or the HAL tick is unusable. */
  __disable_irq();
  /* Set a debugger breakpoint on app_fault when needed. An explicit BKPT can
     become a HardFault if the debugger detaches after the C_DEBUGEN check. */
  for (;;)
  {
    __WFI();
  }
}

_Noreturn void app_system_fault(uint32_t system_status)
{
  g_app_diagnostics.system_status = system_status;
  if (g_app_diagnostics.init_fault != APP_FAULT_NONE)
  {
    app_fault(g_app_diagnostics.init_fault, g_app_diagnostics.init_detail);
  }
  app_fault(APP_FAULT_SYSTEM_INIT, system_status);
}

void app_check_status(hal_status_t status, app_fault_t fault)
{
  if (status != HAL_OK)
  {
    app_fault(fault, (uint32_t)status);
  }
}

#if APP_PHASE == 1
static void write_message(const char *message, uint32_t size)
{
  app_check_status(bsp_vcp_write(message, size), APP_FAULT_UART_TX);
  ++g_app_diagnostics.uart_messages;
}

static void button_event(void)
{
  /* P1 ISR only records the event. Foreground performs the fixed-duty test. */
  ++g_app_diagnostics.button_events;
}
#endif

#if APP_PHASE >= 7
static void mode_button_event(void)
{
  app_mode_t mode = (g_app_diagnostics.desired_mode == APP_MODE_NORMAL)
                    ? APP_MODE_ALARM : APP_MODE_NORMAL;
  if (!dma_graph_request_mode(mode)) { app_fault(APP_FAULT_ILLEGAL_RELINK, (uint32_t)mode); }
  g_app_diagnostics.desired_mode = mode;
  ++g_app_diagnostics.button_events;
  ++g_app_diagnostics.processed_events;
}
#endif

_Noreturn void app_run(void)
{
  g_app_diagnostics.tim_kernel_hz = HAL_TIM_GetClockFreq(mx_tim2_gethandle());
  if ((g_app_diagnostics.tim_kernel_hz != APP_TIM_KERNEL_HZ)
      || (TIM2->PSC != APP_PWM_PRESCALER)
      || (TIM2->ARR != APP_PWM_PERIOD_COUNTS - 1U))
  {
    app_fault(APP_FAULT_PWM_CONFIGURATION, g_app_diagnostics.tim_kernel_hz);
  }

#if APP_PHASE == 1
  uint32_t step = 0U;
  app_check_status(bsp_led_set_duty(steps[step].duty_percent), APP_FAULT_PWM_SET_DUTY);
  app_check_status(bsp_led_start(), APP_FAULT_PWM_START);
  g_app_diagnostics.duty_percent = steps[step].duty_percent;
  write_message(banner, sizeof(banner) - 1U);
  write_message(steps[step].message, steps[step].message_size);
  app_check_status(bsp_button_start(button_event), APP_FAULT_BUTTON_START);
  g_app_diagnostics.ready = 1U;

  for (;;)
  {
    if (g_app_diagnostics.processed_events != g_app_diagnostics.button_events)
    {
      step = (step + 1U) % (sizeof(steps) / sizeof(steps[0]));
      app_check_status(bsp_led_set_duty(steps[step].duty_percent), APP_FAULT_PWM_SET_DUTY);
      g_app_diagnostics.duty_percent = steps[step].duty_percent;
      write_message(steps[step].message, steps[step].message_size);
      ++g_app_diagnostics.processed_events;
    }

    /* Avoid a check/sleep race if an EXTI arrives immediately before WFI.
       Keep SysTick in P1 for UART timeouts and the debounce timestamp. */
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    if (g_app_diagnostics.processed_events == g_app_diagnostics.button_events)
    {
      __DSB();
      __WFI();
    }
    __set_PRIMASK(primask);
  }
#elif APP_PHASE == 2
  bringup_direct_run();
#elif APP_PHASE == 3 || APP_PHASE == 4
  dma_graph_build();
#if APP_PHASE == 3
  static const char begin[] = "[P3] Timer linked list: UP -> DOWN\r\n";
#else
  static const char begin[] = "[P4] One LPDMA1_CH0: UART -> TIM -> UART -> TIM\r\n";
#endif
  app_check_status(bsp_vcp_write(begin, sizeof(begin) - 1U), APP_FAULT_UART_TX);
  dma_graph_start();
  g_app_diagnostics.ready = 1U;
  while (g_dma_graph.completions == 0U)
  {
    if (HAL_GetTick() - g_dma_graph.start_ms > 2500U)
    {
      app_fault(APP_FAULT_DMA_TIMEOUT, 2500U);
    }
    __WFI();
  }
  if ((g_dma_graph.elapsed_ms < 1998U) || (g_dma_graph.elapsed_ms > 2010U)
      || (TIM2->CCR1 != 0U) || (LPDMA1_CH0->CBR1 != 0U))
  {
    app_fault(APP_FAULT_DMA_VERIFY, g_dma_graph.elapsed_ms);
  }
#if APP_PHASE == 3
  static const char done[] = "[P3] PASS: two timer nodes completed autonomously\r\n";
#else
  static const char done[] = "[P4] PASS: four mixed nodes completed on one channel\r\n";
#endif
  app_check_status(bsp_vcp_write(done, sizeof(done) - 1U), APP_FAULT_UART_TX);
  for (;;) { __WFI(); }
#elif APP_PHASE >= 5
  dma_graph_build();
  dma_graph_start();
#if APP_PHASE >= 7
  app_check_status(bsp_button_start(mode_button_event), APP_FAULT_BUTTON_START);
#endif
  g_app_diagnostics.ready = 1U;
  HAL_SuspendTick();
  SCB->ICSR = SCB_ICSR_PENDSTCLR_Msk;
  for (;;)
  {
    __DSB();
    __WFI();
    ++g_app_diagnostics.sleep_wakeups;
  }
#else
#error Unsupported APP_PHASE
#endif
}
