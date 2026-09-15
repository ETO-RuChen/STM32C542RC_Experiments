/**
  ******************************************************************************
  * file           : app_timer_toggle.h
  * brief          : TIM6 interrupt GPIO toggle demo interface.
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef APP_TIMER_TOGGLE_H
#define APP_TIMER_TOGGLE_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Includes ------------------------------------------------------------------*/
#include "mx_hal_def.h"

/* Exported types ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/
/* Exported macros -----------------------------------------------------------*/
/* Exported functions ------------------------------------------------------- */

/**
  * brief:  初始化 TIM6 定时中断翻转 PA5 实验。
  * retval: SYSTEM_OK 表示初始化成功，否则表示初始化失败
  */
system_status_t App_TimerToggle_Init(void);

/**
  * brief:  定时器翻转实验主循环任务，当前保留为空任务接口。
  * retval: None
  */
void App_TimerToggle_Process(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* APP_TIMER_TOGGLE_H */
