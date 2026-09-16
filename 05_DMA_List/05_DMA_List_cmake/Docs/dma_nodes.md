# DMA 节点设计草案

以下是 P3～P7 的目标，不是当前 P1 已实现的功能。

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

报警 LUT 应包含约 50～100 ms 的亮/灭平台；如果逐样本直接 999、0 交替，在 1 kHz 更新下只形成 500 Hz 包络，肉眼看不到目标报警闪烁。

P3/P4 每个节点采用完整静态格式，显式配置所有必要字段；不启用额外 DMA trigger，不使用宽度转换。
节点和 LUT 的生命周期覆盖整个运行过程；描述符位于 DMA 可访问 SRAM，所有 link 使用同一 CLBAR 高地址窗口。

P7 两种期望拓扑：

| desired_mode | N6.next | A2.next |
| --- | --- | --- |
| NORMAL | N1 | N1 |
| ALARM | A1 | A1 |

两次 link 写入并不是一个原子图切换。需要验证先建立目标闭环、再开放出口的写入顺序，以及 DMA 已装载旧值时的收敛行为。尚未获得 RM/实测依据，不保证某个固定的最大切换延迟。
