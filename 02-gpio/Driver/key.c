#include "key.h"
#include "stm32f1xx_hal.h"

/* ==================================================================
 * 极性抽象：本板 按下时低电平，换板子只改这
 * ================================================================== */
#define KEY_ISPREES GPIO_PIN_RESET  //输入低电平
#define KEY_NOPREES GPIO_PIN_SET    //输入高电平

/* ==================================================================
 * 硬件映射表：加一个 KEY 只改这里
 * ================================================================== */

 typedef struct
 {
    GPIO_TypeDef *port;
    uint16_t pin;
    uint32_t rccmask;
 } Key_Hw_t;

 static const Key_Hw_t g_keyHw[] = 
 {
    {GPIOB, GPIO_PIN_12, RCC_APB2ENR_IOPBEN},
    {GPIOB, GPIO_PIN_13, RCC_APB2ENR_IOPBEN},
 };

#define KEY_HW_NUM (sizeof(g_keyHw) / sizeof(g_keyHw[0]))

 void Key_Init(void)
 {
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    for(uint8_t i = 0; i < KEY_NUM; i++)
    {
        //时钟使能
        RCC->APB2ENR |= g_keyHw[i].rccmask;

        //配置gpio
        GPIO_InitStruct.Pin = g_keyHw[i].pin;
        GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

        //GPIO初始化
        HAL_GPIO_Init(g_keyHw[i].port, &GPIO_InitStruct);
    }
 }

uint8_t Key_IsPressed(Key_Id_t keyNo)
{
    if((unsigned)keyNo >= KEY_NUM)
    {
        return 0;
    }
    GPIO_PinState state;
    state = HAL_GPIO_ReadPin(g_keyHw[keyNo].port, g_keyHw[keyNo].pin);
    if(state == KEY_ISPREES)
    {
        return 1;
    }
    else{
        return 0;
    }
}

 uint8_t Key_Scan(Key_Id_t keyNo)
 {
    if(!Key_IsPressed(keyNo))
    {
        return 0;
    }
    HAL_Delay(20);
    if(Key_IsPressed(keyNo))
    {
        while(Key_IsPressed(keyNo));
        return 1;

    }
    while(Key_IsPressed(keyNo))
    {

    }
    return 0;
 }

 