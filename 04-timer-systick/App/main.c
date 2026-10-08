#include "stm32f1xx_hal.h"
#include "led.h"
#include "key.h"
#include "tim.h"

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

static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    //配置振荡器和PLL
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;  //使用HES
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;                    //打开外部晶振
    RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;     //HSE不分频，8MHz
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;                    //HSI 也开着（备用）
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;               //打开PLL
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;        //PLL输入来自HSE
    RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;                //8MHz * 9 = 72MHz

    HAL_RCC_OscConfig(&RCC_OscInitStruct);

    //2.配置系统时钟和总线分频
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                               | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2);


}

int main(void)
{

    HAL_Init();
    SystemClock_Config();

    Led_Init();                          /* 指示灯 */
    Key_Init();                          /* EXTI 按键 */
    Tim_Init();

    while (1)
    {
        Key_Task(KEY1, LED_1);           /* 按 SW2 → 翻转 LED1 */
        Key_Task(KEY2, LED_2);           /* 按 SW3 → 翻转 LED2 */
    }
}
