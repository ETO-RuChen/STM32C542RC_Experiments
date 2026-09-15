/**
  ******************************************************************************
  * @file           : app_low_power_demo.c
  * @brief          : STOP1 低功耗按键唤醒实验。
  *
  * 实验现象：
  * 1. 上电后 PA5 闪烁，串口打印实验状态；
  * 2. 等待 3 秒后进入 STOP1 低功耗模式；
  * 3. PC13 产生 EXTI13 上升沿中断后唤醒芯片；
  * 4. 唤醒后恢复系统时钟，串口打印唤醒次数，PA5 快闪提示。
  *
  * 注意：
  * 当前 CubeMX2 配置里 PC13 的 EXTI 触发沿是 RISING，所以板载按键
  * 可能表现为“松开按键时唤醒”。如果希望按下瞬间唤醒，需要在 CubeMX2
  * 里把 PC13 的 EXTI 触发沿改成 FALLING，并同步修改本文件中的判断。
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "app_low_power_demo.h"

#include <stdio.h>

#include "mx_basic_stdio_app.h"
#include "mx_gpio_default.h"
#include "mx_pwr.h"
#include "mx_rcc.h"
#include "stm32_hal.h"

/* Private defines -----------------------------------------------------------*/
/* 系统仍在运行时的 LED 慢闪次数，用于提示 MCU 还没有进入 STOP1。 */
#define APP_LOW_POWER_RUN_BLINK_COUNT          (3U)

/* 系统运行提示闪烁间隔，数值越大闪烁越慢。 */
#define APP_LOW_POWER_RUN_BLINK_INTERVAL_MS    (200U)

/* 每轮实验进入 STOP1 前等待的时间，方便观察串口和 LED。 */
#define APP_LOW_POWER_ENTER_DELAY_MS           (3000U)

/* STOP1 被按键唤醒后的 LED 快闪次数，用于提示唤醒成功。 */
#define APP_LOW_POWER_WAKE_BLINK_COUNT         (6U)

/* 唤醒成功提示闪烁间隔，数值较小表示快闪。 */
#define APP_LOW_POWER_WAKE_BLINK_INTERVAL_MS   (80U)

/* 进入 STOP1 前给串口预留的发送时间，避免最后几个字符还没发完就停时钟。 */
#define APP_LOW_POWER_UART_DRAIN_DELAY_MS      (100U)

/* Private variables ---------------------------------------------------------*/
/* 保存 CubeMX2 生成的 EXTI13 句柄，用来确认中断来源确实是 PC13/EXTI13。 */
static hal_exti_handle_t *g_pExti13Handle = NULL;

/* 按键唤醒标志：在中断回调中置 1，在主循环打印后清 0。 */
static volatile uint8_t g_WakeupKeyPressed = 0U;

/* 唤醒计数：每进入一次 EXTI13 回调就累加，便于串口观察唤醒次数。 */
static volatile uint32_t g_WakeupCount = 0U;

/* Private function prototypes ----------------------------------------------*/
static void App_LowPowerDemo_Exti13Callback(hal_exti_handle_t *hexti, hal_exti_trigger_t trigger);
static void App_LowPowerDemo_BlinkLed(uint32_t blink_count, uint32_t interval_ms);
static void App_LowPowerDemo_EnterStop1(void);
static const char *App_LowPowerDemo_GetPreviousModeName(hal_pwr_system_mode_t previous_mode);

