#ifndef NTC_CONTROL_H
#define NTC_CONTROL_H

#include "pid.h"

void NTC_Control_Init(void);
void NTC_Control_Update(void);
uint8_t Is_Recording_Started(void);
uint8_t Get_Oscillation_Count(void);
uint8_t Get_Stable_Count(void);

#endif
