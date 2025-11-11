#include "pwm2.h"
// 占空比设置函数 (0-100%)
void Set_PWM_DutyCycle(float duty_percent)
{
    if(duty_percent < 0) duty_percent = 0;
    if(duty_percent > 100) duty_percent = 100;

    uint16_t pulse = (uint16_t)(duty_percent * htim4.Init.Period / 100.0f);
    __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_4, pulse);
}
// 测试代码
void Test_HW517(void)
{

    // 测试不同占空比
    Set_PWM_DutyCycle(0);    // 关闭
    HAL_Delay(1000);

    Set_PWM_DutyCycle(25);   // 25%功率
    HAL_Delay(1000);

    Set_PWM_DutyCycle(50);   // 50%功率
    HAL_Delay(1000);

    Set_PWM_DutyCycle(75);   // 75%功率
    HAL_Delay(1000);

    Set_PWM_DutyCycle(100);  // 全功率
    HAL_Delay(1000);
}
