/*
 * main.c
 * Proyecto: Practica 3 - Monitor Portatil de Signos Vitales
 * Microcontrolador: PIC18F4550
 * Compilador: XC8
 *
 * Funcionalidades:
 *   - OLED SSD1306 por I2C.
 *   - UART hacia PC.
 *   - MAX30102 para frecuencia cardiaca.
 *   - DS18B20 para temperatura corporal.
 *   - LED de sistema encendido.
 *   - LED de sistema funcional.
 *   - LED de espera.
 *   - Interruptor de congelado en RB2.
 *   - Buzzer de alarma por BPM fuera de rango en RD3.
 *   - LED de alarma BPM fuera de rango en RD4.
 *
 * Logica:
 *   - RD0 = LED sistema encendido.
 *   - RD1 = LED sistema funcional.
 *   - RD2 = LED de espera.
 *   - RD3 = buzzer.
 *   - RD4 = LED de alarma BPM.
 *   - RB2 = interruptor para congelar valores.
 *   - El MAX30102 detecta si hay dedo.
 *   - Si hay dedo, se activa el DS18B20.
 *   - Si no hay dedo, se muestra "No detectado".
 *   - La frecuencia cardiaca solo muestra:
 *       No detectado
 *       Calculando
 *       Valor BPM
 *   - Al activar el interruptor, OLED y UART conservan los ultimos valores.
 *   - El buzzer se activa si BPM < 60 o BPM > 100.
 *   - El LED de alarma parpadea 0.5 s apagado / 0.5 s encendido
 *     cuando el buzzer esta activo.
 */

#include "system.h"
#include "i2c_master.h"
#include "ssd1306.h"
#include "uart.h"
#include "ds18b20.h"
#include "max30102.h"
#include "freeze_switch.h"
#include "buzzer.h"

#define LOOP_DELAY_MS               10u

/*
 * Actualizacion cada 1 segundo:
 * 100 ciclos x 10 ms = 1000 ms.
 */
#define DISPLAY_UPDATE_COUNT        100u
#define UART_UPDATE_COUNT           100u

/*
 * DS18B20:
 * 80 ciclos x 10 ms = 800 ms.
 */
#define DS18B20_CONVERSION_COUNT    80u

/*
 * Deteccion de dedo por IR.
 * Si el dedo no se detecta, baja estos valores.
 */
#define FINGER_IR_ON_THRESHOLD      30000UL
#define FINGER_IR_OFF_THRESHOLD     20000UL

/*
 * Rango de temperatura considerado como contacto con piel.
 * 2800 = 28.00 C
 * 4500 = 45.00 C
 */
#define SKIN_MIN_TEMP_X100          2800
#define SKIN_MAX_TEMP_X100          4500

