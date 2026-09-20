/**
  ******************************************************************************
  * @file    main.c
  * @brief   最小例程：PA8 低电平点亮 LED，每 500ms 翻转一次。
  * @note    芯片 STM32F103C8；系统时钟为复位默认的 HSI 8MHz（未开 PLL）。
  ******************************************************************************
  */

#include "stm32f1xx_hal.h"

/* LED 硬件配置：PA8 ---> LED ---> 限流电阻 ---> 3.3V（低电平点亮）
 * 换引脚只改下面三行 */
#define LED_GPIO_PORT          GPIOA
#define LED_GPIO_PIN           GPIO_PIN_8
#define LED_GPIO_CLK_ENABLE()  __HAL_RCC_GPIOA_CLK_ENABLE()

int main(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* 复位后系统时钟是 HSI 8MHz，先把频率变量刷新成真实值，HAL_Delay 才准 */
  SystemCoreClockUpdate();

  /* 复位外设、初始化 Flash 接口，并把 SysTick 配成 1ms 中断（HAL_Delay 的时基） */
  HAL_Init();

  /* 配置 LED 引脚：推挽输出、无上下拉、低速 */
  LED_GPIO_CLK_ENABLE();

  GPIO_InitStruct.Pin   = LED_GPIO_PIN;
  GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull  = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LED_GPIO_PORT, &GPIO_InitStruct);

  /* 主循环：每 500ms 翻转一次 PA8 */
  while (1)
  {
    HAL_GPIO_TogglePin(LED_GPIO_PORT, LED_GPIO_PIN);
    HAL_Delay(2000);
  }
}
