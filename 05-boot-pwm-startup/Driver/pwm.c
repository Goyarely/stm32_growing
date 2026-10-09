#include "stm32f1xx_hal.h"

static TIM_HandleTypeDef s_tim1;

void Pwm_Init(void)
{
    //时钟使能
    __HAL_RCC_TIM1_CLK_ENABLE();        //TMI1挂APB2
    __HAL_RCC_GPIOA_CLK_ENABLE();

    //PA8->TIM1_CH1:必须（复用推挽）
    GPIO_InitTypeDef g = {0};
    g.Pin = GPIO_PIN_8;
    g.Mode = GPIO_MODE_AF_PP;
    g.Pull = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_LOW;

    HAL_GPIO_Init(GPIOA, &g);

    //时基：PSC=71->1MHZ; ARR=999->1kHz,1000
    s_tim1.Instance = TIM1;
    s_tim1.Init.Prescaler = 71;
    s_tim1.Init.CounterMode = TIM_COUNTERMODE_UP;
    s_tim1.Init.Period = 999;
    s_tim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    s_tim1.Init.RepetitionCounter = 0;  //高级定时器独有的字段
    s_tim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    HAL_TIM_PWM_Init(&s_tim1);

    //通道1：pwm模式1
    TIM_OC_InitTypeDef oc = {0};

    oc.OCMode = TIM_OCMODE_PWM1;
    oc.Pulse = 0;                   //初始占空比
    oc.OCPolarity = TIM_OCPOLARITY_HIGH;
    oc.OCNPolarity = TIM_OCNPOLARITY_HIGH;
    oc.OCFastMode = TIM_OCFAST_DISABLE;
    oc.OCIdleState = TIM_OCIDLESTATE_RESET;
    oc.OCNIdleState = TIM_OCNIDLESTATE_RESET;
    HAL_TIM_PWM_ConfigChannel(&s_tim1, &oc, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&s_tim1, TIM_CHANNEL_1);   /* ← 补在最后 */

}

//设置占空比
void Pwm_SetDuty(uint8_t duty)
{
    if(duty > 100u) duty = 100u;

    //1000级->CCR = duty% * 10
    __HAL_TIM_SET_COMPARE(&s_tim1, TIM_CHANNEL_1, (uint32_t)duty * 10u);
}