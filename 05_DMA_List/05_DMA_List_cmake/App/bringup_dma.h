#ifndef BRINGUP_DMA_H
#define BRINGUP_DMA_H

#include <stdint.h>

/** APP_PHASE=2 的单次 Direct DMA 验证结果，供调试器检查。 */
typedef struct
{
  uint32_t timer_completions; /**< TIM2_UP DMA 完成回调次数。 */
  uint32_t uart_completions;  /**< USART2 DMA 发送完成回调次数。 */
  uint32_t start_ms;          /**< TIM2 DMA 启动时的 HAL tick。 */
  uint32_t elapsed_ms;        /**< 2000 个 1 ms 样本的实测总时长。 */
  uint32_t remaining_bytes;   /**< 完成时 CBR1 剩余字节，应为 0。 */
  uint32_t final_ccr;         /**< 完成时 TIM2_CCR1，应回到 0。 */
  uint32_t dma_errors;        /**< HAL DMA 最后错误码。 */
  uint32_t uart_errors;       /**< HAL UART 最后错误码。 */
  uint32_t passed;            /**< 所有 P2 校验通过后置 1。 */
} bringup_dma_diagnostics_t;

extern volatile bringup_dma_diagnostics_t g_bringup_dma;
/** 执行 P2：UART DMA 日志 + TIM2 Direct DMA 波形 + 结果校验。 */
_Noreturn void bringup_direct_run(void);

#endif
