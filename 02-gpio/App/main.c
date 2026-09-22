#include "stm32f1xx_hal.h"
#include "led.h"
#include "key.h"

int main(void)
{
  SystemCoreClockUpdate();     /* 必须在 HAL_Init 之前 */
  HAL_Init();

  Led_Init();                  /* 驱动初始化 */
  Key_Init();                   //按键初始化
  volatile uint32_t g_pressCnt = 0;//按键事件计数
  volatile uint32_t g_toggleCnt = 0;//LED实际翻转次数
  while (1)
  {
    // Led_On(LED_2);
    // HAL_Delay(500);
    // Led_Off(LED_2);
    // Led_On(LED_1);
    // HAL_Delay(500);
    // Led_Off(LED_1);
    if(Key_Scan(KEY1))
    {
      g_pressCnt++;
      Led_Toggle(LED_1);
      g_toggleCnt++;
    }
    if(Key_Scan(KEY2))
    {
      Led_Toggle(LED_2);
    }
  }
}

