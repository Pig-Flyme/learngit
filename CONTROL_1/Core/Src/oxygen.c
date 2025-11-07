#include "oxygen.h"

 uint8_t rx_data4[RX_BUFFER_SIZE];    // uart4接收缓存
 uint8_t tx_ox_flag = 0;    // uart4发送完成标志
 uint8_t rx_ox_flag = 0;
 uint8_t current_device = 0;  // 0设置为溶氧设备，1设置为尾气设备

 // 把Modbus返回的4字节转float (ABCD顺序大端)
 static float ModbusBytesToFloat(uint8_t *data)
 {
     uint32_t temp = ((uint32_t)data[0] << 24) |
                     ((uint32_t)data[1] << 16) |
                     ((uint32_t)data[2] << 8)  |
                     ((uint32_t)data[3]);
     float value;
     memcpy(&value, &temp, 4);
     return value;
 }

 //baud rate 9600
//溶氧校准，1. 将电极放在空气中，等待其稳定约180秒左右，（请勿将溶氧膜头在阳光下直射)
//2. 向电极发送空气校准指令0A  06  00  1A  00  01  68  B6
   void Oxygen_Init(){
	   uint8_t sendBuffer[] = {0x0A, 0x06, 0x00, 0x1A, 0x00, 0x01, 0x68, 0xB6};
	  HAL_UART_Transmit_DMA(&huart4, sendBuffer, sizeof(sendBuffer));
	  while (tx_ox_flag == 0) {}  // 等待发送完成
	           // 开启接收
	       HAL_UARTEx_ReceiveToIdle_DMA(&huart4, rx_data4, sizeof(rx_data4));
	       __HAL_DMA_DISABLE_IT(huart4.hdmarx, DMA_IT_HT);
}
//读2个寄存器，回复4位16进制浮点数转换
   void Oxygen_Get() {
	   // 清空接收缓存
	   memset(rx_data4, 0, sizeof(rx_data4));
       uint8_t sendBuffer[] = {0x0A, 0x03, 0x00, 0x00, 0x00, 0x02, 0xC5, 0x70};
       tx_ox_flag = 0;
       HAL_UART_Transmit_DMA(&huart4, sendBuffer, sizeof(sendBuffer));
       // 启动接收
       HAL_UARTEx_ReceiveToIdle_DMA(&huart4, rx_data4, sizeof(rx_data4));
   }

   void Oxygen_Read(void) {
	   // 调试打印：查看当前接收缓冲区的实际数据
	       printf("ox dump: ");
	       for (uint16_t k = 0; k < 40; k++) {  // 打印前40字节够用了
	           printf("%02X ", rx_data4[k]);
	       }
	       printf("\n");
	   float oxygen = ModbusBytesToFloat(&rx_data4[3]);
	    printf("Oxygen=%.2f\r\n", oxygen);
   }

   void Oxygen_Task(void) {
//       static uint8_t initialized = 0;
//
//       if (!initialized) {
//           Oxygen_Init();
//           initialized = 1;
//           return;  // 首次初始化后直接返回
//       }

       // 正常读取流程
       Oxygen_Get();

       // 添加接收等待逻辑（非阻塞方式）
       uint32_t start = HAL_GetTick();
       while (!rx_ox_flag && (HAL_GetTick() - start < 100)) {
           // 等待100ms或直到接收完成
       }

       if (rx_ox_flag) {
           Oxygen_Read();
           rx_ox_flag = 0;  // 清除接收标志

           // 重新启动下一次接收
           HAL_UARTEx_ReceiveToIdle_DMA(&huart4, rx_data4, sizeof(rx_data4));
       }
   }

void Send_Task(void){
	if(current_device==0){
		Oxygen_Task();
		current_device=1;
	}
	else{
		Endgas_Task();
		current_device=0;
	}
}