/* Exported functions --------------------------------------------------------*/
system_status_t App_LowPowerDemo_Init(void)
{
  /* 初始化 basic_stdio 后，printf 才会重定向到 CubeMX2 配置的 USART2。 */
  if (mx_basic_stdio_init() != SYSTEM_OK)
  {
    return SYSTEM_PERIPHERAL_ERROR;
  }

  /*
    获取 EXTI13 句柄。
    C5 新版 HAL 的 EXTI 回调是基于 hal_exti_handle_t 注册的，
    不是老版 STM32 HAL 常见的 HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)。
  */
  g_pExti13Handle = mx_gpio_default_exti13_gethandle();
  if (g_pExti13Handle == NULL)
  {
    return SYSTEM_PERIPHERAL_ERROR;
  }

  /*
    注册 EXTI13 触发回调。
    后续 PC13 产生 EXTI13 中断时，HAL_EXTI_IRQHandler() 会间接调用
    App_LowPowerDemo_Exti13Callback()。
  */
  if (HAL_EXTI_RegisterTriggerCallback(g_pExti13Handle, App_LowPowerDemo_Exti13Callback) != HAL_OK)
  {
    return SYSTEM_PERIPHERAL_ERROR;
  }

  /* 初始化软件状态，并清除上一次低功耗模式标志，避免旧标志影响本轮判断。 */
  g_WakeupKeyPressed = 0U;
  g_WakeupCount = 0U;
  HAL_PWR_CleanPreviousSystemPowerMode();

  (void)printf("\r\n[LOW_POWER] STOP1 wakeup demo start.\r\n");
  (void)printf("[LOW_POWER] PC13 EXTI13 rising edge wakes MCU, PA5 shows status.\r\n");

  return SYSTEM_OK;
}

void App_LowPowerDemo_Process(void)
{
  /*
    进入 STOP1 前先慢闪 PA5。
    看到这段闪烁，说明芯片当前还处于正常运行状态。
  */
  App_LowPowerDemo_BlinkLed(APP_LOW_POWER_RUN_BLINK_COUNT, APP_LOW_POWER_RUN_BLINK_INTERVAL_MS);

  (void)printf("[LOW_POWER] Enter STOP1 after %lu ms...\r\n", (unsigned long)APP_LOW_POWER_ENTER_DELAY_MS);

  /*
    留出 3 秒观察时间。
    这段时间内可以在串口助手看到即将进入 STOP1 的提示。
  */
  HAL_Delay(APP_LOW_POWER_ENTER_DELAY_MS);

  /* 真正执行 STOP1 进入流程，函数会停在 WFI，直到 PC13 中断唤醒。 */
  App_LowPowerDemo_EnterStop1();

  /*
    STOP1 唤醒后需要恢复系统时钟。
    进入 STOP 模式会关闭/切换部分时钟源，唤醒后如果不重新配置 RCC，
    后续 USART、HAL_Delay、系统主频相关功能都可能不准确。
  */
  if (mx_rcc_init() != SYSTEM_OK)
  {
    (void)printf("[LOW_POWER] RCC restore failed after STOP1.\r\n");
    return;
  }

  {
    /*
      读取并清除上一次系统低功耗模式标志。
      正常情况下这里应打印 STOP1；如果显示 RUN/UNKNOWN，说明没有检测到
      STOP 标志，可能是刚进入 STOP1 就被其他中断唤醒，或低功耗标志被提前清除。
    */
    hal_pwr_system_mode_t previous_mode = HAL_PWR_GetPreviousSystemPowerMode();
    HAL_PWR_CleanPreviousSystemPowerMode();

    (void)printf("[LOW_POWER] Wakeup from %s, count=%lu, key_flag=%u.\r\n",
                 App_LowPowerDemo_GetPreviousModeName(previous_mode),
                 (unsigned long)g_WakeupCount,
                 (unsigned int)g_WakeupKeyPressed);
  }

  /*
    主循环已经处理完本次按键事件，清除软件标志。
    唤醒次数 g_WakeupCount 不清零，用于持续观察累计唤醒次数。
  */
  g_WakeupKeyPressed = 0U;

  /* 快闪 PA5，提示本轮 STOP1 唤醒流程已经完成。 */
  App_LowPowerDemo_BlinkLed(APP_LOW_POWER_WAKE_BLINK_COUNT, APP_LOW_POWER_WAKE_BLINK_INTERVAL_MS);
}

