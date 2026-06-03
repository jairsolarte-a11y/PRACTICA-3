#include "system.h"
#include "ds18b20.h"
 
/*
 * ==========================================================
 * Comandos del sensor DS18B20
 * ==========================================================
 */
#define DS18B20_CMD_SKIP_ROM       0xCC
#define DS18B20_CMD_CONVERT_T      0x44
#define DS18B20_CMD_READ_SCRATCH   0xBE

/*
 * ==========================================================
 * Temporizaciones del protocolo 1-Wire (microsegundos)
 * ==========================================================
 */
#define ONEWIRE_RESET_LOW_US         480
#define ONEWIRE_PRESENCE_WAIT_US      70
#define ONEWIRE_RECOVERY_US          410

#define ONEWIRE_WRITE1_LOW_US          6
#define ONEWIRE_WRITE1_RELEASE_US     64

#define ONEWIRE_WRITE0_LOW_US         60
#define ONEWIRE_WRITE0_RELEASE_US     10

#define ONEWIRE_READ_INIT_US           6
#define ONEWIRE_READ_SAMPLE_US         9
#define ONEWIRE_READ_RECOVERY_US      55

/*
 * ==========================================================
 * Funciones internas del protocolo 1-Wire
 * ==========================================================
 */

/*
 * Fuerza la línea de datos a nivel bajo.
 */
static void OneWire_Low(void)
{
    DS18B20_LAT = 0;
    DS18B20_TRIS = 0;
}

/*
 * Libera la línea de datos para permitir
 * que la resistencia pull-up la lleve a nivel alto.
 */
static void OneWire_Release(void)
{
    DS18B20_TRIS = 1;
}

/*
 * Lee el estado actual del bus 1-Wire.
 */
static uint8_t OneWire_ReadPin(void)
{
    return DS18B20_PORT;
}

/*
 * Genera la secuencia RESET del protocolo 1-Wire.
 *
 * Retorna:
 * 1 -> Sensor detectado
 * 0 -> Sensor no detectado
 */
static uint8_t OneWire_Reset(void)
{
    uint8_t presence_detected;

    OneWire_Low();
    __delay_us(ONEWIRE_RESET_LOW_US);

    OneWire_Release();
    __delay_us(ONEWIRE_PRESENCE_WAIT_US);

    presence_detected = (OneWire_ReadPin() == 0);

    __delay_us(ONEWIRE_RECOVERY_US);

    return presence_detected;
}

/*
 * Escribe un bit en el bus 1-Wire.
 */
static void OneWire_WriteBit(uint8_t bit_value)
{
    if (bit_value)
    {
        OneWire_Low();
        __delay_us(ONEWIRE_WRITE1_LOW_US);

        OneWire_Release();
        __delay_us(ONEWIRE_WRITE1_RELEASE_US);
    }
    else
    {
        OneWire_Low();
        __delay_us(ONEWIRE_WRITE0_LOW_US);

        OneWire_Release();
        __delay_us(ONEWIRE_WRITE0_RELEASE_US);
    }
}

/*
 * Lee un bit desde el bus 1-Wire.
 */
static uint8_t OneWire_ReadBit(void)
{
    uint8_t bit_value;

    OneWire_Low();
    __delay_us(ONEWIRE_READ_INIT_US);

    OneWire_Release();
    __delay_us(ONEWIRE_READ_SAMPLE_US);

    bit_value = OneWire_ReadPin();

    __delay_us(ONEWIRE_READ_RECOVERY_US);

    return bit_value;
}

/*
 * Escribe un byte completo en el bus.
 */
static void OneWire_WriteByte(uint8_t data)
{
    uint8_t i;

    for (i = 0; i < 8; i++)
    {
        OneWire_WriteBit(data & 0x01);
        data >>= 1;
    }
}

/*
 * Lee un byte completo desde el bus.
 */
static uint8_t OneWire_ReadByte(void)
{
    uint8_t i;
    uint8_t data = 0;

    for (i = 0; i < 8; i++)
    {
        if (OneWire_ReadBit())
        {
            data |= (1U << i);
        }
    }

    return data;
}

/*
 * Inicializa el controlador DS18B20.
 */
void DS18B20_Init(void)
{
    OneWire_Release();
}

/*
 * Inicia una conversión de temperatura.
 *
 * Retorna:
 * 1 -> Conversión iniciada correctamente
 * 0 -> Sensor no detectado
 */
uint8_t DS18B20_StartConversion(void)
{
    if (!OneWire_Reset())
    {
        return 0;
    }

    OneWire_WriteByte(DS18B20_CMD_SKIP_ROM);
    OneWire_WriteByte(DS18B20_CMD_CONVERT_T);

    return 1;
}

/*
 * Lee la temperatura y la devuelve en centésimas de grado.
 *
 * Ejemplo:
 * 3650 -> 36.50 °C
 */
uint8_t DS18B20_ReadTemperatureX100(int16_t *temperature_x100)
{
    uint8_t temp_lsb;
    uint8_t temp_msb;

    int16_t raw_temperature;
    int32_t temperature_scaled;

    if (!OneWire_Reset())
    {
        return 0;
    }

    OneWire_WriteByte(DS18B20_CMD_SKIP_ROM);
    OneWire_WriteByte(DS18B20_CMD_READ_SCRATCH);

    temp_lsb = OneWire_ReadByte();
    temp_msb = OneWire_ReadByte();

    raw_temperature =
        (int16_t)(((uint16_t)temp_msb << 8) | temp_lsb);

    /*
     * Resolución DS18B20:
     *
     * 1 LSB = 0.0625 °C
     *
     * Para obtener centésimas:
     *
     * temp_x100 = raw * 6.25
     *
     * Se implementa como:
     *
     * raw * 625 / 100
     */
    temperature_scaled =
        ((int32_t)raw_temperature * 625L) / 100L;

    *temperature_x100 =
        (int16_t)temperature_scaled;

    return 1;
}