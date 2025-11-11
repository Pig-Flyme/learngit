#include "ntc_control.h"
#include "pid.h"
#include "relay.h"
#include "temperature.h"
#include "usart.h"
#include <math.h>
#include <string.h>
#include "pt100.h"

// ==================== 参数定义 ====================
#define SETPOINT                37.0f
#define PRE_ADJUST_TOLERANCE    1.0f
#define PREHEAT_TIMEOUT         60000
#define AUTO_TUNE_DURATION      300000
#define FORCE_HEATING_THRESHOLD 36.0f

// ==================== 全局变量 ====================
PID_HandleTypeDef pid;
PID_Params_t pid_params = { .Kp = 2.0f, .Ki = 0.5f, .Kd = 0.1f };
AutoTuneHandle tune_handle;   // ← 兼容你的 pid.h 定义

TempStage_t temp_stage = TEMP_STAGE_PREHEAT;
static uint32_t preheat_start_time = 0;

// ==================== 初始化 ====================
void NTC_Control_Init(void)
{
    printf("\r\n=== [NTC] Temperature Control Init ===\r\n");
    printf("Target: %.2f°C | Preheat tolerance: ±%.2f°C\r\n",
           SETPOINT, PRE_ADJUST_TOLERANCE);

    temp_stage = TEMP_STAGE_PREHEAT;
    preheat_start_time = HAL_GetTick();
    Relay_Switch(1);
    printf("[NTC] Enter PREHEAT stage...\r\n");
}

// ==================== 主循环任务 ====================
void NTC_Control_Update(void)
{
    float temp = PT100_Task();
    uint32_t now = HAL_GetTick();

    // ---------- 阶段1：预热 ----------
    if (temp_stage == TEMP_STAGE_PREHEAT) {
        float diff = fabsf(temp - SETPOINT);

        if (diff <= PRE_ADJUST_TOLERANCE) {
            Relay_Switch(0);
            printf("[NTC] Preheat done (%.2f°C), start PID AutoTune...\r\n", temp);

            PID_Init(&pid, pid_params.Kp, pid_params.Ki, pid_params.Kd, -100, 100);
            PID_AutoTune_Init(&tune_handle, &pid_params,
            		PT100_Task, Relay_Switch,
                              SETPOINT, AUTO_TUNE_DURATION);

            temp_stage = TEMP_STAGE_AUTOTUNE;
            return;
        }

        if (now - preheat_start_time > PREHEAT_TIMEOUT) {
            Relay_Switch(0);
            printf("[NTC] Preheat timeout after %.1fs, skip autotune.\r\n",
                   PREHEAT_TIMEOUT / 1000.0f);

            PID_Init(&pid, pid_params.Kp, pid_params.Ki, pid_params.Kd, -100, 100);
            temp_stage = TEMP_STAGE_PID;
            return;
        }

        Relay_Switch(temp < SETPOINT ? 1 : 0);
        printf("[NTC] Preheating... %.2f°C\r\n", temp);
        return;
    }

    // ---------- 阶段2：自整定 ----------
    if (temp_stage == TEMP_STAGE_AUTOTUNE) {
        PID_AutoTune_Task(&tune_handle);

        if (tune_handle.state == TUNE_IDLE) {
            printf("[NTC] PID AutoTune complete.\r\n");
            printf("Tuned Params: Kp=%.3f Ki=%.3f Kd=%.3f\r\n",
                   pid_params.Kp, pid_params.Ki, pid_params.Kd);

            PID_UpdateParams(&pid, pid_params.Kp, pid_params.Ki, pid_params.Kd);
            temp_stage = TEMP_STAGE_PID;
        }
        return;
    }

    // ---------- 阶段3：正常PID控制 ----------
    if (temp_stage == TEMP_STAGE_PID) {
        float output = PID_Calculate(&pid, SETPOINT, temp,1.0f);  // ← 修正函数名

        if (temp < FORCE_HEATING_THRESHOLD || output > 0) {
            Relay_Switch(1);
        } else {
            Relay_Switch(0);
        }

        printf("[NTC] PID: T=%.2f°C | Out=%.2f | Kp=%.2f Ki=%.2f Kd=%.2f\r\n",
               temp, output, pid.params.Kp, pid.params.Ki, pid.params.Kd);
    }
}
