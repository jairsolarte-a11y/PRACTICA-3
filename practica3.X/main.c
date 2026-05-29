/*
 * main.c
 * Proyecto: Practica 3 - Monitor Portatil de Signos Vitales
 * Microcontrolador: PIC18F4550
 * Compilador: XC8
 *
 * Commit 3:
 * Se agrega comunicacion UART para monitoreo serial.
 *
 * Funcionalidades actuales:
 * - OLED SSD1306 por I2C.
 * - LEDs indicadores en RD0, RD1 y RD2.
 * - UART hacia computador a 9600 baudios.
 *
 * Todavia no se agregan:
 * - MAX30102.
 * - DS18B20.
 * - Calculo de BPM.
 * - Lectura de temperatura.
 */

#include "system.h"
#include "i2c_master.h"
#include "ssd1306.h"
#include "uart.h"

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

    /*
     * Mensajes enviados al computador por UART.
     * Sirven para verificar que la comunicacion serial funciona.
     */
    UART_WriteLine("Sistema iniciado");
    UART_WriteLine("OLED funcionando");
    UART_WriteLine("LEDs indicadores activos");
    UART_WriteLine("UART funcionando a 9600 baudios");
    UART_WriteLine("Sistema en espera");

    /*
     * Mensajes mostrados en la pantalla OLED.
     */
    SSD1306_SetCursor(10, 0);
    SSD1306_WriteString("Signos Vitales");

    SSD1306_SetCursor(10, 2);
    SSD1306_WriteString("OLED OK");

    SSD1306_SetCursor(10, 4);
    SSD1306_WriteString("UART OK");

    SSD1306_SetCursor(10, 6);
    SSD1306_WriteString("En espera");

    /*
     * Estados iniciales de LEDs:
     * RD0 encendido: firmware iniciado.
     * RD1 apagado: sensores aun no validados.
     * RD2 encendido: sistema en espera.
     */
    LED_Power_On();
    LED_Status_Off();
    LED_Wait_On();

    while (1)
    {
        /*
         * En este commit el sistema queda en espera.
         * La UART ya queda disponible para enviar mensajes
         * en los siguientes commits.
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
     * Luego cada modulo configura los pines que necesita.
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

    /*
     * Inicializacion UART.
     * RC6 = TX
     * RC7 = RX
     */
    UART_Init();

    /*
     * Inicializacion del bus I2C a 100 kHz.
     */
    I2C_Master_Init(100000UL);

    /*
     * Inicializacion de la pantalla OLED.
     */
    SSD1306_Init();
    SSD1306_ClearDisplay();
}

static void LEDs_Init(void)
{
    /*
     * RD0, RD1 y RD2 como salidas digitales.
     */
    LED_POWER_TRIS = 0;
    LED_STATUS_TRIS = 0;
    LED_WAIT_TRIS = 0;

    /*
     * Estado inicial: todos apagados.
     */
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