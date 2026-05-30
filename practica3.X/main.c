/*
 * main.c
 * Proyecto: Practica 3 - Monitor Portatil de Signos Vitales
 * Microcontrolador: PIC18F4550
 * Compilador: XC8
 *
 * Commit 5:
 * Se agrega lectura de valores RED e IR del MAX30102.
 *
 * Funcionalidades actuales:
 * - OLED SSD1306 por I2C.
 * - LEDs indicadores en RD0, RD1 y RD2.
 * - UART hacia computador a 9600 baudios.
 * - Deteccion inicial del MAX30102.
 * - Lectura de valores RED e IR desde el FIFO del MAX30102.
 *
 * Todavia no se agrega:
 * - Deteccion de dedo.
 * - Calculo de BPM.
 * - Sensor DS18B20.
 */

#include "system.h"
#include "i2c_master.h"
#include "ssd1306.h"
#include "uart.h"
#include "max30102.h"

#define LOOP_DELAY_MS               10u

/*
 * Actualizacion cada 1 segundo:
 * 100 ciclos x 10 ms = 1000 ms.
 */
#define DISPLAY_UPDATE_COUNT        100u
#define UART_UPDATE_COUNT           100u

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

static void UInt16_ToString(uint16_t value, char *buffer);
static void UInt32_ToString(uint32_t value, char *buffer);

static void Show_Red_Ir_On_OLED(uint32_t red_value, uint32_t ir_value);
static void Send_Header_By_UART(void);
static void Send_Red_Ir_By_UART(uint16_t sample, uint32_t red_value, uint32_t ir_value);

void main(void)
{
    uint8_t max_ok = 0;

    uint32_t red_value = 0;
    uint32_t ir_value = 0;

    uint16_t sample = 0;
    uint16_t display_counter = 0;
    uint16_t uart_counter = 0;

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
     * Verificacion del MAX30102.
     */
    max_ok = MAX30102_Init();

    SSD1306_ClearDisplay();

    if (max_ok == 0)
    {
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

        while (1)
        {
            NOP();
        }
    }

    /*
     * Si llega aqui, el sensor fue detectado correctamente.
     */
    LED_Status_On();
    LED_Wait_On();

    UART_WriteLine("MAX30102 detectado correctamente");
    UART_WriteLine("Lectura RED e IR iniciada");

    Send_Header_By_UART();

    SSD1306_SetCursor(10, 0);
    SSD1306_WriteString("Signos Vitales");

    SSD1306_SetCursor(10, 2);
    SSD1306_WriteString("Leyendo MAX");

    SSD1306_SetCursor(10, 4);
    SSD1306_WriteString("RED / IR");

    while (1)
    {
        /*
         * Lectura real del FIFO del MAX30102.
         * Se obtiene una muestra RED y una muestra IR.
         */
        MAX30102_ReadFIFO(&red_value, &ir_value);

        /*
         * Envio por UART cada 1 segundo.
         */
        uart_counter++;

        if (uart_counter >= UART_UPDATE_COUNT)
        {
            uart_counter = 0;
            sample++;

            Send_Red_Ir_By_UART(sample, red_value, ir_value);
        }

        /*
         * Actualizacion de OLED cada 1 segundo.
         */
        display_counter++;

        if (display_counter >= DISPLAY_UPDATE_COUNT)
        {
            display_counter = 0;

            Show_Red_Ir_On_OLED(red_value, ir_value);
        }

        __delay_ms(LOOP_DELAY_MS);
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

static void UInt16_ToString(uint16_t value, char *buffer)
{
    char temp[6];
    uint8_t i = 0;
    uint8_t j = 0;

    if (value == 0)
    {
        buffer[0] = '0';
        buffer[1] = '\0';
        return;
    }

    while (value > 0)
    {
        temp[i] = (char)('0' + (value % 10));
        value /= 10;
        i++;
    }

    while (i > 0)
    {
        i--;
        buffer[j] = temp[i];
        j++;
    }

    buffer[j] = '\0';
}

static void UInt32_ToString(uint32_t value, char *buffer)
{
    char temp[11];
    uint8_t i = 0;
    uint8_t j = 0;

    if (value == 0)
    {
        buffer[0] = '0';
        buffer[1] = '\0';
        return;
    }

    while (value > 0)
    {
        temp[i] = (char)('0' + (value % 10UL));
        value /= 10UL;
        i++;
    }

    while (i > 0)
    {
        i--;
        buffer[j] = temp[i];
        j++;
    }

    buffer[j] = '\0';
}

static void Show_Red_Ir_On_OLED(uint32_t red_value, uint32_t ir_value)
{
    char text[12];

    SSD1306_ClearDisplay();

    SSD1306_SetCursor(10, 0);
    SSD1306_WriteString("MAX30102 DATA");

    SSD1306_SetCursor(10, 2);
    SSD1306_WriteString("RED:");
    UInt32_ToString(red_value, text);
    SSD1306_WriteString(text);

    SSD1306_SetCursor(10, 4);
    SSD1306_WriteString("IR:");
    UInt32_ToString(ir_value, text);
    SSD1306_WriteString(text);

    SSD1306_SetCursor(10, 6);
    SSD1306_WriteString("Leyendo...");
}

static void Send_Header_By_UART(void)
{
    UART_WriteLine("");
    UART_WriteLine("Muestra\tRED\tIR");
    UART_WriteLine("---------------------------");
}

static void Send_Red_Ir_By_UART(uint16_t sample, uint32_t red_value, uint32_t ir_value)
{
    char text[12];

    UInt16_ToString(sample, text);
    UART_WriteString(text);
    UART_WriteString("\t");

    UInt32_ToString(red_value, text);
    UART_WriteString(text);
    UART_WriteString("\t");

    UInt32_ToString(ir_value, text);
    UART_WriteLine(text);
}