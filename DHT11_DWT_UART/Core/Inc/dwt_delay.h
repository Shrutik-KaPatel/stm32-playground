#ifndef DWT_DELAY_H
#define DWT_DELAY_H

#include "stm32f4xx.h"

void DWT_Init(void);
void delay_us(uint32_t us);

#endif
