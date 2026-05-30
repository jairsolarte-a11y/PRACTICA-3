#include "system.h"
#include "i2c_master.h"
#include "max30102.h"

/*
 * Registros del MAX30102.
 */
#define MAX30102_REG_INT_STATUS_1      0x00
#define MAX30102_REG_INT_STATUS_2      0x01
#define MAX30102_REG_FIFO_WR_PTR       0x04
#define MAX30102_REG_OVF_COUNTER       0x05
#define MAX30102_REG_FIFO_RD_PTR       0x06
#define MAX30102_REG_FIFO_DATA         0x07
#define MAX30102_REG_FIFO_CONFIG       0x08
#define MAX30102_REG_MODE_CONFIG       0x09
#define MAX30102_REG_SPO2_CONFIG       0x0A
#define MAX30102_REG_LED1_PA           0x0C
#define MAX30102_REG_LED2_PA           0x0D
#define MAX30102_REG_PART_ID           0xFF

#define MAX30102_PART_ID_VALUE         0x15

/*
 * Funcion interna:
 * Escribe un valor en un registro del MAX30102.
 */
static void MAX30102_WriteRegister(uint8_t reg, uint8_t value)
{
    I2C_Master_Start();
    I2C_Master_Write((MAX30102_I2C_ADDRESS << 1) | 0);
    I2C_Master_Write(reg);
    I2C_Master_Write(value);
    I2C_Master_Stop();
}

/*
 * Funcion interna:
 * Lee un registro del MAX30102.
 */
static uint8_t MAX30102_ReadRegister(uint8_t reg)
{
    uint8_t value;

    I2C_Master_Start();
    I2C_Master_Write((MAX30102_I2C_ADDRESS << 1) | 0);
    I2C_Master_Write(reg);

    I2C_Master_RepeatedStart();
    I2C_Master_Write((MAX30102_I2C_ADDRESS << 1) | 1);

    value = I2C_Master_Read(0);

    I2C_Master_Stop();

    return value;
}

/*
 * Funcion interna:
 * Lee varios bytes consecutivos desde un registro del MAX30102.
 *
 * Esta funcion es necesaria para leer el FIFO, porque el sensor entrega
 * 6 bytes por muestra:
 *
 * RED: byte 0, byte 1, byte 2
 * IR:  byte 3, byte 4, byte 5
 */
static void MAX30102_ReadMulti(uint8_t reg, uint8_t *buffer, uint8_t length)
{
    uint8_t i;

    I2C_Master_Start();
    I2C_Master_Write((MAX30102_I2C_ADDRESS << 1) | 0);
    I2C_Master_Write(reg);

    I2C_Master_RepeatedStart();
    I2C_Master_Write((MAX30102_I2C_ADDRESS << 1) | 1);

    for (i = 0; i < length; i++)
    {
        if (i < (length - 1))
        {
            buffer[i] = I2C_Master_Read(1);
        }
        else
        {
            buffer[i] = I2C_Master_Read(0);
        }
    }

    I2C_Master_Stop();
}

/*
 * Funcion: MAX30102_Init
 *
 * Objetivo fisico:
 * Verificar que el sensor MAX30102 esta conectado al bus I2C
 * y configurarlo en modo SpO2 para activar los canales RED e IR.
 *
 * Retorno:
 * 1 = sensor detectado y configurado.
 * 0 = sensor no detectado.
 */
uint8_t MAX30102_Init(void)
{
    uint8_t part_id;

    /*
     * Lee el registro PART_ID.
     * Si el valor leido es 0x15, el sensor MAX30102 fue detectado.
     */
    part_id = MAX30102_ReadRegister(MAX30102_REG_PART_ID);

    if (part_id != MAX30102_PART_ID_VALUE)
    {
        return 0;
    }

    /*
     * Reset interno del sensor.
     */
    MAX30102_WriteRegister(MAX30102_REG_MODE_CONFIG, 0x40);
    __delay_ms(100);

    /*
     * Limpia los punteros del FIFO.
     */
    MAX30102_WriteRegister(MAX30102_REG_FIFO_WR_PTR, 0x00);
    MAX30102_WriteRegister(MAX30102_REG_OVF_COUNTER, 0x00);
    MAX30102_WriteRegister(MAX30102_REG_FIFO_RD_PTR, 0x00);

    /*
     * FIFO_CONFIG = 0x1F:
     * - Promedio de muestras = 1.
     * - FIFO rollover habilitado.
     * - FIFO almost full = 15.
     */
    MAX30102_WriteRegister(MAX30102_REG_FIFO_CONFIG, 0x1F);

    /*
     * MODE_CONFIG = 0x03:
     * Modo SpO2, activa RED + IR.
     */
    MAX30102_WriteRegister(MAX30102_REG_MODE_CONFIG, 0x03);

    /*
     * SPO2_CONFIG = 0x27:
     * - ADC range = 4096 nA.
     * - Sample rate = 100 Hz.
     * - Pulse width = 411 us.
     */
    MAX30102_WriteRegister(MAX30102_REG_SPO2_CONFIG, 0x27);

    /*
     * Corriente de los LEDs internos del sensor.
     * LED1 = RED.
     * LED2 = IR.
     */
    MAX30102_WriteRegister(MAX30102_REG_LED1_PA, 0x24);
    MAX30102_WriteRegister(MAX30102_REG_LED2_PA, 0x24);

    /*
     * Limpia banderas de interrupcion leyendo sus registros.
     */
    (void)MAX30102_ReadRegister(MAX30102_REG_INT_STATUS_1);
    (void)MAX30102_ReadRegister(MAX30102_REG_INT_STATUS_2);

    return 1;
}

/*
 * Funcion: MAX30102_ReadFIFO
 *
 * Objetivo fisico:
 * Leer una muestra real del sensor MAX30102.
 *
 * El sensor entrega 6 bytes:
 * - 3 bytes para RED.
 * - 3 bytes para IR.
 *
 * Cada valor es de 18 bits, por eso se aplica la mascara 0x03FFFF.
 *
 * Retorno:
 * 1 = lectura realizada.
 */
uint8_t MAX30102_ReadFIFO(uint32_t *red_value, uint32_t *ir_value)
{
    uint8_t data[6];

    MAX30102_ReadMulti(MAX30102_REG_FIFO_DATA, data, 6);

    *red_value = (((uint32_t)data[0] << 16) |
                  ((uint32_t)data[1] << 8)  |
                  ((uint32_t)data[2]));

    *ir_value = (((uint32_t)data[3] << 16) |
                 ((uint32_t)data[4] << 8)  |
                 ((uint32_t)data[5]));

    /*
     * El MAX30102 entrega datos de 18 bits.
     * Se limpian los bits superiores que no pertenecen a la medicion.
     */
    *red_value &= 0x03FFFF;
    *ir_value  &= 0x03FFFF;

    return 1;
}