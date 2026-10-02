/**
 * @file app_main.c
 * @brief DMA 链表应用的顶层流程、故障停机和模式切换入口。
 */
#include "app_main.h"
#include "app_config.h"
#include "bsp_button.h"
#include "dma_graph.h"
#include "mx_tim2.h"

volatile app_diagnostics_t g_app_diagnostics;

_Noreturn void app_fault(app_fault_t fault, uint32_t detail)
{
  g_app_diagnostics.ready = 0U;
  g_app_diagnostics.fault_detail = detail;
  g_app_diagnostics.fault = fault;
  __DSB();
  __disable_irq();
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

static void mode_button_event(void)
{
  /* EXTI13 只提交两个 SRAM link word，不停止或重建 DMA。 */
  app_mode_t mode = (g_app_diagnostics.desired_mode == APP_MODE_NORMAL)
                    ? APP_MODE_ALARM : APP_MODE_NORMAL;
  if (!dma_graph_request_mode(mode))
  {
    app_fault(APP_FAULT_ILLEGAL_RELINK, (uint32_t)mode);
  }
  g_app_diagnostics.desired_mode = mode;
  ++g_app_diagnostics.button_events;
}

_Noreturn void app_run(void)
{
  /* 确认 CubeMX2 生成的时钟和 PWM 参数仍符合 DMA 样本的时间基准。 */
  g_app_diagnostics.tim_kernel_hz = HAL_TIM_GetClockFreq(mx_tim2_gethandle());
  if ((g_app_diagnostics.tim_kernel_hz != APP_TIM_KERNEL_HZ)
      || (TIM2->PSC != APP_PWM_PRESCALER)
      || (TIM2->ARR != APP_PWM_PERIOD_COUNTS - 1U))
  {
    app_fault(APP_FAULT_PWM_CONFIGURATION, g_app_diagnostics.tim_kernel_hz);
  }

  dma_graph_build();
  dma_graph_start();
  app_check_status(bsp_button_start(mode_button_event), APP_FAULT_BUTTON_START);
  g_app_diagnostics.ready = 1U;
  HAL_SuspendTick();
  SCB->ICSR = SCB_ICSR_PENDSTCLR_Msk;

  for (;;)
  {
    __DSB();
    __WFI();
  }
}
