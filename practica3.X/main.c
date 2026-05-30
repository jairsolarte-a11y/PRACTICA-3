/*
 * main.c
 * Proyecto: Practica 3 - Monitor Portatil de Signos Vitales
 * Microcontrolador: PIC18F4550
 * Compilador: XC8
 *
 * Commit 7:
 * Se agrega calculo de frecuencia cardiaca BPM usando MAX30102.
 *
 * Funcionalidades actuales:
 * - OLED SSD1306 por I2C.
 * - LEDs indicadores en RD0, RD1 y RD2.
 * - UART hacia computador a 9600 baudios.
 * - Deteccion inicial del MAX30102.
 * - Lectura de valores RED e IR desde el FIFO del MAX30102.
 * - Deteccion de presencia de dedo usando valor IR.
 * - Calculo de BPM usando la senal IR.
 *
 * Todavia no se agrega:
 * - Sensor DS18B20.
 * - Lectura de temperatura.
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
 * Deteccion de dedo por IR.
 */
#define FINGER_IR_ON_THRESHOLD      30000UL
#define FINGER_IR_OFF_THRESHOLD     20000UL

/*
 * LEDs indicadores:
 * RD0 = LED sistema encendido.
 * RD1 = LED sistema funcional / MAX30102 detectado.
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
static void LED_Wait_Update(uint8_t finger_detected);

static uint8_t Is_Finger_Detected(uint32_t ir_value);

static void UInt16_ToString(uint16_t value, char *buffer);
static void UInt32_ToString(uint32_t value, char *buffer);

static void Show_BPM_On_OLED(uint8_t finger_detected,
                             uint8_t bpm_valid,
                             uint16_t bpm,
                             uint32_t red_value,
                             uint32_t ir_value);

static void Send_Header_By_UART(void);

static void Send_BPM_By_UART(uint16_t sample,
                             uint8_t finger_detected,
                             uint8_t bpm_valid,
                             uint16_t bpm,
                             uint32_t red_value,
                             uint32_t ir_value);

void main(void)
{
    uint8_t max_ok = 0;

    uint32_t red_value = 0;
    uint32_t ir_value = 0;

    uint8_t finger_detected = 0;
    uint8_t previous_finger_detected = 0;

    uint16_t bpm = 0;
    uint8_t bpm_valid = 0;

    uint16_t sample = 0;
    uint16_t display_counter = 0;
    uint16_t uart_counter = 0;

    System_Init();

    UART_WriteLine("Sistema iniciado");
    UART_WriteLine("OLED funcionando");
    UART_WriteLine("UART funcionando a 9600 baudios");
    UART_WriteLine("Verificando MAX30102...");

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
     * Si llega aqui, el MAX30102 fue detectado correctamente.
     */
    LED_Status_On();
    LED_Wait_On();

    UART_WriteLine("MAX30102 detectado correctamente");
    UART_WriteLine("Calculo de BPM iniciado");
    UART_WriteLine("Esperando dedo...");

    Send_Header_By_UART();

    SSD1306_SetCursor(10, 0);
    SSD1306_WriteString("Signos Vitales");

    SSD1306_SetCursor(10, 2);
    SSD1306_WriteString("Esperando dedo");

    SSD1306_SetCursor(10, 4);
    SSD1306_WriteString("BPM listo");

    while (1)
    {
        /*
         * Lectura real del FIFO del MAX30102.
         */
        MAX30102_ReadFIFO(&red_value, &ir_value);

        /*
         * Deteccion de dedo usando la senal IR.
         */
        finger_detected = Is_Finger_Detected(ir_value);

        /*
         * LED de espera:
         * Encendido si no hay dedo.
         * Apagado si hay dedo.
         */
        LED_Wait_Update(finger_detected);

        /*
         * Cuando el dedo se detecta por primera vez,
         * se reinicia el algoritmo de BPM para evitar datos anteriores.
         */
        if ((finger_detected == 1) && (previous_finger_detected == 0))
        {
            MAX30102_ResetHeartRateAlgorithm();

            bpm = 0;
            bpm_valid = 0;

            UART_WriteLine("");
            UART_WriteLine("Dedo detectado - calculando BPM");
            Send_Header_By_UART();
        }

        /*
         * Si el dedo se retira, se limpian las mediciones.
         */
        if ((finger_detected == 0) && (previous_finger_detected == 1))
        {
            MAX30102_ResetHeartRateAlgorithm();

            bpm = 0;
            bpm_valid = 0;

            UART_WriteLine("");
            UART_WriteLine("Dedo retirado");
            Send_Header_By_UART();
        }

        /*
         * Calculo de BPM solo cuando hay dedo.
         */
        if (finger_detected == 1)
        {
            bpm_valid = MAX30102_ProcessHeartRate(ir_value, &bpm);
        }
        else
        {
            bpm = 0;
            bpm_valid = 0;
            MAX30102_ResetHeartRateAlgorithm();
        }

        /*
         * Envio por UART cada 1 segundo.
         */
        uart_counter++;

        if (uart_counter >= UART_UPDATE_COUNT)
        {
            uart_counter = 0;
            sample++;

            Send_BPM_By_UART(sample,
                             finger_detected,
                             bpm_valid,
                             bpm,
                             red_value,
                             ir_value);
        }

        /*
         * Actualizacion de OLED cada 1 segundo.
         */
        display_counter++;

        if (display_counter >= DISPLAY_UPDATE_COUNT)
        {
            display_counter = 0;

            Show_BPM_On_OLED(finger_detected,
                             bpm_valid,
                             bpm,
                             red_value,
                             ir_value);
        }

        previous_finger_detected = finger_detected;

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

static void LED_Wait_Update(uint8_t finger_detected)
{
    if (finger_detected == 0)
    {
        LED_Wait_On();
    }
    else
    {
        LED_Wait_Off();
    }
}

static uint8_t Is_Finger_Detected(uint32_t ir_value)
{
    static uint8_t finger_state = 0;

    /*
     * Si actualmente no hay dedo, se exige superar el umbral alto.
     */
    if (finger_state == 0)
    {
        if (ir_value >= FINGER_IR_ON_THRESHOLD)
        {
            finger_state = 1;
        }
    }
    /*
     * Si ya habia dedo detectado, solo se quita el estado cuando
     * el IR cae por debajo del umbral bajo.
     */
    else
    {
        if (ir_value < FINGER_IR_OFF_THRESHOLD)
        {
            finger_state = 0;
        }
    }

    return finger_state;
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

static void Show_BPM_On_OLED(uint8_t finger_detected,
                             uint8_t bpm_valid,
                             uint16_t bpm,
                             uint32_t red_value,
                             uint32_t ir_value)
{
    char text[12];

    SSD1306_ClearDisplay();

    SSD1306_SetCursor(10, 0);
    SSD1306_WriteString("Signos Vitales");

    SSD1306_SetCursor(10, 2);

    if (finger_detected == 0)
    {
        SSD1306_WriteString("No detectado");

        SSD1306_SetCursor(10, 4);
        SSD1306_WriteString("Esperando dedo");
    }
    else
    {
        if (bpm_valid == 1)
        {
            SSD1306_WriteString("BPM:");
            UInt16_ToString(bpm, text);
            SSD1306_WriteString(text);
        }
        else
        {
            SSD1306_WriteString("Calculando");
        }

        SSD1306_SetCursor(10, 4);
        SSD1306_WriteString("IR:");
        UInt32_ToString(ir_value, text);
        SSD1306_WriteString(text);
    }

    SSD1306_SetCursor(10, 6);
    SSD1306_WriteString("RED:");
    UInt32_ToString(red_value, text);
    SSD1306_WriteString(text);
}

static void Send_Header_By_UART(void)
{
    UART_WriteLine("");
    UART_WriteLine("Muestra\tDedo\tBPM\tRED\tIR");
    UART_WriteLine("------------------------------------------------");
}

static void Send_BPM_By_UART(uint16_t sample,
                             uint8_t finger_detected,
                             uint8_t bpm_valid,
                             uint16_t bpm,
                             uint32_t red_value,
                             uint32_t ir_value)
{
    char text[12];

    UInt16_ToString(sample, text);
    UART_WriteString(text);
    UART_WriteString("\t");

    if (finger_detected)
    {
        UART_WriteString("Detectado\t");
    }
    else
    {
        UART_WriteString("No detectado\t");
    }

    if (finger_detected == 0)
    {
        UART_WriteString("No detectado\t");
    }
    else
    {
        if (bpm_valid)
        {
            UInt16_ToString(bpm, text);
            UART_WriteString(text);
            UART_WriteString("\t");
        }
        else
        {
            UART_WriteString("Calculando\t");
        }
    }

    UInt32_ToString(red_value, text);
    UART_WriteString(text);
    UART_WriteString("\t");

    UInt32_ToString(ir_value, text);
    UART_WriteLine(text);
}