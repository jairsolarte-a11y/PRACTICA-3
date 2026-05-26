#ifndef SSD1306_H
#define SSD1306_H

#include "system.h"

/*
   Direccion I2C de 8 bits para escritura.

   Si tu pantalla usa 0x78, deja esta linea activa.
   Si tu pantalla usa 0x7A, comenta 0x78 y activa 0x7A.

   0x78 equivale a direccion 7 bits 0x3C.
   0x7A equivale a direccion 7 bits 0x3D.
*/

#define SSD1306_I2C_ADDRESS_WRITE  0x78
// #define SSD1306_I2C_ADDRESS_WRITE  0x7A

void SSD1306_Init(void);
void SSD1306_ClearDisplay(void);
void SSD1306_ClearLine(uint8_t page);
void SSD1306_SetCursor(uint8_t column, uint8_t page);
void SSD1306_WriteChar(char c);
void SSD1306_WriteString(const char *str);

#endif