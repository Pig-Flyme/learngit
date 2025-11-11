/*
 * pt100.h
 *
 *  Created on: Nov 5, 2025
 *      Author: Tang
 */

#ifndef INC_PT100_H_
#define INC_PT100_H_


#include "usart.h"

extern uint8_t rx_data8[RX_BUFFER_SIZE];    // uart7接收缓存


// 温度数据
extern float pt100_temp;

// 函数声明
void PT100_Init(void);
void PT100_GetTemperature(void);
void PT100_ReadTemperature(void);
float PT100_Task(void);
uint16_t PT100_CRC16(uint8_t *data, uint16_t len);

#endif /* INC_PT100_H_ */
