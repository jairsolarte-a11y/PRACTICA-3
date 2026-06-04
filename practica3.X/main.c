/*
 * main.c
 * Prueba I2C MAX30102 con visualizacion en OLED SSD1306
 * Microcontrolador: PIC18F4550
 * Compilador: XC8
 *
 * Objetivo:
 *   - Verificar comunicacion I2C con MAX30102.
 *   - Leer PART_ID y REV_ID.
 *   - Configurar el sensor.
 *   - Leer valores RED e IR desde FIFO.
 *   - Mostrar estado y valores en OLED.
 *   - Enviar informacion por UART.
 *
 * Pines usados:
 *   RB0 = SDA
 *   RB1 = SCL
 *   RC6 = TX UART
 *   RC7 = RX UART
 *
 * OLED:
 *   Se muestra la informacion usando la misma forma del proyecto:
 *   SSD1306_ClearDisplay()
 *   SSD1306_SetCursor(10, pagina)
 *   SSD1306_WriteString(...)
 */

#include "system.h"
#include "i2c_master.h"
#include "ssd1306.h"
#include "uart.h"

/*
 * Tiempo base del ciclo principal.
 */
#define LOOP_DELAY_MS               10u

/*
 * Actualizacion OLED:
 * 50 ciclos x 10 ms = 500 ms.
 */
#define DISPLAY_UPDATE_COUNT        50u

/*
 * Envio UART:
 * 100 ciclos x 10 ms = 1000 ms.
 */
#define UART_UPDATE_COUNT           100u

/*
 * MAX30102:
 * Direccion I2C de 7 bits = 0x57.
 * Escritura = 0xAE.
 * Lectura   = 0xAF.
 */
#define MAX30102_I2C_ADDRESS        0x57u
#define MAX30102_I2C_WRITE          ((MAX30102_I2C_ADDRESS << 1) | 0u)
#define MAX30102_I2C_READ           ((MAX30102_I2C_ADDRESS << 1) | 1u)

/*
 * Registros principales del MAX30102.
 */
#define MAX30102_REG_INT_STATUS_1   0x00u
#define MAX30102_REG_INT_STATUS_2   0x01u
#define MAX30102_REG_FIFO_WR_PTR    0x04u
#define MAX30102_REG_OVF_COUNTER    0x05u
#define MAX30102_REG_FIFO_RD_PTR    0x06u
#define MAX30102_REG_FIFO_DATA      0x07u
#define MAX30102_REG_FIFO_CONFIG    0x08u
#define MAX30102_REG_MODE_CONFIG    0x09u
#define MAX30102_REG_SPO2_CONFIG    0x0Au
#define MAX30102_REG_LED1_PA        0x0Cu
#define MAX30102_REG_LED2_PA        0x0Du
#define MAX30102_REG_REV_ID         0xFEu
#define MAX30102_REG_PART_ID        0xFFu

/*
 * Valor esperado para MAX30102.
 */
#define MAX30102_EXPECTED_PART_ID   0x15u

/*
 * Umbral simple para saber si hay dedo.
 * Si no detecta dedo, puedes bajar este valor.
 */
#define FINGER_IR_ON_THRESHOLD      30000UL
#define FINGER_IR_OFF_THRESHOLD     20000UL

static void System_Init(void);

static uint8_t MAX30102_Test_ReadRegister(uint8_t reg, uint8_t *value);
static uint8_t MAX30102_Test_WriteRegister(uint8_t reg, uint8_t value);
static uint8_t MAX30102_Test_ReadFIFO(uint32_t *red_value, uint32_t *ir_value);
static uint8_t MAX30102_Test_Config(void);
static uint8_t MAX30102_Test_CheckID(uint8_t *rev_id, uint8_t *part_id);

static uint8_t Is_Finger_Detected(uint32_t ir_value);

static void UInt8_ToHexString(uint8_t value, char *buffer);
static void UInt16_ToString(uint16_t value, char *buffer);
static void UInt32_ToString(uint32_t value, char *buffer);

static void Show_Start_On_OLED(void);
static void Show_Error_On_OLED(uint8_t part_id);
static void Show_Data_On_OLED(uint8_t sensor_ok,
                              uint8_t part_id,
                              uint8_t rev_id,
                              uint32_t red_value,
                              uint32_t ir_value,
                              uint8_t finger_detected);

static void Send_Test_By_UART(uint8_t sensor_ok,
                              uint8_t part_id,
                              uint8_t rev_id,
                              uint32_t red_value,
                              uint32_t ir_value,
                              uint8_t finger_detected);

