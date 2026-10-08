#include "led.h"
#include "stm32f1xx_hal.h"

/* ==================================================================
 * 极性抽象：本板 LED 是【低电平点亮】，换板子只改这两行
 * ================================================================== */
#define LED_ON_LEVEL    GPIO_PIN_RESET    /* 拉低 → 亮 */
#define LED_OFF_LEVEL   GPIO_PIN_SET      /* 拉高 → 灭 */

/* ==================================================================
 * 硬件映射表：加一个 LED 只改这里
 * ================================================================== */
typedef struct
{
  GPIO_TypeDef *port;      /* GPIOA / GPIOB        */
  uint16_t      pin;       /* GPIO_PIN_x           */
  uint32_t      rccMask;   /* RCC_APB2ENR_IOPxEN   */
} Led_Hw_t;

static const Led_Hw_t g_ledHw[] =
{
  /* LED_1 */ { GPIOA, GPIO_PIN_8,  RCC_APB2ENR_IOPAEN },
  /* LED_2 */ { GPIOB, GPIO_PIN_15, RCC_APB2ENR_IOPBEN },
};

#define LED_HW_NUM   (sizeof(g_ledHw) / sizeof(g_ledHw[0]))

/* 编译期断言：表的项数必须和 Led_Id_t 里的 LED_NUM 一致。
 * 注意 sizeof 不能用在 #if 里，所以用"负长度数组"这个经典技巧。 */
typedef char led_hw_num_must_match[(LED_NUM == (int)LED_HW_NUM) ? 1 : -1];


void Led_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    for(uint8_t i = 0; i < LED_HW_NUM; i++)
    {
        //开时钟
        RCC->APB2ENR |= g_ledHw[i].rccMask;

        //配置gpio
        GPIO_InitStruct.Pin =   g_ledHw[i].pin;
        GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        HAL_GPIO_Init(g_ledHw[i].port, &GPIO_InitStruct);

        //上电默认熄灭
        HAL_GPIO_WritePin(g_ledHw[i].port, g_ledHw[i].pin, LED_OFF_LEVEL);
    }
}
//亮灯
void Led_On(Led_Id_t ledNo)
{
    if((unsigned)ledNo >= LED_HW_NUM)
    {
        return;
    }
    HAL_GPIO_WritePin(g_ledHw[ledNo].port, g_ledHw[ledNo].pin, LED_ON_LEVEL);
}
//熄灭
void Led_Off(Led_Id_t ledNo)
{
    if((unsigned)ledNo >= LED_HW_NUM)
    {
        return;
    }
    HAL_GPIO_WritePin(g_ledHw[ledNo].port, g_ledHw[ledNo].pin, LED_OFF_LEVEL);
}
//翻转
void Led_Toggle(Led_Id_t ledNo)
{
    if((unsigned)ledNo >= LED_HW_NUM)
    {
        return;
    }
    HAL_GPIO_TogglePin(g_ledHw[ledNo].port, g_ledHw[ledNo].pin);
}
