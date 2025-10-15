#ifndef TEMPERATURE_H
#define TEMPERATURE_H

#include "stm32h7xx_hal.h"
#include "adc.h"

void TempSensor_Init(void);
float Read_Temperature(void);

#endif
