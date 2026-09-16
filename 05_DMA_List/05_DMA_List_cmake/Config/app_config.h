#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#define APP_UART_BAUDRATE          115200U
#define APP_UART_TIMEOUT_MS        100U
#define APP_TIM_KERNEL_HZ          144000000U
#define APP_PWM_PRESCALER          143U
#define APP_PWM_FREQUENCY_HZ        1000U
#define APP_PWM_PERIOD_COUNTS      1000U
#define APP_BUTTON_DEBOUNCE_MS      40U
#define APP_FADE_UP_MS              1000U
#define APP_FADE_DOWN_MS            1000U
#define APP_DARK_HOLD_MS            500U
#define APP_ALARM_HALF_PERIOD_MS    75U
#define APP_ALARM_FLASH_COUNT       4U

/* MB2213 board pack declares B1 active HIGH. */
#define APP_BUTTON_ACTIVE_HIGH     1U

_Static_assert(APP_TIM_KERNEL_HZ / (APP_PWM_PRESCALER + 1U)
               / APP_PWM_PERIOD_COUNTS == APP_PWM_FREQUENCY_HZ,
               "PWM clock constants are inconsistent");

#endif
