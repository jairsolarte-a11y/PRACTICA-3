#ifndef DS18B20_H
#define DS18B20_H

#include <xc.h>
#include <stdint.h>

#define DS18B20_TRIS   TRISAbits.TRISA1
#define DS18B20_LAT    LATAbits.LATA1
#define DS18B20_PORT   PORTAbits.RA1

void DS18B20_Init(void);
uint8_t DS18B20_StartConversion(void);
uint8_t DS18B20_ReadTemperatureX100(int16_t *temperature_x100);

#endif