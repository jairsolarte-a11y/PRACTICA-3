#include "adc.h"

void ADC_Init(void)
{
    /*
       Potenciometro en RA0 / AN0.

       ADCON1 = 0x0E:
       AN0 analogico.
       AN1 en adelante digitales.
       Vref+ = VDD.
       Vref- = VSS.
    */

    ADCON1 = 0x0E;

    /*
       RA0 como entrada.
    */

    TRISAbits.TRISA0 = 1;

    /*
       ADCON2:
       ADFM = 1  -> resultado justificado a la derecha.
       ACQT = 101 -> tiempo de adquisicion 12 TAD.
       ADCS = 110 -> clock ADC Fosc/64.
    */

    ADCON2bits.ADFM = 1;
    ADCON2bits.ACQT = 0b101;
    ADCON2bits.ADCS = 0b110;

    /*
       Encender ADC en canal AN0.
    */

    ADCON0 = 0x01;
}

uint16_t ADC_Read(uint8_t channel)
{
    uint16_t result;

    /*
       Seleccionar canal ADC.
       En PIC18F4550, CHS esta en bits 5:2 de ADCON0.
    */

    ADCON0 &= 0b11000011;
    ADCON0 |= ((channel << 2) & 0b00111100);

    /*
       Pequeno tiempo de adquisicion.
    */

    __delay_us(20);

    /*
       Iniciar conversion.
    */

    ADCON0bits.GO_nDONE = 1;

    while (ADCON0bits.GO_nDONE);

    result = ((uint16_t)ADRESH << 8) | ADRESL;

    return result;
}
