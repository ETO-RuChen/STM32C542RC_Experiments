/**
  ******************************************************************************
  * file           : app_timer_toggle.c
  * brief          : TIM6 update interrupt toggles PA5 output.
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "app_timer_toggle.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
static hal_tim_handle_t *g_pTim6Handle;
static volatile uint32_t g_Tim6UpdateCount;

/* Private functions prototype -----------------------------------------------*/

/* Exported functions --------------------------------------------------------*/

/**
  * brief:  初始化 TIM6 中断翻转实验，只启动定时器中断，不重新配置外设。
  * retval: system_status_t
  */
system_status_t App_TimerToggle_Init(void)
{
  g_pTim6Handle = mx_tim6_gethandle();
  if (g_pTim6Handle == NULL)
  {
    return SYSTEM_PERIPHERAL_ERROR;
  }

  g_Tim6UpdateCount = 0U;

  /* TIM6 已由 CubeMX2 配成 2 Hz 更新事件；启动中断后每 0.5s 进入一次回调。 */
  if (HAL_TIM_Start_IT(g_pTim6Handle) != HAL_OK)
  {
    return SYSTEM_PERIPHERAL_ERROR;
  }

  return SYSTEM_OK;
}

/**
  * brief:  主循环保留任务接口，方便后续加入状态监视或低功耗处理。
  * retval: None
  */
void App_TimerToggle_Process(void)
{
}

/**
  * brief:  TIM 更新事件回调；只响应 TIM6，并在中断中翻转 PA5。
  * retval: None
  */
void HAL_TIM_UpdateCallback(hal_tim_handle_t *htim)
{
  if (htim == g_pTim6Handle)
  {
    g_Tim6UpdateCount++;
    HAL_GPIO_TogglePin(PA5_PORT, PA5_PIN);
  }
}
