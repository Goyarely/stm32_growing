#ifndef _KEY_H
#define _KEY_H

#include <stdint.h>

typedef enum{
    KEY1 = 0,       //板上 SW2 = PB12
    KEY2,           //板上 SW3 = PB13
    KEY_NUM         //总数，同时也是非法值的上界
}Key_Id_t;

void Key_Init(void);
uint8_t Key_IsPressed(Key_Id_t keyNo);  //原始电平：1=按下， 0 = 松开
uint8_t Key_Scan(Key_Id_t keyNo);       //消抖后：1 = 发生了一次“按下”事件

#endif