/* Private functions ---------------------------------------------------------*/
static void App_LowPowerDemo_Exti13Callback(hal_exti_handle_t *hexti, hal_exti_trigger_t trigger)
{
  /*
    只响应本实验使用的 EXTI13 句柄和 RISING 触发沿。
    这样可以避免以后工程里增加其他 EXTI 中断时误判为 PC13 唤醒。
  */
  if ((hexti == g_pExti13Handle) && (trigger == HAL_EXTI_TRIGGER_RISING))
  {
    /*
      中断回调里只记录事件。
      不在中断里 printf、HAL_Delay 或做复杂逻辑，是为了让中断尽快返回，
      也避免串口/时钟状态刚从 STOP1 恢复时出现不可预期问题。
    */
    g_WakeupKeyPressed = 1U;
    g_WakeupCount++;
  }
}

static void App_LowPowerDemo_BlinkLed(uint32_t blink_count, uint32_t interval_ms)
{
  uint32_t blink_index;

  /*
    每次循环包含一次亮和一次灭。
    使用 HAL_GPIO_TogglePin() 可以不关心当前 LED 初始电平，
    但最后会再翻转回原状态，避免影响后续低功耗前的 LED 状态设置。
  */
  for (blink_index = 0U; blink_index < blink_count; blink_index++)
  {
    HAL_GPIO_TogglePin(PA5_PORT, PA5_PIN);
    HAL_Delay(interval_ms);
    HAL_GPIO_TogglePin(PA5_PORT, PA5_PIN);
    HAL_Delay(interval_ms);
  }
}

static void App_LowPowerDemo_EnterStop1(void)
{
  /*
    进入 STOP1 前先关闭 PA5。
    这样板上现象更清楚：LED 不再闪烁时，基本可以认为程序已经准备休眠。
  */
  HAL_GPIO_WritePin(PA5_PORT, PA5_PIN, PA5_INACTIVE_STATE);

  /* 清除本轮进入 STOP1 前的软件按键标志，等待新的 PC13 中断重新置位。 */
  g_WakeupKeyPressed = 0U;

  /* 给串口一点发送时间，避免刚打印完就停时钟导致末尾字符丢失。 */
  HAL_Delay(APP_LOW_POWER_UART_DRAIN_DELAY_MS);

  /*
    进入低功耗前清除历史低功耗标志。
    这样唤醒后 HAL_PWR_GetPreviousSystemPowerMode() 读到的就是本轮 STOP1
    产生的新标志，而不是上一次实验遗留的旧状态。
  */
  HAL_PWR_CleanPreviousSystemPowerMode();

  /*
    暂停 SysTick。
    HAL 默认 SysTick 每 1 ms 产生一次中断，如果不暂停，MCU 可能刚执行 WFI
    进入 STOP1，就马上被 SysTick 中断唤醒，导致“看起来没有睡下去”。
  */
  HAL_SuspendTick();

  /*
    进入 STOP1。
    HAL_PWR_LOW_PWR_MODE_WFI 表示使用 WFI(Wait For Interrupt) 方式等待中断。
    当前实验的唤醒源是 PC13 -> EXTI13 中断。
    这句函数会阻塞在内部的 WFI 指令，直到有中断把 CPU 唤醒。
  */
  HAL_PWR_EnterStopMode(HAL_PWR_LOW_PWR_MODE_WFI, HAL_PWR_STOP1_MODE);

  /*
    程序能运行到这里，说明 CPU 已经从 STOP1 被某个中断唤醒。
    先恢复 SysTick，让 HAL_Delay()、HAL_GetTick() 等时间函数恢复工作。
  */
  HAL_ResumeTick();
}

static const char *App_LowPowerDemo_GetPreviousModeName(hal_pwr_system_mode_t previous_mode)
{
  const char *p_mode_name;

  /*
    把 HAL 返回的枚举值转换成便于串口打印的字符串。
    这只是调试辅助函数，不影响低功耗进入和唤醒本身。
  */
  switch (previous_mode)
  {
    case HAL_PWR_SYSTEM_STOP0_MODE:
      p_mode_name = "STOP0";
      break;

    case HAL_PWR_SYSTEM_STOP1_MODE:
      p_mode_name = "STOP1";
      break;

    case HAL_PWR_SYSTEM_STANDBY_MODE:
      p_mode_name = "STANDBY";
      break;

    case HAL_PWR_SYSTEM_RUN_MODE:
    default:
      p_mode_name = "RUN/UNKNOWN";
      break;
  }

  return p_mode_name;
}
