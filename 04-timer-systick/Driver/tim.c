#include "tim.h"
#include "stm32f1xx_hal.h"
#include "stdint.h"
#include "led.h"

#define TIM_USE_REGISTER 1

static TIM_HandleTypeDef s_htim2;
static uint32_t s_cnt = 0;

void Tim_Init(void)
{
    static uint32_t s_dummy = 0;

#if TIM_USE_REGISTER
    /* ============ 寄存器版 ============ */
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN; //开时钟

    TIM2->CR1 &= ~TIM_CR1_DIR;          //向上计数
    TIM2->CR1 &= ~TIM_CR1_ARPE;         //与HAL DAISABLE一样

    TIM2->PSC = 7199;                   //预分频
    TIM2->ARR = 9;                      //10kHz/10 = 1kHz = 1ms

    TIM2->EGR = TIM_EGR_UG;             //立即加载psc/ARR
    TIM2->SR = 0;                       //清掉UG顺带置的UIF，避免一个“立即”的中断

    TIM2->DIER |= TIM_DIER_UIE;         //使能更新中断

    HAL_NVIC_SetPriority(TIM2_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(TIM2_IRQn);

    TIM2->CR1 |= TIM_CR1_CEN;           //启动计数

    (void)s_dummy;
#else
    /* ============ HAL 版 ============ */
    __HAL_RCC_TIM2_CLK_ENABLE();

    s_htim2.Instance = TIM2;
    s_htim2.Init.Prescaler = 7199;
    s_htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
    s_htim2.Init.Period = 9;
    s_htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    s_htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    HAL_TIM_Base_Init(&s_htim2);

    HAL_NVIC_SetPriority(TIM2_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(TIM2_IRQn);
    HAL_TIM_Base_Start_IT(&s_htim2);
#endif
}

void Tim_IrqHandler(void)
{
#if TIM_USE_REGISTER
    /* ============ 寄存器版：骨架 3 的三步全手写 ============ */
    if((TIM2->SR & TIM_SR_UIF) != 0U)   //查：是我触发的吗
    {
        TIM2->SR = (uint32_t)~TIM_SR_UIF;   //清零
        if(++s_cnt >= 500)
        {
            s_cnt = 0;
            Led_Toggle(LED_1);
        }
    }
#else
    /* ============ HAL 版 ============ */
    HAL_TIM_IRQHandler(&s_htim2);
#endif
}

//HAL弱回调：只有HAL版会到这
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM2)
    {
        if (++s_cnt >= 500U)
        {
            s_cnt = 0;
            Led_Toggle(LED_1);
        }
    }
}