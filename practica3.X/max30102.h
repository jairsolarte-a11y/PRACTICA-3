#ifndef MAX30102_H
#define MAX30102_H

#include <xc.h>
#include <stdint.h>

/*
 * Direccion I2C de 7 bits del MAX30102.
 * En la comunicacion I2C se desplaza a la izquierda dentro de max30102.c.
 */
#define MAX30102_I2C_ADDRESS  0x57

uint8_t MAX30102_Init(void);

#endif