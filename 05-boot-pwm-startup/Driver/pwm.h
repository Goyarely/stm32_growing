#ifndef _PWM_H
#define _PWM_H
#include "stdint.h"

void Pwm_SetDuty(uint8_t duty);

void Pwm_Init(void);

#endif