/*
 * main.c
 * Proyecto: Practica 3 - Monitor Portatil de Signos Vitales
 * Microcontrolador: PIC18F4550
 * Compilador: XC8
 *
 * Commit 4:
 * Se agrega deteccion del sensor MAX30102 por I2C.
 *
 * Funcionalidades actuales:
 * - OLED SSD1306 por I2C.
 * - LEDs indicadores en RD0, RD1 y RD2.
 * - UART hacia computador a 9600 baudios.
 * - Deteccion inicial del sensor MAX30102.
 *
 * Todavia no se agrega:
 * - Lectura RED/IR del MAX30102.
 * - Deteccion de dedo.
 * - Calculo de BPM.
 * - Sensor DS18B20.
 */

#include "system.h"
#include "i2c_master.h"
#include "ssd1306.h"
#include "uart.h"
#include "max30102.h"

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
    uint8_t max_ok = 0;

    System_Init();

    /*
     * Mensajes iniciales por UART.
     */
    UART_WriteLine("Sistema iniciado");
    UART_WriteLine("OLED funcionando");
    UART_WriteLine("LEDs indicadores activos");
    UART_WriteLine("UART funcionando a 9600 baudios");
    UART_WriteLine("Verificando MAX30102...");

    /*
     * Verificacion del sensor MAX30102.
     * Si retorna 1, el sensor respondio correctamente por I2C.
     * Si retorna 0, no se pudo leer el PART_ID esperado.
     */
    max_ok = MAX30102_Init();

    SSD1306_ClearDisplay();

    if (max_ok == 1)
    {
        /*
         * Estado fisico:
         * RD1 se enciende porque el MAX30102 fue detectado.
         * RD2 queda encendido porque el sistema aun esta en espera.
         */
        LED_Status_On();
        LED_Wait_On();

        UART_WriteLine("MAX30102 detectado correctamente");
        UART_WriteLine("Sistema en espera");

        SSD1306_SetCursor(10, 0);
        SSD1306_WriteString("Signos Vitales");

        SSD1306_SetCursor(10, 2);
        SSD1306_WriteString("MAX30102 OK");

        SSD1306_SetCursor(10, 4);
        SSD1306_WriteString("UART OK");

        SSD1306_SetCursor(10, 6);
        SSD1306_WriteString("En espera");
    }
    else
    {
        /*
         * Estado fisico:
         * RD1 permanece apagado porque el sensor no fue detectado.
         * RD2 se apaga para indicar estado de error, no de espera normal.
         */
        LED_Status_Off();
        LED_Wait_Off();

        UART_WriteLine("ERROR: MAX30102 no detectado");
        UART_WriteLine("Revise VCC, GND, SDA y SCL");

        SSD1306_SetCursor(10, 0);
        SSD1306_WriteString("MAX30102 ERROR");

        SSD1306_SetCursor(10, 2);
        SSD1306_WriteString("Revise I2C");

        SSD1306_SetCursor(10, 4);
        SSD1306_WriteString("SDA SCL VCC");
    }

    while (1)
    {
        /*
         * En este commit el sistema queda detenido en una pantalla
         * de diagnostico. En el siguiente commit se agregara la lectura
         * de valores RED e IR desde el FIFO del MAX30102.
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
     * Estados iniciales.
     */
    LED_Power_On();
    LED_Status_Off();
    LED_Wait_On();

    /*
     * Inicializacion UART.
     * RC6 = TX
     * RC7 = RX
     */
    UART_Init();

    /*
     * Inicializacion del bus I2C a 100 kHz.
     * Este bus se usa para OLED y MAX30102.
     */
    I2C_Master_Init(100000UL);

    /*
     * Inicializacion de pantalla OLED.
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