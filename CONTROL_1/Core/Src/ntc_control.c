#include "ntc_control.h"
#include "pid.h"
#include "relay.h"
#include "temperature.h"
#include "usart.h"
#include <string.h>
#include <math.h>

#define SETPOINT 37.0f
#define PRE_ADJUST_TOLERANCE 1.0f
#define AUTO_TUNE_DURATION 300000   // 5分钟
#define FORCE_HEATING_THRESHOLD 36.0f

static PID_HandleTypeDef pid;
static PID_Params_t pid_params = {4.0f, 0.05f, 2.0f};
static AutoTuneHandle tune_handle;
static uint8_t tune_complete = 0;
static uint32_t last_heating_time = 0;

// 稳定性和振荡检测
static uint8_t stable_count = 0;
static uint8_t recording_started = 0;
static uint8_t oscillation_count = 0;
static uint8_t cross_setpoint = 0;
static float last_temp_for_osc = SETPOINT;
#define STABLE_THRESHOLD 8

// PWM控制参数
#define PWM_PERIOD 10000  // 10秒周期
static uint32_t pwm_start_time = 0;
static uint32_t current_on_time = 0;
static uint8_t pwm_initialized = 0;

// =================== 阶段1：预调节 ===================
static void PreAdjustTemperature(void) {
    float temp = Read_Temperature();
    printf("Initial temp: %.2f℃, Target: %.2f℃\r\n", temp, SETPOINT);

    if (fabsf(temp - SETPOINT) > PRE_ADJUST_TOLERANCE) {
        printf("Pre-adjusting temperature...\r\n");
        while (fabsf(Read_Temperature() - SETPOINT) > PRE_ADJUST_TOLERANCE) {
            temp = Read_Temperature();

            if (temp >= 50.0f) {
                Relay_Switch(0);
                printf("[NTC] Overtemperature during pre-adjust! %.2f℃\r\n", temp);
                break;
            }
            Relay_Switch(temp < SETPOINT ? 1 : 0);
            HAL_Delay(1000);
        }
        Relay_Switch(0);
        HAL_Delay(2000);
    }
}

// =================== 初始化 ===================
void NTC_Control_Init(void) {
    PreAdjustTemperature();
    PID_Init(&pid, pid_params.Kp, pid_params.Ki, pid_params.Kd, -100.0f, 100.0f);
    pid.integral = 0;
    pid.last_error = 0;

    PID_AutoTune_Init(&tune_handle, &pid_params, Read_Temperature, Relay_Switch,
                      SETPOINT, AUTO_TUNE_DURATION);
    tune_complete = 0;
    last_heating_time = HAL_GetTick();

    // 初始化振荡检测变量
    stable_count = 0;
    recording_started = 0;
    oscillation_count = 0;
    cross_setpoint = 0;
    last_temp_for_osc = Read_Temperature();

    // 初始化PWM
    pwm_start_time = HAL_GetTick();
    current_on_time = 0;
    pwm_initialized = 1;
}

