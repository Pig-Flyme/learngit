#include "temperature.h"

#define R_PULL 10000.0f
#define R0     10000.0f
#define T0     298.15f
#define BETA   3950.0f
#define VREF   3.3f

void TempSensor_Init(void) {
    HAL_ADC_Start(&hadc2);
}

float Read_Temperature(void) {
    HAL_ADC_Start(&hadc2);
    if (HAL_ADC_PollForConversion(&hadc2, 100) != HAL_OK) {
        HAL_ADC_Stop(&hadc2);
        return -100.0f;
    }

    uint32_t raw = HAL_ADC_GetValue(&hadc2);
    static float filt = 0;
    filt += ((float)raw - filt) * 0.0625f;
    float adc_value = filt;

    float Vadc = (adc_value / 65535.0f) * VREF;
    if (Vadc <= 0.0f || Vadc >= VREF) {
        HAL_ADC_Stop(&hadc2);
        return -100.0f;
    }

    float Rntc = R_PULL * (Vadc / (VREF - Vadc));
    float invT = (1.0f / T0) + (1.0f / BETA) * logf(Rntc / R0);
    float Tc = (1.0f / invT) - 273.15f;

    if (Tc < 0.0f) Tc = 0.0f;
    if (Tc > 150.0f) Tc = 150.0f;

    HAL_ADC_Stop(&hadc2);
    return Tc;
}
