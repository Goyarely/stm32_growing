#include "stm32f1xx_hal.h"
#include "led.h"
#include "key.h"

/* ==================================================================
 * 按键任务（非阻塞）
 *   中断负责"检测"（按下瞬间置标志）
 *   这里负责"消抖 + 响应"（每圈调用一次，绝大多数圈立即返回）
 *
 * 100ms 窗口的作用：过滤机械抖动，以及"极快的连击"
 * （人手最快连按间隔约 100ms，实测无感；要更精细就上状态机）
 * ================================================================== */
static void Key_Task(Key_Id_t keyNo, Led_Id_t ledNo)
{
    static uint32_t s_lastAccept[KEY_NUM] = {0};

    if (Key_HasEvent(keyNo) == 0U)      /* 无事件 → 立即返回，几乎零开销 */
    {
        return;
    }

    if ((uint32_t)(HAL_GetTick() - s_lastAccept[keyNo]) < 100U)
    {
        return;                          /* 抖动或极快连击 → 丢弃 */
    }

    HAL_Delay(20);                       /* 一次性消抖 */

    if (Key_IsPressed(keyNo) != 0U)      /* 二次确认：真的还按着 */
    {
        s_lastAccept[keyNo] = HAL_GetTick();
        Led_Toggle(ledNo);               /* 按下立即响应，不等松手 */
    }
}

int main(void)
{
    SystemCoreClockUpdate();             /* 必须在 HAL_Init 之前：刷新软件里的频率值 */
    HAL_Init();

    Led_Init();                          /* 指示灯 */
    Key_Init();                          /* EXTI 按键 */

    while (1)
    {
        Key_Task(KEY1, LED_1);           /* 按 SW2 → 翻转 LED1 */
        Key_Task(KEY2, LED_2);           /* 按 SW3 → 翻转 LED2 */
    }
}
