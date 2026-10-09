#ifndef _TIM_H
#define _TIM_H

#include <stdint.h>          /* 头文件自给自足：uint8_t / int8_t */

void Tim_Init(void);        //配置并开启用得到的定时器
void Tim_IrqHandler(void);  //供stm32f1xx_it.c 里的IAR转发

/* ==================================================================
 * 占空比控制：自动呼吸 + 手动调节
 * ================================================================== */
void    Tim_SetBreath(uint8_t on);   //1 = 自动呼吸；0 = 手动固定
uint8_t Tim_IsBreathing(void);       //当前是否处于自动呼吸（1 = 是）
void    Tim_StepDuty(int8_t step);   //手动加/减占空比(%)，并自动退出呼吸

#endif