/*
 * LEDs indicadores:
 * RD0 = LED sistema encendido.
 * RD1 = LED sistema funcional.
 * RD2 = LED espera.
 *
 * Alarma:
 * RD3 = buzzer.
 * RD4 = LED de alarma.
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
static void LED_Status_Update(uint8_t max_ok, uint8_t ds18b20_ok);

static void LED_Wait_On(void);
static void LED_Wait_Off(void);
static void LED_Wait_Update(uint8_t finger_detected);

static uint8_t Is_Finger_Detected(uint32_t ir_value);
static uint8_t Is_Skin_Detected(uint8_t temp_valid, int16_t temp_x100);

static void UInt16_ToString(uint16_t value, char *buffer);
static void UInt32_ToString(uint32_t value, char *buffer);
static void Temp_ToString(int16_t temp_x100, char *buffer);

static void Show_Data_On_OLED(uint8_t finger_detected,
                              uint8_t temp_valid,
                              uint8_t skin_detected,
                              int16_t temp_x100,
                              uint8_t bpm_valid,
                              uint16_t bpm,
                              uint8_t system_functional,
                              uint8_t freeze_mode,
                              uint8_t buzzer_alarm);

static void Send_Header_By_UART(void);

static void Send_Data_By_UART(uint16_t sample,
                              uint8_t finger_detected,
                              uint8_t temp_valid,
                              uint8_t skin_detected,
                              int16_t temp_x100,
                              uint8_t bpm_valid,
                              uint16_t bpm,
                              uint32_t red_value,
                              uint32_t ir_value,
                              uint8_t system_functional,
                              uint8_t freeze_mode,
                              uint8_t buzzer_alarm);

void main(void)
{
    uint8_t max_ok = 0;

    uint32_t red_value = 0;
    uint32_t ir_value = 0;

    uint8_t finger_detected = 0;
    uint8_t previous_finger_detected = 0;

    uint16_t bpm = 0;
    uint8_t bpm_valid = 0;

    int16_t temp_x100 = 0;
    uint8_t temp_valid = 0;
    uint8_t skin_detected = 0;

    uint8_t ds18b20_ok = 0;
    uint8_t ds_conversion_active = 0;
    uint16_t ds_counter = 0;

    uint8_t system_functional = 0;

    /*
     * Variable de alarma:
     * 1 = BPM fuera de rango
     * 0 = BPM normal o no valido
     */
    uint8_t buzzer_alarm = 0;

    /*
     * Variables para modo congelado.
     */
    uint8_t freeze_mode = 0;
    uint8_t previous_freeze_mode = 0;

    uint16_t frozen_sample = 0;
    uint32_t frozen_red_value = 0;
    uint32_t frozen_ir_value = 0;

    uint8_t frozen_finger_detected = 0;
    uint8_t frozen_temp_valid = 0;
    uint8_t frozen_skin_detected = 0;

    int16_t frozen_temp_x100 = 0;

    uint8_t frozen_bpm_valid = 0;
    uint16_t frozen_bpm = 0;

    uint8_t frozen_system_functional = 0;

    uint16_t sample = 0;
    uint16_t display_counter = 0;
    uint16_t uart_counter = 0;

    System_Init();

    max_ok = MAX30102_Init();

    if (max_ok == 0)
    {
        LED_Status_Off();
        LED_Wait_Off();
        Buzzer_Off();

        SSD1306_ClearDisplay();
        SSD1306_SetCursor(10, 0);
        SSD1306_WriteString("MAX30102 ERROR");

        SSD1306_SetCursor(10, 2);
        SSD1306_WriteString("Revise I2C");

        SSD1306_SetCursor(10, 4);
        SSD1306_WriteString("SDA SCL VCC");

        UART_WriteLine("ERROR: MAX30102 no detectado");
        UART_WriteLine("Revise VCC, GND, SDA y SCL");

        while (1)
        {
            ;
        }
    }

    UART_WriteLine("Sistema iniciado");
    UART_WriteLine("MAX30102 detectado correctamente");
    UART_WriteLine("Esperando dedo para activar mediciones");

    Send_Header_By_UART();

    SSD1306_ClearDisplay();

    SSD1306_SetCursor(10, 0);
    SSD1306_WriteString("Signos Vitales");

    SSD1306_SetCursor(10, 2);
    SSD1306_WriteString("Esperando dedo");

    LED_Wait_On();
    Buzzer_Off();

    while (1)
    {
        /*
         * Lectura real del MAX30102.
         */
        MAX30102_ReadFIFO(&red_value, &ir_value);

        /*
         * Deteccion de dedo usando IR.
         */
        finger_detected = Is_Finger_Detected(ir_value);

        /*
         * LED de espera:
         * Encendido si no hay dedo.
         * Apagado si hay dedo.
         */
        LED_Wait_Update(finger_detected);

        /*
         * Cuando el dedo se detecta por primera vez, reinicia mediciones.
         */
        if ((finger_detected == 1) && (previous_finger_detected == 0))
        {
            MAX30102_ResetHeartRateAlgorithm();

            bpm = 0;
            bpm_valid = 0;

            temp_x100 = 0;
            temp_valid = 0;
            skin_detected = 0;

            ds_counter = 0;
            ds_conversion_active = DS18B20_StartConversion();

            if (ds_conversion_active)
            {
                ds18b20_ok = 1;
            }
            else
            {
                ds18b20_ok = 0;
            }

            UART_WriteLine("");
            UART_WriteLine("Dedo detectado - medicion activada");
            Send_Header_By_UART();
        }

        /*
         * Si no hay dedo, se reinician las mediciones.
         */
        if (finger_detected == 0)
        {
            bpm = 0;
            bpm_valid = 0;

            temp_x100 = 0;
            temp_valid = 0;
            skin_detected = 0;

            ds_conversion_active = 0;
            ds_counter = 0;
            ds18b20_ok = 0;

            MAX30102_ResetHeartRateAlgorithm();
        }
        else
        {
            /*
             * Procesamiento de BPM solo cuando hay dedo.
             */
            bpm_valid = MAX30102_ProcessHeartRate(ir_value, &bpm);

            /*
             * Manejo no bloqueante del DS18B20.
             */
            if (ds_conversion_active)
            {
                ds_counter++;

                if (ds_counter >= DS18B20_CONVERSION_COUNT)
                {
                    temp_valid = DS18B20_ReadTemperatureX100(&temp_x100);

                    if (temp_valid)
                    {
                        ds18b20_ok = 1;
                    }
                    else
                    {
                        ds18b20_ok = 0;
                    }

                    ds_conversion_active = 0;
                    ds_counter = 0;
                }
            }
            else
            {
                ds_conversion_active = DS18B20_StartConversion();

                if (ds_conversion_active)
                {
                    ds18b20_ok = 1;
                }
                else
                {
                    ds18b20_ok = 0;
                }

                ds_counter = 0;
            }

            skin_detected = Is_Skin_Detected(temp_valid, temp_x100);
        }

        /*
         * Estado funcional:
         * - MAX30102 detectado.
         * - DS18B20 responde cuando hay dedo.
         */
        if ((max_ok == 1) && (ds18b20_ok == 1))
        {
            system_functional = 1;
        }
        else
        {
            system_functional = 0;
        }

        LED_Status_Update(max_ok, ds18b20_ok);

        /*
         * Interruptor de congelado.
         *
         * Cuando el interruptor se activa, se toma una copia de los valores actuales.
         * Mientras siga activo, OLED, UART, buzzer y LED de alarma usan esa copia.
         */
        previous_freeze_mode = freeze_mode;
        freeze_mode = Freeze_Switch_Update();

        if ((freeze_mode == 1) && (previous_freeze_mode == 0))
        {
            frozen_sample = sample;
            frozen_red_value = red_value;
            frozen_ir_value = ir_value;

            frozen_finger_detected = finger_detected;
            frozen_temp_valid = temp_valid;
            frozen_skin_detected = skin_detected;

            frozen_temp_x100 = temp_x100;

            frozen_bpm_valid = bpm_valid;
            frozen_bpm = bpm;

            frozen_system_functional = system_functional;

            UART_WriteLine("");
            UART_WriteLine("MODO CONGELADO ACTIVADO");
            UART_WriteLine("Los valores de OLED, UART, buzzer y LED quedan fijos");

            /*
             * La alarma se evalua con los valores congelados.
             */
            buzzer_alarm = Buzzer_Update(frozen_finger_detected,
                                         frozen_bpm_valid,
                                         frozen_bpm);

            Show_Data_On_OLED(frozen_finger_detected,
                              frozen_temp_valid,
                              frozen_skin_detected,
                              frozen_temp_x100,
                              frozen_bpm_valid,
                              frozen_bpm,
                              frozen_system_functional,
                              freeze_mode,
                              buzzer_alarm);
        }

        if ((freeze_mode == 0) && (previous_freeze_mode == 1))
        {
            UART_WriteLine("");
            UART_WriteLine("MODO NORMAL ACTIVADO");
            Send_Header_By_UART();
        }

        /*
         * Control del buzzer y LED de alarma.
         *
         * En modo normal:
         *   evalua los valores reales.
         *
         * En modo congelado:
         *   evalua los valores congelados.
         */
        if (freeze_mode == 0)
        {
            buzzer_alarm = Buzzer_Update(finger_detected,
                                         bpm_valid,
                                         bpm);
        }
        else
        {
            buzzer_alarm = Buzzer_Update(frozen_finger_detected,
                                         frozen_bpm_valid,
                                         frozen_bpm);
        }

        /*
         * UART cada 1 segundo.
         * En modo congelado se reenvia la misma copia de datos.
         */
        uart_counter++;

        if (uart_counter >= UART_UPDATE_COUNT)
        {
            uart_counter = 0;

            if (freeze_mode == 0)
            {
                sample++;

                Send_Data_By_UART(sample,
                                  finger_detected,
                                  temp_valid,
                                  skin_detected,
                                  temp_x100,
                                  bpm_valid,
                                  bpm,
                                  red_value,
                                  ir_value,
                                  system_functional,
                                  freeze_mode,
                                  buzzer_alarm);
            }
            else
            {
                Send_Data_By_UART(frozen_sample,
                                  frozen_finger_detected,
                                  frozen_temp_valid,
                                  frozen_skin_detected,
                                  frozen_temp_x100,
                                  frozen_bpm_valid,
                                  frozen_bpm,
                                  frozen_red_value,
                                  frozen_ir_value,
                                  frozen_system_functional,
                                  freeze_mode,
                                  buzzer_alarm);
            }
        }

        /*
         * OLED cada 1 segundo.
         * En modo congelado se muestra la misma copia de datos.
         */
        display_counter++;

        if (display_counter >= DISPLAY_UPDATE_COUNT)
        {
            display_counter = 0;

            if (freeze_mode == 0)
            {
                Show_Data_On_OLED(finger_detected,
                                  temp_valid,
                                  skin_detected,
                                  temp_x100,
                                  bpm_valid,
                                  bpm,
                                  system_functional,
                                  freeze_mode,
                                  buzzer_alarm);
            }
            else
            {
                Show_Data_On_OLED(frozen_finger_detected,
                                  frozen_temp_valid,
                                  frozen_skin_detected,
                                  frozen_temp_x100,
                                  frozen_bpm_valid,
                                  frozen_bpm,
                                  frozen_system_functional,
                                  freeze_mode,
                                  buzzer_alarm);
            }
        }

        previous_finger_detected = finger_detected;

        __delay_ms(LOOP_DELAY_MS);
    }
}

