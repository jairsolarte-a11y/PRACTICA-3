#ifndef I2C_MASTER_H
#define I2C_MASTER_H

#include "system.h"

void I2C_Master_Init(uint32_t clock_hz);
void I2C_Master_Start(void);
void I2C_Master_Stop(void);
uint8_t I2C_Master_Write(uint8_t data);

#endif