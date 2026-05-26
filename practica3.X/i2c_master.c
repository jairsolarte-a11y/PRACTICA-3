#include "i2c_master.h"

/*
   I2C hardware MSSP del PIC18F4550:

   SDA -> RB0 / pin fisico 33
   SCL -> RB1 / pin fisico 34

   Con Fosc = 8 MHz y SSPADD = 119:
   SCL aproximadamente = 16.6 kHz
*/

static void I2C_Master_Wait(void)
{
    while ((SSPCON2 & 0x1F) || SSPSTATbits.R_nW);
}

void I2C_Master_Init(uint32_t clock_hz)
{
    (void)clock_hz;

    TRISBbits.TRISB0 = 1;   // SDA
    TRISBbits.TRISB1 = 1;   // SCL

    SSPSTAT = 0x80;
    SSPCON1 = 0b00101000;
    SSPCON2 = 0x00;

    SSPADD = 119u;

    PIR1bits.SSPIF = 0;
}

void I2C_Master_Start(void)
{
    I2C_Master_Wait();

    SSPCON2bits.SEN = 1;

    while (SSPCON2bits.SEN);
}

void I2C_Master_Stop(void)
{
    I2C_Master_Wait();

    SSPCON2bits.PEN = 1;

    while (SSPCON2bits.PEN);
}

uint8_t I2C_Master_Write(uint8_t data)
{
    I2C_Master_Wait();

    PIR1bits.SSPIF = 0;
    SSPBUF = data;

    while (!PIR1bits.SSPIF);

    PIR1bits.SSPIF = 0;

    return SSPCON2bits.ACKSTAT;
}