static void System_Init(void)
{
    OSCCON = 0x72;

    /*
     * Todos los pines analogicos como digitales.
     * RA1 se usa como digital para DS18B20.
     */
    ADCON1 = 0x0F;

    CMCON = 0x07;
    CVRCON = 0x00;

    LEDs_Init();
    Freeze_Switch_Init();
    Buzzer_Init();

    LED_Power_On();
    LED_Status_Off();
    LED_Wait_On();
    Buzzer_Off();

    UART_Init();

    I2C_Master_Init(100000UL);

    SSD1306_Init();
    SSD1306_ClearDisplay();

    DS18B20_Init();
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

static void LED_Status_Update(uint8_t max_ok, uint8_t ds18b20_ok)
{
    if ((max_ok == 1) && (ds18b20_ok == 1))
    {
        LED_Status_On();
    }
    else
    {
        LED_Status_Off();
    }
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

    if (finger_state == 0)
    {
        if (ir_value >= FINGER_IR_ON_THRESHOLD)
        {
            finger_state = 1;
        }
    }
    else
    {
        if (ir_value < FINGER_IR_OFF_THRESHOLD)
        {
            finger_state = 0;
        }
    }

    return finger_state;
}

static uint8_t Is_Skin_Detected(uint8_t temp_valid, int16_t temp_x100)
{
    if ((temp_valid == 1) &&
        (temp_x100 >= SKIN_MIN_TEMP_X100) &&
        (temp_x100 <= SKIN_MAX_TEMP_X100))
    {
        return 1;
    }

    return 0;
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

static void Temp_ToString(int16_t temp_x100, char *buffer)
{
    char int_text[6];
    uint16_t abs_value;
    uint16_t int_part;
    uint8_t decimal_part;
    uint8_t i = 0;
    uint8_t j = 0;

    if (temp_x100 < 0)
    {
        buffer[j] = '-';
        j++;
        abs_value = (uint16_t)(-temp_x100);
    }
    else
    {
        abs_value = (uint16_t)temp_x100;
    }

    int_part = abs_value / 100;
    decimal_part = abs_value % 100;

    UInt16_ToString(int_part, int_text);

    while (int_text[i] != '\0')
    {
        buffer[j] = int_text[i];
        i++;
        j++;
    }

    buffer[j] = '.';
    j++;

    buffer[j] = (char)('0' + (decimal_part / 10));
    j++;

    buffer[j] = (char)('0' + (decimal_part % 10));
    j++;

    buffer[j] = '\0';
}

static void Show_Data_On_OLED(uint8_t finger_detected,
                              uint8_t temp_valid,
                              uint8_t skin_detected,
                              int16_t temp_x100,
                              uint8_t bpm_valid,
                              uint16_t bpm,
                              uint8_t system_functional,
                              uint8_t freeze_mode,
                              uint8_t buzzer_alarm)
{
    char text[12];

    SSD1306_ClearDisplay();

    SSD1306_SetCursor(10, 0);
    SSD1306_WriteString("Signos Vitales");

    /*
     * Frecuencia cardiaca.
     */
    SSD1306_SetCursor(10, 2);

    if (finger_detected == 0)
    {
        SSD1306_WriteString("FC: No detect");
    }
    else if (bpm_valid == 0)
    {
        SSD1306_WriteString("FC: Calculando");
    }
    else
    {
        SSD1306_WriteString("FC:");
        UInt16_ToString(bpm, text);
        SSD1306_WriteString(text);
        SSD1306_WriteString(" BPM");
    }

    /*
     * Temperatura.
     */
    SSD1306_SetCursor(10, 4);

    if (finger_detected == 0)
    {
        SSD1306_WriteString("Temp: No detect");
    }
    else if (temp_valid == 0)
    {
        SSD1306_WriteString("Temp: Leyendo");
    }
    else if (skin_detected == 0)
    {
        SSD1306_WriteString("Temp: No detect");
    }
    else
    {
        SSD1306_WriteString("Temp:");
        Temp_ToString(temp_x100, text);
        SSD1306_WriteString(text);
        SSD1306_WriteString(" C");
    }

    /*
     * Estado general / alarma.
     */
    SSD1306_SetCursor(10, 6);

    if (buzzer_alarm)
    {
        SSD1306_WriteString("ALERTA: BPM");
    }
    else if (freeze_mode)
    {
        SSD1306_WriteString("Modo: CONGELADO");
    }
    else if (system_functional)
    {
        SSD1306_WriteString("Sistema: OK");
    }
    else
    {
        SSD1306_WriteString("Sistema: ESP");
    }
}

static void Send_Header_By_UART(void)
{
    UART_WriteLine("");
    UART_WriteLine("Muestra\tFC_Estado\tFC_Valor\tTemp_Estado\tTemp_Valor\tRED\tIR\tSistema\tModo\tAlarma_BPM");
    UART_WriteLine("------------------------------------------------------------------------------------------------------");
}

static void Send_Data_By_UART(uint16_t sample,
                              uint8_t finger_detected,
                              uint8_t temp_valid,
                              uint8_t skin_detected,
                              int16_t temp_x100,
                              uint8_t bpm_valid,
                              uint16_t bpm,
                              uint32_t red_value,
                              uint32_t ir_value,
                              uint8_t system_functional,
                              uint8_t freeze_mode,
                              uint8_t buzzer_alarm)
{
    char text[12];

    UART_WriteUInt16(sample);
    UART_WriteString("\t");

    /*
     * Estado y valor de frecuencia cardiaca.
     */
    if (finger_detected == 0)
    {
        UART_WriteString("No detectado\t");
        UART_WriteString("-- BPM\t");
    }
    else if (bpm_valid == 0)
    {
        UART_WriteString("Detectado\t");
        UART_WriteString("Calculando\t");
    }
    else
    {
        UART_WriteString("Detectado\t");
        UART_WriteUInt16(bpm);
        UART_WriteString(" BPM\t");
    }

    /*
     * Estado y valor de temperatura.
     */
    if (finger_detected == 0)
    {
        UART_WriteString("No detectado\t");
        UART_WriteString("-- C\t");
    }
    else if (temp_valid == 0)
    {
        UART_WriteString("Detectado\t");
        UART_WriteString("Leyendo\t");
    }
    else if (skin_detected == 0)
    {
        UART_WriteString("No detectado\t");
        UART_WriteString("-- C\t");
    }
    else
    {
        UART_WriteString("Detectado\t");
        Temp_ToString(temp_x100, text);
        UART_WriteString(text);
        UART_WriteString(" C\t");
    }

    /*
     * Valores RED e IR.
     */
    UInt32_ToString(red_value, text);
    UART_WriteString(text);
    UART_WriteString("\t");

    UInt32_ToString(ir_value, text);
    UART_WriteString(text);
    UART_WriteString("\t");

    /*
     * Estado general del sistema.
     */
    if (system_functional)
    {
        UART_WriteString("OK\t");
    }
    else
    {
        UART_WriteString("ESPERA\t");
    }

    /*
     * Modo de trabajo.
     */
    if (freeze_mode)
    {
        UART_WriteString("CONGELADO\t");
    }
    else
    {
        UART_WriteString("NORMAL\t");
    }

    /*
     * Estado de alarma BPM.
     */
    if (buzzer_alarm)
    {
        UART_WriteLine("ACTIVA");
    }
    else
    {
        UART_WriteLine("INACTIVA");
    }
}