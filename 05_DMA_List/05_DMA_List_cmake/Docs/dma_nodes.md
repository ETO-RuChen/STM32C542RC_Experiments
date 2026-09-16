# DMA 节点与运行时改链

以下是 APP_PHASE=7 的实际 8 节点布局，全部由同一个 LPDMA1_CH0 执行。

| 节点 | Request | 源 / 目的 | 宽度 | 源递增 | 后继 |
| --- | --- | --- | --- | --- | --- |
| N1 | USART2_TX | `[NORMAL] Cycle Start\r\n` → TDR | byte → byte | 是 | N2 |
| N2 | TIM2_UPD | fade_up_lut → CCR1 | word → word | 是 | N3 |
| N3 | USART2_TX | `[NORMAL] LED Max\r\n` → TDR | byte → byte | 是 | N4 |
| N4 | TIM2_UPD | fade_down_lut → CCR1 | word → word | 是 | N5 |
| N5 | USART2_TX | `[NORMAL] Cycle Done\r\n` → TDR | byte → byte | 是 | N6 |
| N6 | TIM2_UPD | constant_zero → CCR1，重复 500 次 | word → word | 否 | N1 / A1 |
| A1 | USART2_TX | `[ALARM] Active\r\n` → TDR | byte → byte | 是 | A2 |
| A2 | TIM2_UPD | alarm_lut → CCR1 | word → word | 是 | A1 / N1 |

所有目的地址固定。UART 长度不包含 C 字符串末尾 NUL；HAL2 transfer size 使用 byte 单位，N6 的 500 次 word 传输对应 2000 bytes。
TIM2 更新频率 1 kHz 时，1000 个 word 样本约为 1 秒；暗态 500 次约 500 ms。UART 发送和节点装载时间会额外增加周期时长。

报警 LUT 为 600 个 word：75 个 999、75 个 0，重复四组。峰值 999 对应 ARR=999 下的 99.9% 占空比。

每个节点采用完整静态格式，显式配置所有必要字段；request 采用 BURST 单次传输节拍，不启用额外 DMA trigger，不使用宽度转换。
节点和 LUT 的生命周期覆盖整个运行过程；描述符位于 DMA 可访问 SRAM，所有 link 使用同一 CLBAR 高地址窗口。

P7 两种期望拓扑：

| desired_mode | N6.next | A2.next |
| --- | --- | --- |
| NORMAL | N1 | N1 |
| ALARM | A1 | A1 |

CLLR 位于 `hal_dma_node_t.regs[5]`（偏移 20 bytes）；每个 HAL node 另有 info 字，不能将节点长度误认为只有 24 bytes。8 节点数组 `_Alignas(4)`、static SRAM 分配；启动前验证完整数组处于 0x20000000～0x2000FFFF 且不跨 CLBAR 窗口。

目标 link 字预计算为 `LL_DMA_UPDATE_ALL | (target_address & DMA_CLLR_LA)`；CLBAR 保存共同高 16 位。只通过对齐的 volatile 32-bit 存储更新 SRAM，不修改硬件当前 CLLR，不改其他节点配置。

请求 ALARM 时先 A2→A1，再 N6→A1；请求 NORMAL 时先 N6→N1，再 A2→N1。两次写之间 DMB，最后 DSB；保存并恢复 PRIMASK。关闭 CPU 中断不会阻止 DMA 取链，两次写也不是原子图切换。旧/新目标均是常驻有效节点；意图是让可能的旧分支在后续取链时收敛。

实测已看到模式切换且 starts 始终为 1；对齐存储/屏障本身不能替代芯片手册的并发取链保证。RM0522 尚未取得，不能保证固定最大切换延迟或所有竞争条件下的行为。

## 时序边界

UART 节点期间 TIM2 持续计数并保持当前 CCR，UART DMA 节点结束只代表最后一个字节写入 TDR，不表示停止位已发送。CCR preload、新节点装载和未选 TIM2 request 期间的请求行为会影响首样本边界。
因此约 2.5 秒正常周期、约 600 ms 报警周期是观测与样本数量对应关系；尚无 PA5/PA2 示波器联测证据，不作精确相位和无积压声明。
