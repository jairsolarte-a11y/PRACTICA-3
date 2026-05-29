#ifndef UART_H
#define UART_H

#include <xc.h>
#include <stdint.h>

void UART_Init(void);
void UART_WriteChar(char data);
void UART_WriteString(const char *text);
void UART_WriteLine(const char *text);
void UART_WriteUInt16(uint16_t value);

#endif