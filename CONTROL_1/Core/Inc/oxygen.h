//溶氧
#ifndef INC_OXYGEN_H_
#define INC_OXYGEN_H_

#include "usart.h"
#include <string.h>
#include <stdio.h>
#include "endgas.h"

  void Oxygen_Init(void);
  void Oxygen_Get(void);
  void Oxygen_Read(void);
  void Oxygen_Task(void);
  void Send_Task(void);


#endif /* INC_OXYGEN_H_ */
