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
#define FORCE_HEATING_THRESHOLD 36.5f
#define RELAY_HYSTERESIS 0.2f       // 滞环防抖动

static PID_HandleTypeDef pid;
static PID_Params_t pid_params = {4.0f, 0.05f, 2.0f};  // 新推荐参数
static AutoTuneHandle tune_handle;
static uint8_t tune_complete = 0;
static uint32_t last_heating_time = 0;

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
}

// =================== 主控制任务 ===================
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

    // --- 滞环继电器控制 ---
    static uint8_t relay_state = 0;
    if (temp < SETPOINT - RELAY_HYSTERESIS) relay_state = 1;
    else if (temp > SETPOINT + RELAY_HYSTERESIS) relay_state = 0;
    Relay_Switch(relay_state);

    // --- 强制加热逻辑 ---
    if (temp < FORCE_HEATING_THRESHOLD && (now - last_heating_time) > 30000) {
        Relay_Switch(1);
        last_heating_time = now;
        printf("[FORCE] Force heating: %.2f℃\r\n", temp);
        return;
    }

    // --- 自适应参数微调 ---
    float dTemp = temp - pid.last_pv;
    PID_SelfAdjust(&pid, SETPOINT - temp, dTemp);
    pid.last_pv = temp;

    // --- UART输出 ---
    char buf[128];
    snprintf(buf, sizeof(buf), "Temp: %.2fC | Relay: %s | Out: %.1f | Kp:%.2f Ki:%.3f\r\n",
             temp, relay_state ? "ON" : "OFF", output,
             pid.params.Kp, pid.params.Ki);
    HAL_UART_Transmit(&huart1, (uint8_t*)buf, strlen(buf), 100);
}
