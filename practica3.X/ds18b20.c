#include "system.h"
#include "ds18b20.h"

#define DS18B20_CMD_SKIP_ROM       0xCC
#define DS18B20_CMD_CONVERT_T      0x44
#define DS18B20_CMD_READ_SCRATCH   0xBE

static void OneWire_Low(void)
{
    DS18B20_LAT = 0;
    DS18B20_TRIS = 0;
}

static void OneWire_Release(void)
{
    DS18B20_TRIS = 1;
}

static uint8_t OneWire_ReadPin(void)
{
    return DS18B20_PORT;
}

static uint8_t OneWire_Reset(void)
{
    uint8_t presence;

    OneWire_Low();
    __delay_us(480);

    OneWire_Release();
    __delay_us(70);

    presence = (OneWire_ReadPin() == 0);

    __delay_us(410);

    return presence;
}

static void OneWire_WriteBit(uint8_t bit_value)
{
    if (bit_value)
    {
        OneWire_Low();
        __delay_us(6);

        OneWire_Release();
        __delay_us(64);
    }
    else
    {
        OneWire_Low();
        __delay_us(60);

        OneWire_Release();
        __delay_us(10);
    }
}

static uint8_t OneWire_ReadBit(void)
{
    uint8_t bit_value;

    OneWire_Low();
    __delay_us(6);

    OneWire_Release();
    __delay_us(9);

    bit_value = OneWire_ReadPin();

    __delay_us(55);

    return bit_value;
}

static void OneWire_WriteByte(uint8_t data)
{
    uint8_t i;

    for (i = 0; i < 8; i++)
    {
        OneWire_WriteBit(data & 0x01);
        data >>= 1;
    }
}

static uint8_t OneWire_ReadByte(void)
{
    uint8_t i;
    uint8_t data = 0;

    for (i = 0; i < 8; i++)
    {
        if (OneWire_ReadBit())
        {
            data |= (1 << i);
        }
    }

    return data;
}

void DS18B20_Init(void)
{
    OneWire_Release();
}

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

uint8_t DS18B20_ReadTemperatureX100(int16_t *temperature_x100)
{
    uint8_t temp_lsb;
    uint8_t temp_msb;
    int16_t raw_temperature;

    if (!OneWire_Reset())
    {
        return 0;
    }

    OneWire_WriteByte(DS18B20_CMD_SKIP_ROM);
    OneWire_WriteByte(DS18B20_CMD_READ_SCRATCH);

    temp_lsb = OneWire_ReadByte();
    temp_msb = OneWire_ReadByte();

    raw_temperature = (int16_t)(((uint16_t)temp_msb << 8) | temp_lsb);

    *temperature_x100 = (int16_t)(((int32_t)raw_temperature * 625L) / 100L);

    return 1;
}