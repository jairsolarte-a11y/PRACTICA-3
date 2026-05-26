#ifndef ADC_H
#define ADC_H

#include "system.h"

void ADC_Init(void);
uint16_t ADC_Read(uint8_t channel);

#endif