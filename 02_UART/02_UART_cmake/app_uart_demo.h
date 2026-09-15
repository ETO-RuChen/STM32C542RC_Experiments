/**
  ******************************************************************************
  * file           : app_uart_demo.h
  * brief          : UART communication demo interface.
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef APP_UART_DEMO_H
#define APP_UART_DEMO_H

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
  * brief:  初始化串口通信实验。
  * retval: SYSTEM_OK 表示初始化成功，否则表示初始化失败
  */
system_status_t App_UartDemo_Init(void);

/**
  * brief:  串口通信实验主循环任务，需要在 while(1) 中持续调用。
  * retval: None
  */
void App_UartDemo_Process(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* APP_UART_DEMO_H */
