/**
  ******************************************************************************
  * @file           : app_low_power_demo.h
  * @brief          : STOP1 低功耗按键唤醒实验接口。
  *
  * 本模块把低功耗实验相关代码从 main.c 中拆出来，main.c 只负责系统初始化
  * 和周期性调用 App_LowPowerDemo_Process()。这样可以清楚地区分：
  * 1. CubeMX2 生成的底层初始化代码；
  * 2. 用户自己编写的实验应用逻辑。
  ******************************************************************************
  */

#ifndef APP_LOW_POWER_DEMO_H
#define APP_LOW_POWER_DEMO_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Includes ------------------------------------------------------------------*/
#include "mx_def.h"

/* Exported functions ------------------------------------------------------- */
/**
  * @brief  初始化 STOP1 低功耗演示模块。
  *
  * 主要完成三件事：
  * 1. 初始化 basic_stdio，让 printf 可以通过 USART2 输出；
  * 2. 获取 CubeMX2 生成的 EXTI13 句柄；
  * 3. 给 EXTI13 注册按键中断回调函数。
  *
  * @retval SYSTEM_OK 初始化成功；其他值表示初始化失败。
  */
system_status_t App_LowPowerDemo_Init(void);

/**
  * @brief  STOP1 低功耗演示主循环任务。
  *
  * 该函数会反复执行：
  * 运行提示闪烁 -> 延时等待 -> 进入 STOP1 -> PC13 唤醒 ->
  * 恢复系统时钟 -> 唤醒提示闪烁。
  */
void App_LowPowerDemo_Process(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* APP_LOW_POWER_DEMO_H */
