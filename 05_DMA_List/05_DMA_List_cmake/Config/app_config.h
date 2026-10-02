#ifndef APP_CONFIG_H
#define APP_CONFIG_H

/* USART2 通过板载 ST-LINK VCP 输出；超时只用于前台阻塞式 bring-up。 */
#define APP_UART_BAUDRATE          115200U
#define APP_UART_TIMEOUT_MS        100U

/*
 * TIM2 内核时钟为 144 MHz。PSC=143 后计数时钟为 1 MHz，ARR=999
 * 因而产生 1 kHz PWM/更新请求；每个 DMA 样本恰好保持 1 ms。
 */
#define APP_TIM_KERNEL_HZ          144000000U
#define APP_PWM_PRESCALER          143U
#define APP_PWM_FREQUENCY_HZ        1000U
#define APP_PWM_PERIOD_COUNTS      1000U

/* 最终阶段关闭 SysTick，使用 TIM6 10 kHz 单脉冲窗口完成 40 ms 按键去抖。 */
#define APP_BUTTON_DEBOUNCE_MS      40U
#define APP_DEBOUNCE_TIMER_HZ       10000U

/* 正常模式波形：1 s 渐亮、1 s 渐暗、500 ms 全暗。 */
#define APP_FADE_UP_MS              1000U
#define APP_FADE_DOWN_MS            1000U
#define APP_DARK_HOLD_MS            500U

/* 报警模式波形：75 ms 亮/75 ms 灭，共闪烁 4 次。 */
#define APP_ALARM_HALF_PERIOD_MS    75U
#define APP_ALARM_FLASH_COUNT       4U

/* MB2213 板级包把 B1/PC13 定义为高电平有效。 */
#define APP_BUTTON_ACTIVE_HIGH     1U

/* 修改时钟或 PWM 常量后，在编译期阻止三者不一致的配置进入固件。 */
_Static_assert(APP_TIM_KERNEL_HZ / (APP_PWM_PRESCALER + 1U)
               / APP_PWM_PERIOD_COUNTS == APP_PWM_FREQUENCY_HZ,
               "PWM clock constants are inconsistent");

#endif
