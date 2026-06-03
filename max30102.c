#include "system.h"
#include "i2c_master.h"
#include "max30102.h"

/*
 * Registros del MAX30102.
 */
#define MAX30102_REG_INT_STATUS_1      0x00
#define MAX30102_REG_INT_STATUS_2      0x01
#define MAX30102_REG_FIFO_WR_PTR       0x04
#define MAX30102_REG_OVF_COUNTER       0x05
#define MAX30102_REG_FIFO_RD_PTR       0x06
#define MAX30102_REG_FIFO_DATA         0x07
#define MAX30102_REG_FIFO_CONFIG       0x08
#define MAX30102_REG_MODE_CONFIG       0x09
#define MAX30102_REG_SPO2_CONFIG       0x0A
#define MAX30102_REG_LED1_PA           0x0C
#define MAX30102_REG_LED2_PA           0x0D
#define MAX30102_REG_PART_ID           0xFF

#define MAX30102_PART_ID_VALUE         0x15

/*
 * Valor minimo de IR para intentar calcular BPM.
 */
#define MAX30102_MIN_IR_FOR_HR         20000UL

/*
 * El MAX30102 se configura a 100 Hz.
 * El main llama el algoritmo cada 10 ms aproximadamente.
 */
#define HR_SAMPLE_RATE_HZ              100UL

/*
 * Rango valido de frecuencia cardiaca.
 */
#define HR_MIN_VALID_BPM               45UL
#define HR_MAX_VALID_BPM               130UL

#define HR_MIN_INTERVAL_SAMPLES        ((60UL * HR_SAMPLE_RATE_HZ) / HR_MAX_VALID_BPM)
#define HR_MAX_INTERVAL_SAMPLES        ((60UL * HR_SAMPLE_RATE_HZ) / HR_MIN_VALID_BPM)

/*
 * Tiempo refractario:
 * evita contar dos picos dentro del mismo latido.
 * 40 muestras x 10 ms = 400 ms.
 */
#define HR_REFRACTORY_SAMPLES          40UL

/*
 * Estabilizacion inicial corta:
 * 50 muestras x 10 ms = 500 ms.
 */
#define HR_WARMUP_SAMPLES              50UL

/*
 * Umbral minimo de amplitud AC.
 * Si se queda mucho en "Calculando", bajar a 60.
 * Si da valores falsos altos, subir a 120 o 180.
 */
#define HR_MIN_AC_THRESHOLD            80UL

/*
 * Con 1 latido valido ya muestra valor.
 */
#define HR_REQUIRED_VALID_BEATS        1U

static int32_t hr_dc_average = 0;
static int32_t hr_ac_prev2 = 0;
static int32_t hr_ac_prev1 = 0;
static uint32_t hr_abs_average = 0;

static uint32_t hr_sample_counter = 0;
static uint32_t hr_last_beat_sample = 0;

static uint16_t hr_current_bpm = 0;
static uint8_t hr_valid_beats = 0;

static void MAX30102_WriteRegister(uint8_t reg, uint8_t value)
{
    I2C_Master_Start();
    I2C_Master_Write((MAX30102_I2C_ADDRESS << 1) | 0);
    I2C_Master_Write(reg);
    I2C_Master_Write(value);
    I2C_Master_Stop();
}

static uint8_t MAX30102_ReadRegister(uint8_t reg)
{
    uint8_t value;

    I2C_Master_Start();
    I2C_Master_Write((MAX30102_I2C_ADDRESS << 1) | 0);
    I2C_Master_Write(reg);

    I2C_Master_RepeatedStart();
    I2C_Master_Write((MAX30102_I2C_ADDRESS << 1) | 1);

    value = I2C_Master_Read(0);

    I2C_Master_Stop();

    return value;
}

static void MAX30102_ReadMulti(uint8_t reg, uint8_t *buffer, uint8_t length)
{
    uint8_t i;

    I2C_Master_Start();
    I2C_Master_Write((MAX30102_I2C_ADDRESS << 1) | 0);
    I2C_Master_Write(reg);

    I2C_Master_RepeatedStart();
    I2C_Master_Write((MAX30102_I2C_ADDRESS << 1) | 1);

    for (i = 0; i < length; i++)
    {
        if (i < (length - 1))
        {
            buffer[i] = I2C_Master_Read(1);
        }
        else
        {
            buffer[i] = I2C_Master_Read(0);
        }
    }

    I2C_Master_Stop();
}

uint8_t MAX30102_Init(void)
{
    uint8_t part_id;

    part_id = MAX30102_ReadRegister(MAX30102_REG_PART_ID);

    if (part_id != MAX30102_PART_ID_VALUE)
    {
        return 0;
    }

    /*
     * Reset del sensor.
     */
    MAX30102_WriteRegister(MAX30102_REG_MODE_CONFIG, 0x40);
    __delay_ms(100);

    /*
     * Limpia FIFO.
     */
    MAX30102_WriteRegister(MAX30102_REG_FIFO_WR_PTR, 0x00);
    MAX30102_WriteRegister(MAX30102_REG_OVF_COUNTER, 0x00);
    MAX30102_WriteRegister(MAX30102_REG_FIFO_RD_PTR, 0x00);

    /*
     * FIFO_CONFIG = 0x1F:
     * Promedio de muestras = 1.
     * FIFO rollover habilitado.
     * FIFO almost full = 15.
     */
    MAX30102_WriteRegister(MAX30102_REG_FIFO_CONFIG, 0x1F);

    /*
     * MODE_CONFIG = 0x03:
     * Modo SpO2, activa RED + IR.
     */
    MAX30102_WriteRegister(MAX30102_REG_MODE_CONFIG, 0x03);

    /*
     * SPO2_CONFIG = 0x27:
     * ADC range = 4096 nA.
     * Sample rate = 100 Hz.
     * Pulse width = 411 us.
     */
    MAX30102_WriteRegister(MAX30102_REG_SPO2_CONFIG, 0x27);

    /*
     * Corriente de LEDs.
     * 0x24 da mejor deteccion rapida que 0x1F.
     */
    MAX30102_WriteRegister(MAX30102_REG_LED1_PA, 0x24);
    MAX30102_WriteRegister(MAX30102_REG_LED2_PA, 0x24);

    /*
     * Limpia interrupciones.
     */
    (void)MAX30102_ReadRegister(MAX30102_REG_INT_STATUS_1);
    (void)MAX30102_ReadRegister(MAX30102_REG_INT_STATUS_2);

    MAX30102_ResetHeartRateAlgorithm();

    return 1;
}

