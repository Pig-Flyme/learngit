/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    usart.h
  * @brief   This file contains all the function prototypes for
  *          the usart.c file
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __USART_H__
#define __USART_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* USER CODE BEGIN Includes */
extern uint8_t tx_stir_flag ;
extern uint8_t tx_pump_flag ;
extern uint8_t tx_ph_flag ;
extern uint8_t tx_ox_flag ;
extern uint8_t rx_ox_flag ;
extern uint8_t rx_ph_flag ;
extern uint8_t rx_stir_flag ;
extern uint8_t rx_pump_flag ;
/* USER CODE END Includes */

extern UART_HandleTypeDef huart4;

extern UART_HandleTypeDef huart7;

extern UART_HandleTypeDef huart1;

extern UART_HandleTypeDef huart3;

extern UART_HandleTypeDef huart6;

/* USER CODE BEGIN Private defines */
#define RX_BUFFER_SIZE 512   // 缓冲区大小
extern uint8_t rx_data6[RX_BUFFER_SIZE];  // pump 缓冲区usart6
extern uint8_t rx_data3[RX_BUFFER_SIZE];  // ph 缓冲区usart3
extern uint8_t rx_data7[RX_BUFFER_SIZE];  // stir 缓冲区uart7
extern uint8_t rx_data4[RX_BUFFER_SIZE];  // oxygen 缓冲区uart4
extern volatile uint8_t tx_busy;
/* USER CODE END Private defines */

void MX_UART4_Init(void);
void MX_UART7_Init(void);
void MX_USART1_UART_Init(void);
void MX_USART3_UART_Init(void);
void MX_USART6_UART_Init(void);

/* USER CODE BEGIN Prototypes */

/* USER CODE END Prototypes */

#ifdef __cplusplus
}
#endif

#endif /* __USART_H__ */

