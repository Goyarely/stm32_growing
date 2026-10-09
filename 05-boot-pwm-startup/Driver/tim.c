#include "tim.h"
#include "stm32f1xx_hal.h"
#include "stdint.h"
#include "pwm.h"
#include "led.h"

#define TIM_USE_REGISTER 0

static TIM_HandleTypeDef s_htim2;
static uint32_t s_cnt = 0;

static volatile int16_t s_duty     = 0;    /* 占空比，单位 %（0~100） */
static int8_t           s_dir      = 1;    /* 呼吸方向：+1 上行 / -1 下行 */
static volatile uint8_t s_breathOn = 1U;   /* 1 = 自动呼吸；0 = 手动固定 */

/* ==================================================================
 * 呼吸一步：每 10ms 让占空比走一格
 *   ★ 寄存器版和 HAL 版【共用】这一份逻辑，避免两处维护
 *
 *   100 级 × 10ms = 上行 1s；上行 + 下行 = 一个完整呼吸 2s
 *   注意：本板 LED 是【低电平点亮】，
 *         所以 s_duty 从小涨到大 = LED 从最亮到最暗（方向是反的）
 * ================================================================== */
static void Breath_Step(void)
{
    if (s_breathOn == 0U)          /* 手动模式 → 不自动改占空比 */
    {
        return;
    }

    if (++s_cnt < 10U)             /* 每 10ms 调一次（1ms × 10） */
    {
        return;
    }
    s_cnt = 0;

    s_duty += s_dir;
    if (s_duty >= 100) { s_duty = 100; s_dir = -1; }
    if (s_duty <=   0) { s_duty =   0; s_dir =  1; }

    Pwm_SetDuty((uint8_t)s_duty);
}

void Tim_Init(void)
{

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
        Breath_Step();                      //呼吸一步（和 HAL 版共用）
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
        Breath_Step();
    }
}

/* ==================================================================
 * 对外接口：占空比控制
 * ================================================================== */

/* 开关自动呼吸：1 = 打开（自动），0 = 关闭（保持当前占空比不动） */
void Tim_SetBreath(uint8_t on)
{
    s_breathOn = (on != 0U) ? 1U : 0U;
}

/* 当前是否处于自动呼吸（给"模式指示灯"用） */
uint8_t Tim_IsBreathing(void)
{
    return s_breathOn;
}

/* 手动加/减占空比（单位 %）。调一次就【接管】——自动呼吸关闭 */
void Tim_StepDuty(int8_t step)
{
    int16_t d;

    s_breathOn = 0U;               /* ★ 先"接管"，再改值，避免和中断抢 */

    d = s_duty + (int16_t)step;
    if (d > 100) { d = 100; }
    if (d <   0) { d =   0; }
    s_duty = d;

    Pwm_SetDuty((uint8_t)d);
}
