# P0 本地可行性核查

本记录列出已查证内容和仍需参考手册/上板确认的边界。P0 尚未全部完成。

## 已查证的依据

| 项目 | 本地证据 | 结论 |
| --- | --- | --- |
| CPU / MCU | `stm32c5xx_dfp/Include/stm32c542xx.h`、`cmake/target.cmake` | STM32C542xx，Cortex-M33，使用 LPDMA |
| 时钟 | `generated/hal/mx_rcc.c` | HSE 24 MHz → PSIS 144 MHz；APB1 分频为 1 |
| PWM | `generated/hal/mx_tim2.c` | PSC=143、ARR=999、PA5 AF1、CH1 PWM1、比较预装载开启 |
| 单控制器 request | `stm32c5xx_drivers/ll/stm32c5xx_ll_dma.h` | LPDMA1 USART2_TX=15、TIM2_UPD=35；最终优先评估 LPDMA1_CH0 |
| HAL2 链表 | `stm32c5xx_drivers/hal/stm32c5xx_hal_dma.h` | 存在 FillNodeConfig、StartLinkedListXfer 及 `_IT_Opt` API |
| 软件队列 | `stm32c5xx_drivers/hal/stm32c5xx_hal_q.h` | 存在 HAL_Q_Init、InsertNode、SetCircularLinkQ 等接口 |
| 功能开关 | `generated/hal/stm32c5xx_hal_conf.h`、`Config/app_hal_overrides.h` | 生成器原值为 0，项目 force-include 统一覆盖为 1；DMA 头文件据此定义 Q circular 支持 |
| 节点对齐 | `stm32c5xx_drivers/hal/stm32c5xx_hal_dma.c` 的 Node management 说明 | 节点必须 32-bit 对齐，不超出 64 KB 寻址窗口 |
| 链地址编码 | `stm32c5xx_dfp/Include/stm32c542xx.h` | CLBAR.LBA 掩码 0xFFFF0000，CLLR.LA 掩码 0x0000FFFC |
| 节点布局 | `stm32c5xx_drivers/ll/stm32c5xx_ll_dma.h` | 6 个寄存器字；CLLR 在寄存器数组偏移 20 bytes；HAL 另有 info 字段 |
| SRAM 布局 | 芯片头文件及 `user_modifiable/Device/STM32C542RCT6/stm32c542xc_flash.ld` | SRAM1 0x20000000 / 32 KB，SRAM2 0x20008000 / 32 KB，链接器合为 64 KB RAM |
| Cache / MPU | `generated/hal/mx_icache.c`、`mx_cortex_mpu.c` | 当前启用 ICACHE；MPU 特殊只读区域为 0x08FFE000～0x08FFFFFF，未配置 SRAM 数据缓存维护 |
| 板载按键和 LED | 已安装 `nucleo-c542rc_hw-board/2.1.0/Descriptors/NUC542RC$KR1/pcbs/MB2213/part-parameters.json` | MB2213 B02 的 B1、LD1 为高有效 |

## 设计决定

- P1、P2 使用生成器现有资源。最终单通道归属由 dma_graph 管理，避免同时让 TIM/UART 两套 HAL 传输流程控制同一 DMA handle。
- P3 先使用完整静态节点，不优化为 dynamic node。异构节点必须重载 CTR1、CTR2、CBR1、CSAR、CDAR、CLLR 等所需字段。
- 所有正常/报警描述符应集中在同一静态数组中，对齐并验证相同地址高 16 位；检查整个数组末端而不只检查起点。
- 当前 SRAM 全部位于同一个 64 KB 窗口；仍需校验实际 map、地址可访问性以及将来的链接脚本变更。
- HAL node 的 next 是编码后的 CLLR 字，不是可以随意写入完整指针的 C 字段。
- `HAL_DMA_SetNodeAddress` 操作 SRAM 中的 link 编码，但其存在不能证明在运行中任意改链安全。普通 Q 插入/删除会维护软件队列元数据，不直接作为运行中改图方案。
- 在 Timer 节点到来前/之后的 UART 阶段，TIM2 仍运行并保持最近有效占空比；需要实测 request 是否积压、preload 延迟和首个样本时序。

## 尚待查证

1. RM0522 对 LPDMA SRAM/外设可访问范围的规定。
2. 节点取链与 CLLR 装载时机、预取深度、CPU 与 DMA 并发访问同一描述符字的限制。
3. 32-bit 对齐 link 写入的硬件原子性、内存排序要求及必要屏障。
4. N6/A2 当前已被装载时，新链接可能延迟至下一次取该节点才生效；不能承诺本轮结束立即切换。
5. 单次 TIM2 update 只搬一个 word 的请求模式，以及不选 TIM2 request 期间的请求行为。
6. 完成 UART DMA 节点只表示最后一个数据项已写入外设，不等同于最后一个停止位已发送；验收需考虑这一时间差。
7. 新图存在多个环时，HAL Q 元数据与实际图的边界，以及 IRQ/error 路径是否会遍历已更改的拓扑。

本机工程和已安装包中未找到 RM0522 PDF；访问 ST 的短链接 `https://www.st.com/resource/en/reference_manual/rm0522.pdf` 返回 HTTP 567，未取得文档，不能以此核查硬件安全性。
用户确认本地也没有 RM0522。后续已按实验方式完成 P1～P7 上板验证及 P8 错误路径验证，详见 work_plan.md；这些实测缩小了不确定范围，但不能替代上述手册核查。

当前 IRQ 错误路径经源码确认：HAL_DMA_IRQHandler 处理 channel flags 并复位通道，不需要遍历已改链的 HAL_Q 图。项目在它之前保存错误寄存器快照。运行中禁止对两个 Q 做拓扑修改或遍历操作。
