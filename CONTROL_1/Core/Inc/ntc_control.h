#ifndef NTC_CONTROL_H
#define NTC_CONTROL_H

#include "pid.h"

void NTC_Control_Init(void);
void NTC_Control_Update(void);
// 添加获取状态函数
uint8_t NTC_IsHeating(void);
uint8_t NTC_IsTuneComplete(void);

#endif
