#ifndef APP_MAIN_H
#define APP_MAIN_H

#include <stdint.h>
#include "stm32_hal.h"
#include "app_mode.h"

/**
 * @brief 应用故障分类。
 *
 * 固件没有可依赖的故障串口路径；发生错误后把类别和 detail 保存到
 * g_app_diagnostics，再关中断并停在 WFI，供调试器读取第一现场。
 */
typedef enum
{
  APP_FAULT_NONE = 0,          /**< 尚未发生故障。 */
  APP_FAULT_SYSTEM_INIT,       /**< mx_system_init() 失败。 */
  APP_FAULT_PWM_CONFIGURATION, /**< 实际 TIM2 时钟/PSC/ARR 与设计常量不一致。 */
  APP_FAULT_PWM_SET_DUTY,      /**< P1 固定占空比设置失败。 */
  APP_FAULT_PWM_START,         /**< TIM2_CH1 输出或计数器启动失败。 */
  APP_FAULT_UART_TX,           /**< 前台或 DMA 串口发送启动/等待失败。 */
  APP_FAULT_BUTTON_START,      /**< EXTI13 回调、触发沿或去抖定时器配置失败。 */
  APP_FAULT_DMA_CONFIG,        /**< DMA 传输/回调/执行模式配置失败。 */
  APP_FAULT_DMA_START,         /**< Direct 或 Linked-list DMA 启动失败。 */
  APP_FAULT_DMA_RUNTIME,       /**< DMA 运行时报告 DTE/ULE/USE 等错误。 */
  APP_FAULT_DMA_TIMEOUT,       /**< P2～P4 限时实验未按期完成。 */
  APP_FAULT_DMA_VERIFY,        /**< 完成时间、剩余字节或最终 CCR 校验失败。 */
  APP_FAULT_UART_RUNTIME,      /**< UART 在 DMA 传输期间报告运行时错误。 */
  APP_FAULT_NODE_MEMORY,       /**< 节点未位于合法 SRAM/对齐/64 KB 链接窗口。 */
  APP_FAULT_NODE_BUILD,        /**< 单个 DMA 节点参数或构建失败。 */
  APP_FAULT_QUEUE_BUILD,       /**< HAL 队列初始化、插入或闭环失败。 */
  APP_FAULT_WAVEFORM,          /**< PWM 查找表生成或边界自检失败。 */
  APP_FAULT_ILLEGAL_RELINK,    /**< 未启动、模式非法等运行时改链请求。 */
  APP_FAULT_UART_INIT,         /**< GNU wrapper 捕获到 USART2 初始化失败。 */
  APP_FAULT_TIM_INIT,          /**< GNU wrapper 捕获到 TIM2 初始化失败。 */
  APP_FAULT_DMA_INIT           /**< GNU wrapper 捕获到 DMA 初始化失败。 */
} app_fault_t;

/** 应用级诊断快照；字段保持简单定长，便于 GDB 在故障后直接读取。 */
typedef struct
{
  app_fault_t fault;          /**< 最终故障类型；APP_FAULT_NONE 表示无故障。 */
  uint32_t fault_detail;      /**< HAL 状态、寄存器值或超时时间等附加信息。 */
  uint32_t system_status;     /**< mx_system_init() 的原始 system_status_t。 */
  uint32_t tim_kernel_hz;     /**< 启动时实测的 TIM2 内核时钟。 */
  uint32_t button_events;     /**< 应用已经接受的有效按键事件数。 */
  uint32_t processed_events;  /**< P1 前台已经消费的事件数。 */
  uint32_t duty_percent;      /**< P1 当前固定 PWM 占空比。 */
  uint32_t uart_messages;     /**< P1 已完成的阻塞式日志条数。 */
  uint32_t ready;             /**< 初始化完成并进入当前阶段主状态时为 1。 */
  uint32_t sleep_wakeups;     /**< 最终阶段 WFI 返回次数，不等同于按键次数。 */
  app_mode_t desired_mode;    /**< 软件期望模式，不保证 DMA 已经取到新链接。 */
  app_fault_t init_fault;     /**< wrapper 保存的首个外设初始化故障。 */
  uint32_t init_detail;       /**< 初始化故障的 HAL 返回值。 */
} app_diagnostics_t;

extern volatile app_diagnostics_t g_app_diagnostics;

_Noreturn void app_system_fault(uint32_t system_status);
_Noreturn void app_fault(app_fault_t fault, uint32_t detail);
/** 检查 HAL 调用；失败时不会返回，而是进入 app_fault() 保存现场。 */
void app_check_status(hal_status_t status, app_fault_t fault);
/** 完成所选 APP_PHASE 的应用初始化并永久运行。 */
_Noreturn void app_run(void);

#endif
