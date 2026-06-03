/*
 * i2c_master.c
 * Driver I2C Maestro para PIC18F4550
 *
 * Pines I2C:
 *   SDA -> RB0
 *   SCL -> RB1
 *
 * Compilador: XC8
 */

#include "i2c_master.h"

/*
 * ==========================================================
 * Constantes MSSP
 * ==========================================================
 */
#define I2C_MASTER_MODE_ENABLE    0x28

#define I2C_MSSP_BUSY_MASK        0x1F
#define I2C_R_W_STATUS_MASK       0x04

/*
 * ==========================================================
 * Funciones internas
 * ==========================================================
 */

/*
 * Espera hasta que el módulo MSSP finalice
 * cualquier operación I2C pendiente.
 */
static void I2C_Master_Wait(void)
{
    while ((SSPCON2 & I2C_MSSP_BUSY_MASK) ||
           (SSPSTAT & I2C_R_W_STATUS_MASK))
    {
        ;
    }
}

/*
 * ==========================================================
 * Inicialización del módulo I2C
 * ==========================================================
 */
void I2C_Master_Init(uint32_t clock_hz)
{
    uint32_t baud_rate_divisor;

    /*
     * RB0 = SDA
     * RB1 = SCL
     *
     * En modo I2C estos pines deben permanecer
     * configurados como entrada ya que el MSSP
     * toma el control de las líneas.
     */
    TRISBbits.TRISB0 = 1;
    TRISBbits.TRISB1 = 1;

    /*
     * Limpieza inicial de registros MSSP.
     */
    SSPCON1 = 0x00;
    SSPCON2 = 0x00;
    SSPSTAT = 0x00;

    /*
     * Configuración:
     * SSPEN = 1
     * SSPM<3:0> = 1000
     *
     * I2C Master Mode
     */
    SSPCON1 = I2C_MASTER_MODE_ENABLE;

    /*
     * Slew Rate deshabilitado
     * para operación estándar a 100 kHz.
     */
    SSPSTATbits.SMP = 1;

    /*
     * Fórmula:
     *
     * SSPADD = (Fosc / (4 * Fscl)) - 1
     */
    baud_rate_divisor =
        (_XTAL_FREQ / (4UL * clock_hz)) - 1UL;

    SSPADD = (uint8_t)baud_rate_divisor;
}

/*
 * ==========================================================
 * Condiciones del bus I2C
 * ==========================================================
 */

/*
 * Genera condición START.
 */
void I2C_Master_Start(void)
{
    I2C_Master_Wait();

    SSPCON2bits.SEN = 1;

    while (SSPCON2bits.SEN)
    {
        ;
    }
}

/*
 * Genera condición REPEATED START.
 */
void I2C_Master_RepeatedStart(void)
{
    I2C_Master_Wait();

    SSPCON2bits.RSEN = 1;

    while (SSPCON2bits.RSEN)
    {
        ;
    }
}

/*
 * Genera condición STOP.
 */
void I2C_Master_Stop(void)
{
    I2C_Master_Wait();

    SSPCON2bits.PEN = 1;

    while (SSPCON2bits.PEN)
    {
        ;
    }
}

/*
 * ==========================================================
 * Escritura de datos
 * ==========================================================
 */

/*
 * Envía un byte al bus I2C.
 *
 * Retorna:
 * 0 -> ACK recibido
 * 1 -> NACK recibido
 */
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
     * 0 = ACK recibido
     * 1 = NACK recibido
     */
    return SSPCON2bits.ACKSTAT;
}

/*
 * ==========================================================
 * Lectura de datos
 * ==========================================================
 */

/*
 * Lee un byte desde el bus I2C.
 *
 * Parámetro:
 * ack = 1 -> enviar ACK
 * ack = 0 -> enviar NACK
 *
 * Retorna:
 * Byte recibido.
 */
uint8_t I2C_Master_Read(uint8_t ack)
{
    uint8_t data;

    I2C_Master_Wait();

    /*
     * Habilita recepción.
     */
    SSPCON2bits.RCEN = 1;

    while (!SSPSTATbits.BF)
    {
        ;
    }

    data = SSPBUF;

    I2C_Master_Wait();

    /*
     * ACKDT:
     * 0 -> ACK
     * 1 -> NACK
     */
    if (ack)
    {
        SSPCON2bits.ACKDT = 0;
    }
    else
    {
        SSPCON2bits.ACKDT = 1;
    }

    /*
     * Envía ACK/NACK.
     */
    SSPCON2bits.ACKEN = 1;

    while (SSPCON2bits.ACKEN)
    {
        ;
    }

    return data;
}