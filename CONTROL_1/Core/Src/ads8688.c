#include "ads8688.h"

extern SPI_HandleTypeDef hspi4;  // 使用正确的SPI句柄



// 写命令函数
void ADS8688_Write_Command(uint16_t com)
{
    uint8_t wr_data[2] = {0x00, 0x00};

    wr_data[0] = (uint8_t)(com >> 8);
    wr_data[1] = (uint8_t)(com & 0x00FF);

    ADS8688_CS_LOW();
    HAL_SPI_Transmit(&hspi4, wr_data, 2, HAL_MAX_DELAY);
    ADS8688_CS_HIGH();
}

// 写寄存器函数
void ADS8688_Write_Program(uint8_t addr, uint8_t data)
{
    uint8_t wr_data[2] = {0x00, 0x00};

    wr_data[0] = (addr << 1) | 0x01;
    wr_data[1] = data;

    ADS8688_CS_LOW();
    HAL_SPI_Transmit(&hspi4, wr_data, 2, HAL_MAX_DELAY);
    ADS8688_CS_HIGH();
}

// 初始化函数
void ADS8688_Init(void)
{
    // GPIO配置已经在CubeMX中完成，这里只操作引脚

    ADS8688_DAISY_LOW();

    // 硬件复位
    ADS8688_RST_LOW();
    HAL_Delay(2);  // 2ms延时
    ADS8688_RST_HIGH();
    HAL_Delay(2);  // 2ms延时

    // 软件复位
    ADS8688_Write_Command(RST);
    HAL_Delay(2);  // 2ms延时

    // 设置所有通道输入范围 ±5.12V
    ADS8688_Write_Program(CH0_INPUT_RANGE, VREF_B_125);
    ADS8688_Write_Program(CH1_INPUT_RANGE, VREF_B_125);
    ADS8688_Write_Program(CH2_INPUT_RANGE, VREF_B_125);
    ADS8688_Write_Program(CH3_INPUT_RANGE, VREF_B_125);
    ADS8688_Write_Program(CH4_INPUT_RANGE, VREF_B_125);
    ADS8688_Write_Program(CH5_INPUT_RANGE, VREF_B_125);
    ADS8688_Write_Program(CH6_INPUT_RANGE, VREF_B_125);
    ADS8688_Write_Program(CH7_INPUT_RANGE, VREF_B_125);

    // 通道配置
    ADS8688_Write_Program(CH_PWR_DN, 0x00);      // 所有通道退出低功耗
    ADS8688_Write_Program(AUTO_SEQ_EN, 0xFF);    // 使能自动扫描

    // 选择通道0开始
    ADS8688_Write_Command(MAN_CH_0);

    printf("ADS8688 Initialized with ±5.12V range\r\n");
}

// 读取通道数据
void Get_MAN_CH_Data(uint16_t ch, uint16_t *data)
{
    uint8_t Rxdata[4];
    uint8_t wr_data[4] = {0x00, 0x00, 0x00, 0x00};

    ADS8688_Write_Command(ch);

    // 添加小延时
    for(volatile int i=0; i<10; i++);

    ADS8688_CS_LOW();
    HAL_SPI_TransmitReceive(&hspi4, (uint8_t *)wr_data, (uint8_t *)Rxdata, 4, HAL_MAX_DELAY);
    ADS8688_CS_HIGH();

    // 使用第3和第4字节作为ADC数据
    *data = ((uint16_t)Rxdata[2] << 8) | Rxdata[3];
}

// 扫描并打印所有8个通道
void ADS8688_ScanAllChannels(void)
{
    uint16_t channels[] = {
        MAN_CH_0, MAN_CH_1, MAN_CH_2, MAN_CH_3,
        MAN_CH_4, MAN_CH_5, MAN_CH_6, MAN_CH_7
    };

    const char* channel_names[] = {
        "CH0", "CH1", "CH2", "CH3",
        "CH4", "CH5", "CH6", "CH7"
    };

    for (int i = 0; i < 8; i++) {
        uint16_t adc_data;
        double voltage_mV;

        Get_MAN_CH_Data(channels[i], &adc_data);

        // 使用与Keil版本相同的电压计算公式
        voltage_mV = ((double)adc_data - 32768) * 10240.0 / 65536;

        // 与Keil版本相同的输出格式
        printf("%s: %10.4lfmV  D: %04X\r\n", channel_names[i], voltage_mV, adc_data);
    }
}
