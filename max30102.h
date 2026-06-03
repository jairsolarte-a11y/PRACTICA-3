#ifndef MAX30102_H
#define MAX30102_H

#include <xc.h>
#include <stdint.h>

#define MAX30102_I2C_ADDRESS  0x57

uint8_t MAX30102_Init(void);
uint8_t MAX30102_ReadFIFO(uint32_t *red_value, uint32_t *ir_value);
uint8_t MAX30102_ProcessHeartRate(uint32_t ir_value, uint16_t *bpm);
void MAX30102_ResetHeartRateAlgorithm(void);

#endif