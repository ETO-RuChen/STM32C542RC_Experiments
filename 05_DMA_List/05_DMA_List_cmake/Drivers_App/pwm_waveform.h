#ifndef PWM_WAVEFORM_H
#define PWM_WAVEFORM_H

#include <stdbool.h>
#include <stdint.h>

/** 一段连续 32-bit CCR1 样本；每个样本由一次 TIM2_UP DMA 请求消费。 */
typedef struct
{
  const uint32_t *samples;
  uint32_t count;
} pwm_waveform_t;

/** 在 SRAM 中生成渐亮、渐暗和报警查找表，并检查所有 CCR1 边界。 */
bool pwm_waveform_init(void);
/** 返回 0 -> 999 的 1 秒渐亮波形。 */
pwm_waveform_t pwm_waveform_up(void);
/** 返回 999 -> 0 的 1 秒渐暗波形。 */
pwm_waveform_t pwm_waveform_down(void);
/** 返回 4 次 75 ms 亮/75 ms 灭的报警波形。 */
pwm_waveform_t pwm_waveform_alarm(void);

#endif
