#include "system.h"
#include "i2c_master.h"
#include "ssd1306.h"

/*
   Proyecto minimo: solo pantalla OLED SSD1306 por I2C.

   PIC18F4550:
   SDA -> RB0 / pin fisico 33
   SCL -> RB1 / pin fisico 34
*/

#define OLED_X_OFFSET    8u

static void System_Init(void);
static void OLED_Show_Message(void);

void main(void)
{
    System_Init();

    OLED_Show_Message();

    while (1)
    {
        /*
           La pantalla mantiene el texto en memoria.
           No se usan sensores, ADC, LEDs, buzzer ni ventilador.
        */
        __delay_ms(1000);
    }
}

static void System_Init(void)
{
    /*
       Oscilador interno a 8 MHz.
    */
    OSCCON = 0x72;

    /*
       Desactivar comparadores.
    */
    CMCON = 0x07;
    CVRCON = 0x00;

    /*
       I2C OLED:
       SDA -> RB0 / pin fisico 33
       SCL -> RB1 / pin fisico 34
    */
    I2C_Master_Init(100000UL);

    SSD1306_Init();
}

static void OLED_Show_Message(void)
{
    SSD1306_ClearDisplay();

    SSD1306_SetCursor(OLED_X_OFFSET, 3);
    SSD1306_WriteString("practica 3");
}
