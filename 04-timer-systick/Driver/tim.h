#ifndef _TIM_H
#define _TIM_H

void Tim_Init(void);        //配置并开启用得到的定时器
void Tim_IrqHandler(void);  //供stm32f1xx_it.c 里的IAR转发

#endif 
