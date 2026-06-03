#include "system.h"
#include "uart.h"

#define UART_BAUD 9600UL

/*
 * Formula para UART asincrona:
 * BRGH = 1
 * BRG16 = 1
 * Baudrate = Fosc / (4 * (SPBRG + 1))
 */
#define UART_SPBRG_VALUE ((_XTAL_FREQ / (4UL * UART_BAUD)) - 1UL)

/*
 * Definicion de pines UART
 */
#define UART_TX_PIN     TRISCbits.TRISC6
#define UART_RX_PIN     TRISCbits.TRISC7

#define UART_TX_OUTPUT  0
#define UART_RX_INPUT   1

void UART_Init(void)
{
    /*
     * RC6 = TX
     * RC7 = RX
     */
    UART_TX_PIN = UART_TX_OUTPUT;
    UART_RX_PIN = UART_RX_INPUT;

    /*
     * Limpia configuracion previa.
     */
    TXSTA = 0x00;
    RCSTA = 0x00;
    BAUDCON = 0x00;

    /*
     * Configuracion UART asincrona.
     */
    TXSTAbits.SYNC = 0;
    TXSTAbits.BRGH = 1;
    BAUDCONbits.BRG16 = 1;

    /*
     * Configuracion de velocidad.
     */
    SPBRGH = (unsigned char)((UART_SPBRG_VALUE >> 8) & 0xFF);
    SPBRG  = (unsigned char)(UART_SPBRG_VALUE & 0xFF);

    /*
     * Habilita modulo UART.
     */
    RCSTAbits.SPEN = 1;
    TXSTAbits.TXEN = 1;
    RCSTAbits.CREN = 1;
}

void UART_WriteChar(char data)
{
    while (!PIR1bits.TXIF)
    {
        ;
    }

    TXREG = data;
}

void UART_WriteString(const char *text)
{
    while (*text != '\0')
    {
        UART_WriteChar(*text);
        text++;
    }
}

void UART_WriteLine(const char *text)
{
    UART_WriteString(text);
    UART_WriteString("\r\n");
}

/*
 * Convierte un entero de 16 bits a texto
 * y lo transmite por UART.
 */
void UART_WriteUInt16(uint16_t value)
{
    char temp[6];   /* 65535 + terminador */
    uint8_t i = 0;

    if (value == 0)
    {
        UART_WriteChar('0');
        return;
    }

    while (value > 0)
    {
        temp[i] = (char)('0' + (value % 10));
        value /= 10;
        i++;
    }

    while (i > 0)
    {
        i--;
        UART_WriteChar(temp[i]);
    }
} 