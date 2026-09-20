#include "stm32f1xx_hal.h"

#define LED_PORT GPIOB
#define LED_PIN GPIO_PIN_15
#define LED_CLOCK() __HAL_RCC_GPIOB_CLK_ENABLE()
#define Loop_N 1000000UL

static void MX_GPIO_Init(void)
{
  LED_CLOCK();
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  
  GPIO_InitStruct.Pin = LED_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LED_PORT, &GPIO_InitStruct);

  //默认LED 关闭
  HAL_GPIO_WritePin(LED_PORT, LED_PIN, GPIO_PIN_SET);
}

static void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;

  HAL_RCC_OscConfig(&RCC_OscInitStruct);

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2);

}

//用systick 的COUNTER寄存器实现延时,不调用HAL_Delay函数
static void my_delay(uint32_t ms)
{
  for(uint32_t i=0; i<ms; i++)
  {
    while((SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) == 0); //等待计数器到达0
  }
}

int main(void)
{
  volatile uint32_t g_hclk = 0;
  volatile uint32_t g_pclk1 = 0;
  volatile uint32_t g_pclk2 = 0;
  //SystemCoreClockUpdate(); //更新系统时钟变量

  HAL_Init(); //初始化HAL库
  SystemClock_Config(); //配置系统时钟
  MX_GPIO_Init(); //初始化GPIO
  g_hclk = HAL_RCC_GetHCLKFreq();
  g_pclk1 = HAL_RCC_GetPCLK1Freq();
  g_pclk2 = HAL_RCC_GetPCLK2Freq();
  while(1)
  {
    HAL_GPIO_TogglePin(LED_PORT, LED_PIN); //翻转LED状态
    HAL_Delay(1000); //延时1000ms
  }
}