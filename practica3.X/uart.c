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

void UART_Init(void)
{
    /*
     * RC6 es TX del PIC18F4550.
     * RC7 es RX del PIC18F4550.
     */
    TRISCbits.TRISC6 = 0;   // TX como salida
    TRISCbits.TRISC7 = 1;   // RX como entrada

    /*
     * Limpia configuracion previa.
     */
    TXSTA = 0x00;
    RCSTA = 0x00;
    BAUDCON = 0x00;

    /*
     * Configuracion UART asincrona.
     */
    TXSTAbits.SYNC = 0;     // Modo asincrono
    TXSTAbits.BRGH = 1;     // Alta velocidad
    BAUDCONbits.BRG16 = 1;  // Generador de baudios de 16 bits

    /*
     * Carga valor para 9600 baudios.
     *
     * Si _XTAL_FREQ = 8000000:
     *   UART_SPBRG_VALUE = 207
     *
     * Si _XTAL_FREQ = 48000000:
     *   UART_SPBRG_VALUE = 1249
     */
    SPBRGH = (unsigned char)((UART_SPBRG_VALUE >> 8) & 0xFF);
    SPBRG  = (unsigned char)(UART_SPBRG_VALUE & 0xFF);

    /*
     * Habilita modulo serial.
     */
    RCSTAbits.SPEN = 1;     // Habilita UART en RC6/RC7
    TXSTAbits.TXEN = 1;     // Habilita transmision
    RCSTAbits.CREN = 1;     // Habilita recepcion
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

void UART_WriteUInt16(uint16_t value)
{
    char temp[6];
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