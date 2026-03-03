#include "stir.h"  //uart7白A黑B


uint8_t rx_data7[RX_BUFFER_SIZE];    // uart7接收缓存
 uint8_t tx_stir_flag = 0;    // uart7发送完成标志
 uint8_t rx_stir_flag = 0;


 //发送使能信号
 void Get_Sign(){
	 uint8_t sendBuffer[] = {0x01, 0x06, 0x00, 0x00, 0x00, 0x01, 0x48, 0x0A};
	 tx_stir_flag = 0;
     HAL_UART_Transmit_DMA(&huart7, sendBuffer, sizeof(sendBuffer));
     while (tx_stir_flag == 0) {}  // 等待发送完成
         // 开启接收
     HAL_UARTEx_ReceiveToIdle_DMA(&huart7, rx_data7, sizeof(rx_data7));
     __HAL_DMA_DISABLE_IT(huart7.hdmarx, DMA_IT_HT);

 }
 //启动速度模式
 void SpeedMode(){
 	uint8_t sendBuffer[] = {0x01, 0x06, 0x00, 0x19, 0x00, 0x03, 0x18, 0x0C};
 	tx_stir_flag = 0;
 	HAL_UART_Transmit_DMA(&huart7, sendBuffer, sizeof(sendBuffer));
 	while (tx_stir_flag == 0) {}  // 等待发送完成
 	         // 开启接收
 	         HAL_UARTEx_ReceiveToIdle_DMA(&huart7, rx_data7, sizeof(rx_data7));
 	         __HAL_DMA_DISABLE_IT(huart7.hdmarx, DMA_IT_HT);

 }
 /**
   * @brief  直接计算 Modbus CRC16（简洁版）
   * @param  pdata: 数据指针
   * @param  len:   数据长度
   * @retval CRC16 值（低字节在前，高字节在后）
   */
 uint16_t CRC16(uint8_t *pdata, uint16_t len)
 {
     uint16_t crc = 0xFFFF;
     while (len--)
     {
         crc ^= *pdata++;          // 异或新字节
         for (uint8_t i = 0; i < 8; i++)
         {
             if (crc & 1)           // 检查最低位
                 crc = (crc >> 1) ^ 0xA001;  // 多项式 0x8005 的反射形式
             else
                 crc >>= 1;
         }
     }
     return crc;
 }/**
  * @brief  设置搅拌电机转速（带方向）
  * @param  speed: 转速值，范围 -1500 ~ 1500
  *               正值为逆时针，负值为顺时针
  * @retval None
  */
//设置速度
 void Set_Stir_Speed(int16_t speed)
 {
     // 限制输入范围
     if (speed > 1500) speed = 1500;
     if (speed < -1500) speed = -1500;

     uint8_t sendBuffer[8];
     sendBuffer[0] = 0x01;                     // 设备地址
     sendBuffer[1] = 0x06;                     // 功能码：写单个寄存器
     sendBuffer[2] = 0x00;                     // 寄存器地址高字节 (0x02)
     sendBuffer[3] = 0x02;                     // 寄存器地址低字节
     sendBuffer[4] = (uint8_t)((speed >> 8) & 0xFF); // 数据高字节
     sendBuffer[5] = (uint8_t)(speed & 0xFF);        // 数据低字节

     uint16_t crc = CRC16(sendBuffer, 6);
     sendBuffer[6] = crc & 0xFF;
     sendBuffer[7] = (crc >> 8) & 0xFF;

     tx_stir_flag = 0;
     HAL_UART_Transmit_DMA(&huart7, sendBuffer, 8);
     while (tx_stir_flag == 0) {}  // 等待发送完成

     // 重新开启接收（保持与现有代码一致）
     HAL_UARTEx_ReceiveToIdle_DMA(&huart7, rx_data7, sizeof(rx_data7));
     __HAL_DMA_DISABLE_IT(huart7.hdmarx, DMA_IT_HT);
 }
 /**
  * @brief  解析搅拌器二进制命令并执行
  * @param  buffer: 接收到的数据缓冲区
  * @param  size:   数据长度
  * @retval None
  */
 void Process_Stir_Command(uint8_t *buffer, uint16_t size)
 {
     // 检查帧长度至少8字节，且帧头为0xAA
     if (size < 8 || buffer[0] != 0xAA)
     {
         return; // 无效帧
     }

     // 计算CRC（校验范围：字节1~5，即地址+功能码+子功能+速度高+速度低）
     uint16_t calc_crc = ModbusCRC16(&buffer[1], 5);
     uint16_t recv_crc = buffer[6] | (buffer[7] << 8);

     if (calc_crc != recv_crc)
     {
         return; // CRC错误
     }

     // 检查地址、功能码、子功能
     if (buffer[1] == 0x01 &&          // 设备地址
         buffer[2] == 0x06 &&          // 功能码：设置搅拌
         buffer[3] == 0x01)            // 子功能：速度
     {
         // 提取速度（有符号16位，高字节在前）
         int16_t speed = (buffer[4] << 8) | buffer[5];
         Set_Stir_Speed(speed);

         // 回发相同帧作为确认（阻塞发送，超时100ms）
         HAL_UART_Transmit(&huart1, buffer, 8, 100);
     }
 }
// //转速100
//void Start_Stir(){
//	uint8_t sendBuffer[] = {0x01, 0x06, 0x00, 0x02, 0x00, 0x64, 0x29, 0xE1};
//	tx_stir_flag = 0;
//	HAL_UART_Transmit_DMA(&huart7, sendBuffer, sizeof(sendBuffer));
//	while (tx_stir_flag == 0) {}  // 等待发送完成
//	         // 开启接收
//	         HAL_UARTEx_ReceiveToIdle_DMA(&huart7, rx_data7, sizeof(rx_data7));
//	         __HAL_DMA_DISABLE_IT(huart7.hdmarx, DMA_IT_HT);
//
//}
////转速0
//void Stop_Stir(){
//	uint8_t sendBuffer[] = {0x01, 0x06, 0x00, 0x02, 0x00, 0x00, 0x28, 0x0A};
//	tx_stir_flag = 0;
//	HAL_UART_Transmit_DMA(&huart7, sendBuffer, sizeof(sendBuffer));
//	while (tx_stir_flag == 0) {}  // 等待发送完成
//	         // 开启接收
//	         HAL_UARTEx_ReceiveToIdle_DMA(&huart7, rx_data7, sizeof(rx_data7));
//	         __HAL_DMA_DISABLE_IT(huart7.hdmarx, DMA_IT_HT);
//}


