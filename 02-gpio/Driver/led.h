#ifndef __LED_H
#define __LED_H

/* 这个头文件只暴露"接口"，不暴露硬件细节 —— 所以不需要 include HAL */

/* LED 标号（用枚举，别用裸数字 0/1/2） */
typedef enum
{
  LED_1 = 0,      /* 板上 LED1 = PA8  */
  LED_2,          /* 板上 LED2 = PB15 */
  LED_NUM         /* 总数；同时也是非法值的上界 */
} Led_Id_t;

void Led_Init(void);
void Led_On(Led_Id_t ledNo);
void Led_Off(Led_Id_t ledNo);
void Led_Toggle(Led_Id_t ledNo);

#endif /* __LED_H */