// =================== 10秒周期PWM控制 ===================
void NTC_Control_Update(void) {
    float temp = Read_Temperature();
    if (temp <= -50.0f) return;

    // --- 高温保护 ---
    if (temp >= 50.0f) {
        Relay_Switch(0);
        printf("[NTC] Overtemperature! %.2f℃\r\n", temp);
        return;
    }

    // --- 自整定 ---
    if (tune_handle.state != TUNE_IDLE) {
        PID_AutoTune_Task(&tune_handle);
        if (tune_handle.state == TUNE_IDLE) {
            if (pid_params.Kp <= 0 || pid_params.Ki <= 0) {
                pid_params.Kp = 4.0f;
                pid_params.Ki = 0.05f;
                pid_params.Kd = 2.0f;
            }
            PID_UpdateParams(&pid, pid_params.Kp, pid_params.Ki, pid_params.Kd);
            tune_complete = 1;
            // 重置稳定性和振荡检测
            stable_count = 0;
            recording_started = 0;
            oscillation_count = 0;
            printf("PID tuned: Kp=%.3f, Ki=%.3f, Kd=%.3f\r\n",
                   pid_params.Kp, pid_params.Ki, pid_params.Kd);
        }
        return;
    }

    // --- 计算真实dt ---
    static uint32_t last_time = 0;
    uint32_t now = HAL_GetTick();
    float dt = (now - last_time) / 1000.0f;
    if (dt <= 0.0f || dt > 2.0f) dt = 1.0f;
    last_time = now;

    // --- PID计算 ---
    float output = PID_Calculate(&pid, SETPOINT, temp, dt);

    // --- 10秒周期PWM控制 ---
    if (!pwm_initialized) {
        pwm_start_time = now;
        pwm_initialized = 1;
    }

    // 将PID输出(-100~100)映射到PWM占空比(0~100%)
    // 当output>0时加热，output<=0时不加热
    float duty_cycle = 0.0f;
    if (output > 0) {
        duty_cycle = output / 100.0f;  // 0.0 ~ 1.0
        if (duty_cycle > 1.0f) duty_cycle = 1.0f;
    }

    // 计算当前周期内的开启时间
    current_on_time = (uint32_t)(duty_cycle * PWM_PERIOD);

    // 计算当前周期内的时间位置
    uint32_t cycle_position = (now - pwm_start_time) % PWM_PERIOD;

    // PWM控制逻辑
    if (cycle_position < current_on_time) {
        Relay_Switch(1);  // 在开启时间内
    } else {
        Relay_Switch(0);  // 在关闭时间内
    }

    // 每周期开始时打印PWM状态
    if (cycle_position == 0 && current_on_time > 0) {
    	printf("[PWM] Cycle start: ON for %ums of %ums (%.1f%%)\r\n",
    	       (unsigned int)current_on_time, (unsigned int)PWM_PERIOD, duty_cycle * 100.0f);
    }

    // --- 温和的强制加热逻辑（仅在温度远低于设定值时触发）---
    if (temp < (SETPOINT - 2.0f) && (now - last_heating_time) > 60000) {
        // 在强制加热时，临时设置100%占空比
        current_on_time = PWM_PERIOD;
        last_heating_time = now;
        printf("[FORCE] Force heating at %.2f℃\r\n", temp);
    }

    // --- 稳定状态检测 ---
    if (fabsf(temp - SETPOINT) <= 0.1f) {
        stable_count++;
        if (stable_count >= STABLE_THRESHOLD && !recording_started) {
            recording_started = 1;
            printf("=== STABLE STATE REACHED - Start Recording ===\r\n");
        }
    } else {
        stable_count = 0;
    }

    // --- 振荡次数检测 ---
    if ((last_temp_for_osc < SETPOINT && temp >= SETPOINT) ||
        (last_temp_for_osc > SETPOINT && temp <= SETPOINT)) {
        cross_setpoint = 1;
    }

    if (cross_setpoint && fabsf(temp - SETPOINT) > 0.3f) {
        oscillation_count++;
        cross_setpoint = 0;
        printf("[OSC] Oscillation detected: %d/2\r\n", oscillation_count);

        if (oscillation_count >= 2 && !recording_started) {
            recording_started = 1;
            printf("=== 2 OSCILLATIONS COMPLETE - Start Recording ===\r\n");
        }
    }

    last_temp_for_osc = temp;

    // --- UART输出 ---
    static uint32_t last_debug_time = 0;
    if (now - last_debug_time >= 2000) {  // 每2秒输出一次，避免太频繁
        char buf[128];
        uint32_t cycle_pos = (now - pwm_start_time) % PWM_PERIOD;
        snprintf(buf, sizeof(buf), "Temp: %.2fC | PWM: %lu/%lums | Out: %.1f | Kp:%.2f | Osc:%d/2 | Rec:%s\r\n",
                 temp, cycle_pos < current_on_time ? current_on_time - cycle_pos : 0,
                 current_on_time, output, pid.params.Kp, oscillation_count,
                 recording_started ? "YES" : "NO");
        HAL_UART_Transmit(&huart1, (uint8_t*)buf, strlen(buf), 100);
        last_debug_time = now;
    }
}

uint8_t Is_Recording_Started(void) {
    return recording_started;
}

uint8_t Get_Oscillation_Count(void) {
    return oscillation_count;
}

uint8_t Get_Stable_Count(void) {
    return stable_count;
}
