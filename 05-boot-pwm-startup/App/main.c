#include "stm32f1xx_hal.h"
#include "led.h"
#include "key.h"
#include "tim.h"
#include "pwm.h"

/* ==================================================================
 * 按键任务 → 占空比（非阻塞）
 *   SW2 (KEY1)：占空比 +10%
 *   SW3 (KEY2)：占空比 -10%
 *
 * ★ 一按就【接管】：自动呼吸被关闭，进入手动模式
 * ★ 手动模式时 LED2（PB15）点亮作为提示
 * ★ 想回到自动呼吸 → 按板上【复位】键（最简单）
 *
 * 为什么不再用 Led_Toggle(LED_1) 了？
 *   因为 PA8 已经交给 TIM1_CH1 做 PWM（配成了 AF_PP 复用输出），
 *   写 ODR 对它【完全无效】—— 一个引脚只能有一个"主人"。
 * ================================================================== */
static void Key_Duty_Task(void)
{
    if (Key_HasEvent(KEY1) != 0U)        /* SW2 → 变亮 */
    {
        HAL_Delay(20);                   /* 一次性消抖 */
        if (Key_IsPressed(KEY1) != 0U)   /* 二次确认：真的还按着 */
        {
            Tim_StepDuty(+10);
        }
    }

    if (Key_HasEvent(KEY2) != 0U)        /* SW3 → 变暗 */
    {
        HAL_Delay(20);
        if (Key_IsPressed(KEY2) != 0U)
        {
            Tim_StepDuty(-10);
        }
    }

    /* ---- LED2 当"模式指示灯"：亮 = 手动，灭 = 自动呼吸 ---- */
    {
        static uint8_t s_manualLatch = 2U;   /* 2 = 未知，强制首次更新 */

        uint8_t manual = (Tim_IsBreathing() == 0U) ? 1U : 0U;

        if (manual != s_manualLatch)
        {
            s_manualLatch = manual;
            if (manual != 0U) { Led_On(LED_2);  }    /* 本板低电平点亮 */
            else              { Led_Off(LED_2); }
        }
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
    Pwm_Init();                          /* ★ 先配好 PWM（TIM1_CH1 -> PA8）*/
    Tim_Init();                          /* ★ 再启动 1ms 时基；中断里会调 Pwm_SetDuty() */

    while (1)
    {
        Key_Duty_Task();                 /* SW2 / SW3 → 手动调占空比 */
    }
}