uint8_t MAX30102_ReadFIFO(uint32_t *red_value, uint32_t *ir_value)
{
    uint8_t data[6];

    MAX30102_ReadMulti(MAX30102_REG_FIFO_DATA, data, 6);

    *red_value = (((uint32_t)data[0] << 16) |
                  ((uint32_t)data[1] << 8)  |
                  ((uint32_t)data[2]));

    *ir_value = (((uint32_t)data[3] << 16) |
                 ((uint32_t)data[4] << 8)  |
                 ((uint32_t)data[5]));

    /*
     * Datos de 18 bits.
     */
    *red_value &= 0x03FFFF;
    *ir_value  &= 0x03FFFF;

    return 1;
}

void MAX30102_ResetHeartRateAlgorithm(void)
{
    hr_dc_average = 0;
    hr_ac_prev2 = 0;
    hr_ac_prev1 = 0;
    hr_abs_average = 0;

    hr_sample_counter = 0;
    hr_last_beat_sample = 0;

    hr_current_bpm = 0;
    hr_valid_beats = 0;
}

uint8_t MAX30102_ProcessHeartRate(uint32_t ir_value, uint16_t *bpm)
{
    int32_t ac_signal;
    uint32_t abs_ac_signal;
    uint32_t dynamic_threshold;

    uint32_t peak_sample;
    uint32_t interval_samples;
    uint32_t calculated_bpm;

    if (ir_value < MAX30102_MIN_IR_FOR_HR)
    {
        MAX30102_ResetHeartRateAlgorithm();
        return 0;
    }

    hr_sample_counter++;

    if (hr_dc_average == 0)
    {
        hr_dc_average = (int32_t)ir_value;
        hr_ac_prev2 = 0;
        hr_ac_prev1 = 0;
        return 0;
    }

    /*
     * Filtro DC:
     * separa la parte lenta de la senal IR.
     */
    hr_dc_average = ((hr_dc_average * 31L) + (int32_t)ir_value) / 32L;

    /*
     * Componente AC:
     * aqui estan las pulsaciones.
     */
    ac_signal = (int32_t)ir_value - hr_dc_average;

    if (ac_signal < 0)
    {
        abs_ac_signal = (uint32_t)(-ac_signal);
    }
    else
    {
        abs_ac_signal = (uint32_t)ac_signal;
    }

    /*
     * Umbral dinamico sensible.
     */
    hr_abs_average = ((hr_abs_average * 15UL) + abs_ac_signal) / 16UL;

    dynamic_threshold = hr_abs_average / 2UL;

    if (dynamic_threshold < HR_MIN_AC_THRESHOLD)
    {
        dynamic_threshold = HR_MIN_AC_THRESHOLD;
    }

    /*
     * Espera corta de estabilizacion.
     */
    if (hr_sample_counter < HR_WARMUP_SAMPLES)
    {
        hr_ac_prev2 = hr_ac_prev1;
        hr_ac_prev1 = ac_signal;
        return 0;
    }

    /*
     * Deteccion de pico:
     * ac_prev1 debe ser mayor que la muestra anterior,
     * mayor que la muestra actual y superar el umbral.
     */
    if ((hr_ac_prev1 > hr_ac_prev2) &&
        (hr_ac_prev1 > ac_signal) &&
        (hr_ac_prev1 > (int32_t)dynamic_threshold))
    {
        peak_sample = hr_sample_counter - 1UL;

        if ((hr_last_beat_sample == 0UL) ||
            ((peak_sample - hr_last_beat_sample) >= HR_REFRACTORY_SAMPLES))
        {
            if (hr_last_beat_sample != 0UL)
            {
                interval_samples = peak_sample - hr_last_beat_sample;

                if ((interval_samples >= HR_MIN_INTERVAL_SAMPLES) &&
                    (interval_samples <= HR_MAX_INTERVAL_SAMPLES))
                {
                    calculated_bpm = (60UL * HR_SAMPLE_RATE_HZ) / interval_samples;

                    if ((calculated_bpm >= HR_MIN_VALID_BPM) &&
                        (calculated_bpm <= HR_MAX_VALID_BPM))
                    {
                        if (hr_current_bpm == 0)
                        {
                            hr_current_bpm = (uint16_t)calculated_bpm;
                        }
                        else
                        {
                            /*
                             * Suavizado moderado.
                             */
                            hr_current_bpm =
                                (uint16_t)((((uint32_t)hr_current_bpm * 3UL) +
                                            calculated_bpm) / 4UL);
                        }

                        if (hr_valid_beats < 10)
                        {
                            hr_valid_beats++;
                        }
                    }
                }
            }

            hr_last_beat_sample = peak_sample;
        }
    }

    hr_ac_prev2 = hr_ac_prev1;
    hr_ac_prev1 = ac_signal;

    if (hr_valid_beats >= HR_REQUIRED_VALID_BEATS)
    {
        *bpm = hr_current_bpm;
        return 1;
    }

    return 0;
}