void main(void)
{
    uint8_t sensor_ok = 0;
    uint8_t sensor_configured = 0;

    uint8_t part_id = 0;
    uint8_t rev_id = 0;

    uint32_t red_value = 0;
    uint32_t ir_value = 0;

    uint8_t finger_detected = 0;

    uint16_t display_counter = 0;
    uint16_t uart_counter = 0;

    System_Init();

    Show_Start_On_OLED();

    UART_WriteLine("");
    UART_WriteLine("=======================================");
    UART_WriteLine(" PRUEBA MAX30102 POR I2C");
    UART_WriteLine(" OLED SSD1306 + UART");
    UART_WriteLine("=======================================");
    UART_WriteLine("");
    UART_WriteLine("Conexiones:");
    UART_WriteLine(" MAX30102 VIN -> 3.3V");
    UART_WriteLine(" MAX30102 GND -> GND");
    UART_WriteLine(" MAX30102 SDA -> RB0");
    UART_WriteLine(" MAX30102 SCL -> RB1");
    UART_WriteLine("");

    while (1)
    {
        /*
         * Si el sensor aun no esta configurado,
         * se intenta leer PART_ID y REV_ID.
         */
        if (sensor_configured == 0)
        {
            sensor_ok = MAX30102_Test_CheckID(&rev_id, &part_id);

            if (sensor_ok == 1)
            {
                sensor_configured = MAX30102_Test_Config();

                if (sensor_configured == 1)
                {
                    UART_WriteLine("MAX30102 detectado y configurado correctamente");
                }
                else
                {
                    UART_WriteLine("MAX30102 detectado, pero fallo la configuracion");
                }
            }
            else
            {
                Show_Error_On_OLED(part_id);
            }
        }
        else
        {
            /*
             * Si el sensor ya fue configurado, se leen RED e IR.
             */
            sensor_ok = MAX30102_Test_ReadFIFO(&red_value, &ir_value);

            if (sensor_ok == 1)
            {
                finger_detected = Is_Finger_Detected(ir_value);
            }
            else
            {
                sensor_configured = 0;
                finger_detected = 0;
            }
        }

        /*
         * Actualizacion de OLED cada 500 ms.
         */
        display_counter++;

        if (display_counter >= DISPLAY_UPDATE_COUNT)
        {
            display_counter = 0;

            if (sensor_configured == 1)
            {
                Show_Data_On_OLED(sensor_ok,
                                  part_id,
                                  rev_id,
                                  red_value,
                                  ir_value,
                                  finger_detected);
            }
            else
            {
                Show_Error_On_OLED(part_id);
            }
        }

        /*
         * Envio UART cada 1 segundo.
         */
        uart_counter++;

        if (uart_counter >= UART_UPDATE_COUNT)
        {
            uart_counter = 0;

            Send_Test_By_UART(sensor_configured,
                              part_id,
                              rev_id,
                              red_value,
                              ir_value,
                              finger_detected);
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
     * Comparadores apagados.
     */
    CMCON = 0x07;
    CVRCON = 0x00;

    UART_Init();

    /*
     * I2C a 100 kHz.
     */
    I2C_Master_Init(100000UL);

    SSD1306_Init();
    SSD1306_ClearDisplay();
}

/*
 * Lee un registro del MAX30102.
 *
 * Retorna:
 *   1 = lectura correcta
 *   0 = error I2C
 */
static uint8_t MAX30102_Test_ReadRegister(uint8_t reg, uint8_t *value)
{
    uint8_t nack;

    I2C_Master_Start();

    nack = I2C_Master_Write(MAX30102_I2C_WRITE);

    if (nack)
    {
        I2C_Master_Stop();
        return 0;
    }

    nack = I2C_Master_Write(reg);

    if (nack)
    {
        I2C_Master_Stop();
        return 0;
    }

    I2C_Master_RepeatedStart();

    nack = I2C_Master_Write(MAX30102_I2C_READ);

    if (nack)
    {
        I2C_Master_Stop();
        return 0;
    }

    /*
     * Se lee un solo byte, por eso se responde con NACK.
     */
    *value = I2C_Master_Read(0);

    I2C_Master_Stop();

    return 1;
}

/*
 * Escribe un registro del MAX30102.
 *
 * Retorna:
 *   1 = escritura correcta
 *   0 = error I2C
 */
static uint8_t MAX30102_Test_WriteRegister(uint8_t reg, uint8_t value)
{
    uint8_t nack;

    I2C_Master_Start();

    nack = I2C_Master_Write(MAX30102_I2C_WRITE);

    if (nack)
    {
        I2C_Master_Stop();
        return 0;
    }

    nack = I2C_Master_Write(reg);

    if (nack)
    {
        I2C_Master_Stop();
        return 0;
    }

    nack = I2C_Master_Write(value);

    if (nack)
    {
        I2C_Master_Stop();
        return 0;
    }

    I2C_Master_Stop();

    return 1;
}

/*
 * Lee FIFO del MAX30102.
 * Obtiene 3 bytes RED y 3 bytes IR.
 *
 * Retorna:
 *   1 = lectura correcta
 *   0 = error I2C
 */
static uint8_t MAX30102_Test_ReadFIFO(uint32_t *red_value, uint32_t *ir_value)
{
    uint8_t data[6];
    uint8_t i;
    uint8_t nack;

    I2C_Master_Start();

    nack = I2C_Master_Write(MAX30102_I2C_WRITE);

    if (nack)
    {
        I2C_Master_Stop();
        return 0;
    }

    nack = I2C_Master_Write(MAX30102_REG_FIFO_DATA);

    if (nack)
    {
        I2C_Master_Stop();
        return 0;
    }

    I2C_Master_RepeatedStart();

    nack = I2C_Master_Write(MAX30102_I2C_READ);

    if (nack)
    {
        I2C_Master_Stop();
        return 0;
    }

    for (i = 0; i < 6; i++)
    {
        if (i < 5)
        {
            data[i] = I2C_Master_Read(1);
        }
        else
        {
            data[i] = I2C_Master_Read(0);
        }
    }

    I2C_Master_Stop();

    *red_value = (((uint32_t)data[0] << 16) |
                  ((uint32_t)data[1] << 8)  |
                  ((uint32_t)data[2]));

    *ir_value = (((uint32_t)data[3] << 16) |
                 ((uint32_t)data[4] << 8)  |
                 ((uint32_t)data[5]));

    /*
     * El MAX30102 entrega datos de 18 bits.
     */
    *red_value &= 0x03FFFF;
    *ir_value  &= 0x03FFFF;

    return 1;
}

/*
 * Configuracion basica del MAX30102.
 *
 * Retorna:
 *   1 = configurado correctamente
 *   0 = error I2C
 */
static uint8_t MAX30102_Test_Config(void)
{
    uint8_t dummy;

    /*
     * Reset del sensor.
     */
    if (MAX30102_Test_WriteRegister(MAX30102_REG_MODE_CONFIG, 0x40) == 0)
    {
        return 0;
    }

    __delay_ms(100);

    /*
     * Limpieza de FIFO.
     */
    if (MAX30102_Test_WriteRegister(MAX30102_REG_FIFO_WR_PTR, 0x00) == 0)
    {
        return 0;
    }

    if (MAX30102_Test_WriteRegister(MAX30102_REG_OVF_COUNTER, 0x00) == 0)
    {
        return 0;
    }

    if (MAX30102_Test_WriteRegister(MAX30102_REG_FIFO_RD_PTR, 0x00) == 0)
    {
        return 0;
    }

    /*
     * FIFO_CONFIG = 0x1F:
     * Promedio de muestras = 1.
     * Rollover habilitado.
     */
    if (MAX30102_Test_WriteRegister(MAX30102_REG_FIFO_CONFIG, 0x1F) == 0)
    {
        return 0;
    }

    /*
     * MODE_CONFIG = 0x03:
     * Modo SpO2: activa RED + IR.
     */
    if (MAX30102_Test_WriteRegister(MAX30102_REG_MODE_CONFIG, 0x03) == 0)
    {
        return 0;
    }

    /*
     * SPO2_CONFIG = 0x27:
     * ADC range 4096 nA.
     * Sample rate 100 Hz.
     * Pulse width 411 us.
     */
    if (MAX30102_Test_WriteRegister(MAX30102_REG_SPO2_CONFIG, 0x27) == 0)
    {
        return 0;
    }

    /*
     * Corriente de LED RED e IR.
     */
    if (MAX30102_Test_WriteRegister(MAX30102_REG_LED1_PA, 0x24) == 0)
    {
        return 0;
    }

    if (MAX30102_Test_WriteRegister(MAX30102_REG_LED2_PA, 0x24) == 0)
    {
        return 0;
    }

    /*
     * Limpia interrupciones leyendo registros de estado.
     */
    (void)MAX30102_Test_ReadRegister(MAX30102_REG_INT_STATUS_1, &dummy);
    (void)MAX30102_Test_ReadRegister(MAX30102_REG_INT_STATUS_2, &dummy);

    return 1;
}

/*
 * Verifica PART_ID y REV_ID.
 *
 * Retorna:
 *   1 = MAX30102 detectado
 *   0 = error o PART_ID incorrecto
 */
static uint8_t MAX30102_Test_CheckID(uint8_t *rev_id, uint8_t *part_id)
{
    uint8_t ok_part;
    uint8_t ok_rev;

    *part_id = 0;
    *rev_id = 0;

    ok_part = MAX30102_Test_ReadRegister(MAX30102_REG_PART_ID, part_id);
    ok_rev = MAX30102_Test_ReadRegister(MAX30102_REG_REV_ID, rev_id);

    if ((ok_part == 1) &&
        (ok_rev == 1) &&
        (*part_id == MAX30102_EXPECTED_PART_ID))
    {
        return 1;
    }

    return 0;
}

/*
 * Deteccion simple de dedo usando IR.
 */
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

static void UInt8_ToHexString(uint8_t value, char *buffer)
{
    uint8_t high_nibble;
    uint8_t low_nibble;

    high_nibble = (value >> 4) & 0x0F;
    low_nibble = value & 0x0F;

    buffer[0] = '0';
    buffer[1] = 'x';

    if (high_nibble < 10)
    {
        buffer[2] = (char)('0' + high_nibble);
    }
    else
    {
        buffer[2] = (char)('A' + high_nibble - 10);
    }

    if (low_nibble < 10)
    {
        buffer[3] = (char)('0' + low_nibble);
    }
    else
    {
        buffer[3] = (char)('A' + low_nibble - 10);
    }

    buffer[4] = '\0';
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

static void Show_Start_On_OLED(void)
{
    SSD1306_ClearDisplay();

    SSD1306_SetCursor(10, 0);
    SSD1306_WriteString("MAX30102 TEST");

    SSD1306_SetCursor(10, 2);
    SSD1306_WriteString("Iniciando...");

    SSD1306_SetCursor(10, 4);
    SSD1306_WriteString("I2C RB0/RB1");

    SSD1306_SetCursor(10, 6);
    SSD1306_WriteString("OLED activa");
}

static void Show_Error_On_OLED(uint8_t part_id)
{
    char text[8];

    SSD1306_ClearDisplay();

    SSD1306_SetCursor(10, 0);
    SSD1306_WriteString("MAX30102 ERROR");

    SSD1306_SetCursor(10, 2);
    SSD1306_WriteString("I2C: NO RESP");

    SSD1306_SetCursor(10, 4);
    SSD1306_WriteString("PART:");
    UInt8_ToHexString(part_id, text);
    SSD1306_WriteString(text);

    SSD1306_SetCursor(10, 6);
    SSD1306_WriteString("Revise SDA/SCL");
}

static void Show_Data_On_OLED(uint8_t sensor_ok,
                              uint8_t part_id,
                              uint8_t rev_id,
                              uint32_t red_value,
                              uint32_t ir_value,
                              uint8_t finger_detected)
{
    char text[12];

    SSD1306_ClearDisplay();

    /*
     * Linea superior.
     */
    SSD1306_SetCursor(10, 0);

    if (sensor_ok)
    {
        SSD1306_WriteString("MAX30102 OK");
    }
    else
    {
        SSD1306_WriteString("MAX30102 ERR");
    }

    /*
     * RED.
     */
    SSD1306_SetCursor(10, 2);
    SSD1306_WriteString("RED:");
    UInt32_ToString(red_value, text);
    SSD1306_WriteString(text);

    /*
     * IR.
     */
    SSD1306_SetCursor(10, 4);
    SSD1306_WriteString("IR:");
    UInt32_ToString(ir_value, text);
    SSD1306_WriteString(text);

    /*
     * Estado de dedo.
     */
    SSD1306_SetCursor(10, 6);

    if (finger_detected)
    {
        SSD1306_WriteString("Dedo: SI ");
    }
    else
    {
        SSD1306_WriteString("Dedo: NO ");
    }

    SSD1306_WriteString("P:");
    UInt8_ToHexString(part_id, text);
    SSD1306_WriteString(text);
}

static void Send_Test_By_UART(uint8_t sensor_ok,
                              uint8_t part_id,
                              uint8_t rev_id,
                              uint32_t red_value,
                              uint32_t ir_value,
                              uint8_t finger_detected)
{
    char text[12];

    UART_WriteLine("");
    UART_WriteLine("----- PRUEBA MAX30102 -----");

    if (sensor_ok)
    {
        UART_WriteLine("Estado I2C: MAX30102 detectado");
    }
    else
    {
        UART_WriteLine("Estado I2C: sensor no detectado");
    }

    UART_WriteString("PART_ID: ");
    UInt8_ToHexString(part_id, text);
    UART_WriteLine(text);

    UART_WriteString("REV_ID: ");
    UInt8_ToHexString(rev_id, text);
    UART_WriteLine(text);

    UART_WriteString("RED: ");
    UInt32_ToString(red_value, text);
    UART_WriteLine(text);

    UART_WriteString("IR: ");
    UInt32_ToString(ir_value, text);
    UART_WriteLine(text);

    if (finger_detected)
    {
        UART_WriteLine("Dedo: SI");
    }
    else
    {
        UART_WriteLine("Dedo: NO");
    }
}