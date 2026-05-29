/*
 * main.c
 * Proyecto: Practica 3 - Monitor Portatil de Signos Vitales
 * Microcontrolador: PIC18F4550
 * Compilador: XC8
 *
 * Commit 2:
 * Se agregan indicadores LED del sistema directamente en main.c.
 *
 * Funcionalidades actuales:
 * - OLED SSD1306 por I2C.
 * - LED de sistema encendido en RD0.
 * - LED de sistema funcional en RD1.
 * - LED de espera en RD2.
 */

#include "system.h"
#include "i2c_master.h"
#include "ssd1306.h"

/*
 * LEDs indicadores:
 * RD0 = LED sistema encendido.
 * RD1 = LED sistema funcional.
 * RD2 = LED espera.
 */
#define LED_POWER_TRIS              TRISDbits.TRISD0
#define LED_POWER_LAT               LATDbits.LATD0

#define LED_STATUS_TRIS             TRISDbits.TRISD1
#define LED_STATUS_LAT              LATDbits.LATD1

#define LED_WAIT_TRIS               TRISDbits.TRISD2
#define LED_WAIT_LAT                LATDbits.LATD2

#define LED_ON                      1
#define LED_OFF                     0

static void System_Init(void);

static void LEDs_Init(void);
static void LED_Power_On(void);
static void LED_Status_On(void);
static void LED_Status_Off(void);
static void LED_Wait_On(void);
static void LED_Wait_Off(void);

void main(void)
{
    System_Init();

    SSD1306_SetCursor(10, 0);
    SSD1306_WriteString("Signos Vitales");

    SSD1306_SetCursor(10, 2);
    SSD1306_WriteString("OLED OK");

    SSD1306_SetCursor(10, 4);
    SSD1306_WriteString("LEDs OK");

    SSD1306_SetCursor(10, 6);
    SSD1306_WriteString("En espera");

    while (1)
    {
        /*
         * En este commit el sistema queda en espera.
         * RD0 queda encendido.
         * RD1 queda apagado.
         * RD2 queda encendido.
         */
        NOP();
    }
}

static void System_Init(void)
{
    /*
     * Oscilador interno a 8 MHz.
     */
    OSCCON = 0x72;

    /*
     * Todos los pines analogicos como digitales.
     */
    ADCON1 = 0x0F;

    /*
     * Comparadores analogicos desactivados.
     */
    CMCON = 0x07;
    CVRCON = 0x00;

    /*
     * Limpieza de salidas.
     */
    LATA = 0x00;
    LATB = 0x00;
    LATC = 0x00;
    LATD = 0x00;
    LATE = 0x00;

    /*
     * Inicialmente todos los puertos como entrada.
     */
    TRISA = 0xFF;
    TRISB = 0xFF;
    TRISC = 0xFF;
    TRISD = 0xFF;
    TRISE = 0xFF;

    /*
     * Inicializacion de LEDs indicadores.
     */
    LEDs_Init();

    LED_Power_On();
    LED_Status_Off();
    LED_Wait_On();

    /*
     * Inicializacion del bus I2C y OLED.
     */
    I2C_Master_Init(100000UL);

    SSD1306_Init();
    SSD1306_ClearDisplay();
}

static void LEDs_Init(void)
{
    LED_POWER_TRIS = 0;
    LED_STATUS_TRIS = 0;
    LED_WAIT_TRIS = 0;

    LED_POWER_LAT = LED_OFF;
    LED_STATUS_LAT = LED_OFF;
    LED_WAIT_LAT = LED_OFF;
}

static void LED_Power_On(void)
{
    LED_POWER_LAT = LED_ON;
}

static void LED_Status_On(void)
{
    LED_STATUS_LAT = LED_ON;
}

static void LED_Status_Off(void)
{
    LED_STATUS_LAT = LED_OFF;
}

static void LED_Wait_On(void)
{
    LED_WAIT_LAT = LED_ON;
}

static void LED_Wait_Off(void)
{
    LED_WAIT_LAT = LED_OFF;
}