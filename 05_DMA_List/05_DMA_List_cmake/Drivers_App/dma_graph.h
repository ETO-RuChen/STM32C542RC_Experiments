#ifndef DMA_GRAPH_H
#define DMA_GRAPH_H

#include <stdint.h>
#include <stdbool.h>
#include "app_mode.h"
#include "stm32_hal.h"

/** 单通道 DMA 图的构建、运行、改链和首个错误现场。 */
typedef struct
{
  uint32_t node_count;          /**< 正常环和报警环中的节点总数。 */
  uint32_t first_address;       /**< nodes[] 首地址，用于验证 SRAM/link 窗口。 */
  uint32_t last_address;        /**< nodes[] 最后一个字节地址。 */
  uint32_t starts;              /**< 硬件图启动次数，正常运行时应为 1。 */
  uint32_t error_count;         /**< DMA 错误回调次数。 */
  uint32_t error_codes;         /**< HAL_DMA_GetLastErrorCodes() 结果。 */
  uint32_t error_cllr;          /**< 错误发生前保存的链接寄存器。 */
  uint32_t error_src;           /**< 错误发生前保存的源地址。 */
  uint32_t error_dest;          /**< 错误发生前保存的目的地址。 */
  uint32_t relinks;             /**< 成功提交的运行时改链次数。 */
  uint32_t rejected_relinks;    /**< 图未启动或模式非法而拒绝的次数。 */
  uint32_t error_snapshot_valid;/**< 首个 DMA 寄存器快照有效时为 1。 */
  uint32_t error_csr;           /**< DMA channel status register 快照。 */
  uint32_t error_ccr;           /**< DMA channel control register 快照。 */
  uint32_t error_cbr1;          /**< DMA block remaining register 快照。 */
  uint32_t error_ctr1;          /**< DMA transfer register 1 快照。 */
  uint32_t error_ctr2;          /**< DMA transfer register 2 快照。 */
} dma_graph_diagnostics_t;

extern volatile dma_graph_diagnostics_t g_dma_graph;

/** 生成 PWM LUT，构建 N1～N6/A1～A2 静态节点及 HAL 队列。 */
void dma_graph_build(void);
/** 启动唯一的 LPDMA 通道、TIM2 更新请求和 PWM 输出。 */
void dma_graph_start(void);
/** 在图运行期间修改 N6/A2 的 CLLR 后继，使硬件最终收敛到目标环。 */
bool dma_graph_request_mode(app_mode_t mode);
/** HAL 清通道前保存第一个 DTE/ULE/USE 错误的寄存器现场。 */
void dma_graph_capture_error(hal_dma_handle_t *hdma);

#endif
