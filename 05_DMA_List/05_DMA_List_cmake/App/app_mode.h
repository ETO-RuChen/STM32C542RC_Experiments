#ifndef APP_MODE_H
#define APP_MODE_H

/** DMA 硬件图的两种目标运行模式，同时用作 branch_words[] 的下标。 */
typedef enum
{
  APP_MODE_NORMAL = 0, /**< 正常呼吸灯环：N1 -> ... -> N6。 */
  APP_MODE_ALARM = 1  /**< 报警闪烁环：A1 -> A2。 */
} app_mode_t;

#endif
