/*
 * i2c_master.c
 * Driver I2C maestro para PIC18F4550
 *
 * Pines I2C en PIC18F4550:
 *   SDA -> RB0
 *   SCL -> RB1
 */

#include "i2c_master.h"

static void I2C_Master_Wait(void)
{
    while ((SSPCON2 & 0x1F) || (SSPSTAT & 0x04))
    {
        ;
    }
}

void I2C_Master_Init(uint32_t clock_hz)
{
    /*
     * RB0 = SDA
     * RB1 = SCL
     *
     * En modo I2C estos pines deben quedar como entrada,
     * porque el modulo MSSP del PIC los controla.
     */
    TRISBbits.TRISB0 = 1;
    TRISBbits.TRISB1 = 1;

    SSPCON1 = 0x00;
    SSPCON2 = 0x00;
    SSPSTAT = 0x00;

    /*
     * SSPCON1 = 0x28:
     * SSPEN = 1
     * SSPM = 1000, modo I2C maestro
     */
    SSPCON1 = 0x28;

    /*
     * Slew rate desactivado para I2C estandar de 100 kHz.
     */
    SSPSTATbits.SMP = 1;

    /*
     * Formula:
     * SSPADD = (Fosc / (4 * Fscl)) - 1
     */
    SSPADD = (uint8_t)((_XTAL_FREQ / (4UL * clock_hz)) - 1UL);
}

void I2C_Master_Start(void)
{
    I2C_Master_Wait();
    SSPCON2bits.SEN = 1;

    while (SSPCON2bits.SEN)
    {
        ;
    }
}

void I2C_Master_RepeatedStart(void)
{
    I2C_Master_Wait();
    SSPCON2bits.RSEN = 1;

    while (SSPCON2bits.RSEN)
    {
        ;
    }
}

void I2C_Master_Stop(void)
{
    I2C_Master_Wait();
    SSPCON2bits.PEN = 1;

    while (SSPCON2bits.PEN)
    {
        ;
    }
}

uint8_t I2C_Master_Write(uint8_t data)
{
    I2C_Master_Wait();

    SSPBUF = data;

    while (!PIR1bits.SSPIF)
    {
        ;
    }

    PIR1bits.SSPIF = 0;

    /*
     * ACKSTAT:
     * 0 = el esclavo respondio
     * 1 = el esclavo no respondio
     */
    return SSPCON2bits.ACKSTAT;
}

uint8_t I2C_Master_Read(uint8_t ack)
{
    uint8_t data;

    I2C_Master_Wait();

    SSPCON2bits.RCEN = 1;

    while (!SSPSTATbits.BF)
    {
        ;
    }

    data = SSPBUF;

    I2C_Master_Wait();

    if (ack)
    {
        SSPCON2bits.ACKDT = 0;
    }
    else
    {
        SSPCON2bits.ACKDT = 1;
    }

    SSPCON2bits.ACKEN = 1;

    while (SSPCON2bits.ACKEN)
    {
        ;
    }

    return data;
}