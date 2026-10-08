#ifndef KEY_H
#define KEY_H

#include <stdint.h>

/* LED 标号（用枚举，别用裸数字 0/1/2） */
typedef enum
{
    KEY1 = 0,       /* 板上 SW2 = PB12 */
    KEY2,           /* 板上 SW3 = PB13 */
    KEY_NUM         /* 总数；同时也是非法值的上界 */
} Key_Id_t;

void    Key_Init(void);                  /* 配好 EXTI + NVIC */
void    Key_IrqHandler(void);            /* 供 stm32f1xx_it.c 里的 ISR 调用 */

uint8_t Key_IsPressed(Key_Id_t keyNo);   /* 原始电平：1 = 按下，0 = 松开（非阻塞） */
uint8_t Key_HasEvent(Key_Id_t keyNo);    /* 有"按下"事件返回 1，并自动清标志（非阻塞） */

#endif /* KEY_H */
