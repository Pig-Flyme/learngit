#include "ads8688.h"

extern SPI_HandleTypeDef hspi4;

// 写命令函数
void ADS8688_Write_Command(uint16_t com)
{
    uint8_t wr_data[2];
    wr_data[0] = (uint8_t)(com >> 8);
    wr_data[1] = (uint8_t)(com & 0xFF);

    ADS8688_CS_LOW();
    HAL_SPI_Transmit(&hspi4, wr_data, 2, HAL_MAX_DELAY);
    ADS8688_CS_HIGH();
}

// 写寄存器函数
void ADS8688_Write_Program(uint8_t addr, uint8_t data)
{
    uint8_t wr_data[2];
    wr_data[0] = (addr << 1) | 0x01;
    wr_data[1] = data;

    ADS8688_CS_LOW();
    HAL_SPI_Transmit(&hspi4, wr_data, 2, HAL_MAX_DELAY);
    ADS8688_CS_HIGH();
}

// 初始化函数（全部通道 0~10.24V）
void ADS8688_Init(void)
{
    ADS8688_DAISY_LOW();

    // 硬件复位
    ADS8688_RST_LOW();
    HAL_Delay(2);
    ADS8688_RST_HIGH();
    HAL_Delay(2);

    // 软件复位
    ADS8688_Write_Command(RST);
    HAL_Delay(2);

    // 所有通道：单极 0–10.24V
    ADS8688_Write_Program(CH0_INPUT_RANGE, VREF_U_25);
    ADS8688_Write_Program(CH1_INPUT_RANGE, VREF_U_25);
    ADS8688_Write_Program(CH2_INPUT_RANGE, VREF_U_25);
    ADS8688_Write_Program(CH3_INPUT_RANGE, VREF_U_25);
    ADS8688_Write_Program(CH4_INPUT_RANGE, VREF_U_25);
    ADS8688_Write_Program(CH5_INPUT_RANGE, VREF_U_25);
    ADS8688_Write_Program(CH6_INPUT_RANGE, VREF_U_25);
    ADS8688_Write_Program(CH7_INPUT_RANGE, VREF_U_25);

    // 启用全部通道
    ADS8688_Write_Program(CH_PWR_DN, 0x00);
    ADS8688_Write_Program(AUTO_SEQ_EN, 0xFF);

    // 选择通道 0 开始
    ADS8688_Write_Command(MAN_CH_0);

    printf("ADS8688 Initialized for 0–10.24V unipolar (4–20mA via 499R)\r\n");
}

// 读取单通道原始值
void Get_MAN_CH_Data(uint16_t ch, uint16_t *data)
{
    uint8_t Tx[4] = {0}, Rx[4] = {0};

    ADS8688_Write_Command(ch);
    for (volatile int i = 0; i < 10; i++);  // 简单延时

    ADS8688_CS_LOW();
    HAL_SPI_TransmitReceive(&hspi4, Tx, Rx, 4, HAL_MAX_DELAY);
    ADS8688_CS_HIGH();

    *data = ((uint16_t)Rx[2] << 8) | Rx[3];
}

// 扫描所有通道并换算为 mA 与 DO(%)
void ADS8688_ScanAllChannels(void)
{
    const uint16_t channels[8] = {
        MAN_CH_0, MAN_CH_1, MAN_CH_2, MAN_CH_3,
        MAN_CH_4, MAN_CH_5, MAN_CH_6, MAN_CH_7
    };
    const char *names[8] = {
        "CH0","CH1","CH2","CH3","CH4","CH5","CH6","CH7"
    };

    // ==== 校准参数 ====
    // 实测电压点：4 mA → 约 1.996 V，20 mA → 约 9.980 V
    const float V_ZERO = 1.996f;   // V @ 4 mA
    const float V_FULL = 9.980f;   // V @ 20 mA
    const float R_SHUNT = 499.0f;  // Ω 电阻
    const float V_FS = 10.24f;     // ADS8688 满量程电压 (V)
    const float ADC_FULL = 65536.0f;

    // 量程对应的溶氧上下限 (mg/L)
    const float DO_LRV = 0.0f;     // 4 mA 对应 0 mg/L
    const float DO_URV = 20.0f;    // 20 mA 对应 20 mg/L

    for (int i = 0; i < 8; i++)
    {
        uint16_t adc_data;
        Get_MAN_CH_Data(channels[i], &adc_data);

        // 1. ADC码 → 电压 (V)
        float voltage = (float)adc_data * V_FS / ADC_FULL;

        // 2. 电压 → 电流 (mA)，含两点校准补偿
        float current_mA = (voltage - V_ZERO) / (V_FULL - V_ZERO) * 16.0f + 4.0f;

        // 4. 线性换算成溶氧 mg/L
        float ratio = (current_mA - 4.0f) / 16.0f;
        if (ratio < 0.0f) ratio = 0.0f;
        if (ratio > 1.0f) ratio = 1.0f;
        float DO_mgL = ratio * (DO_URV - DO_LRV) + DO_LRV;

        // 5. 打印结果
        printf("%s: %7.3f V | %6.3f mA | DO=%6.2f mg/L | Raw=0x%04X\r\n",
               names[i], voltage, current_mA, DO_mgL, adc_data);
    }
}

