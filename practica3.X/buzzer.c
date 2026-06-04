#include "buzzer.h"

/*
 * Pin usado para el buzzer:
 * RD3 = salida digital.
 *
 * Pin usado para LED de alarma:
 * RD4 = salida digital.
 *
 * Condicion:
 *   Si BPM < 60 o BPM > 100:
 *      - buzzer encendido
 *      - LED parpadea 0.5 s apagado / 0.5 s encendido
 *
 *   Si BPM esta entre 60 y 100:
 *      - buzzer apagado
 *      - LED apagado
 */

#define BUZZER_TRIS                 TRISDbits.TRISD3
#define BUZZER_LAT                  LATDbits.LATD3

#define ALARM_LED_TRIS              TRISDbits.TRISD4
#define ALARM_LED_LAT               LATDbits.LATD4

#define BUZZER_ON_LEVEL             1u
#define BUZZER_OFF_LEVEL            0u

#define ALARM_LED_ON_LEVEL          1u
#define ALARM_LED_OFF_LEVEL         0u

/*
 * El ciclo principal del main trabaja cada 10 ms.
 *
 * 50 ciclos x 10 ms = 500 ms = 0.5 segundos.
 */
#define ALARM_LED_BLINK_COUNT       50u

void Buzzer_Init(void)
{
    BUZZER_TRIS = 0;
    ALARM_LED_TRIS = 0;

    BUZZER_LAT = BUZZER_OFF_LEVEL;
    ALARM_LED_LAT = ALARM_LED_OFF_LEVEL;
}

void Buzzer_On(void)
{
    BUZZER_LAT = BUZZER_ON_LEVEL;
}

void Buzzer_Off(void)
{
    BUZZER_LAT = BUZZER_OFF_LEVEL;
    ALARM_LED_LAT = ALARM_LED_OFF_LEVEL;
}

uint8_t Buzzer_Update(uint8_t finger_detected,
                      uint8_t bpm_valid,
                      uint16_t bpm)
{
    static uint8_t previous_alarm_active = 0;
    static uint8_t led_state = 0;
    static uint16_t blink_counter = 0;

    uint8_t alarm_active = 0;

    /*
     * La alarma solo se evalua cuando:
     *   - hay dedo detectado
     *   - el BPM ya es valido
     */
    if ((finger_detected == 1) && (bpm_valid == 1))
    {
        if ((bpm < BPM_MIN_NORMAL) || (bpm > BPM_MAX_NORMAL))
        {
            alarm_active = 1;
        }
    }

    /*
     * Si la alarma esta activa:
     *   - buzzer encendido
     *   - LED parpadeando cada 0.5 segundos
     */
    if (alarm_active)
    {
        Buzzer_On();

        /*
         * Cuando la alarma se acaba de activar,
         * el LED inicia apagado durante los primeros 0.5 segundos.
         */
        if (previous_alarm_active == 0)
        {
            led_state = 0;
            blink_counter = 0;
            ALARM_LED_LAT = ALARM_LED_OFF_LEVEL;
        }
        else
        {
            blink_counter++;

            if (blink_counter >= ALARM_LED_BLINK_COUNT)
            {
                blink_counter = 0;

                if (led_state == 0)
                {
                    led_state = 1;
                    ALARM_LED_LAT = ALARM_LED_ON_LEVEL;
                }
                else
                {
                    led_state = 0;
                    ALARM_LED_LAT = ALARM_LED_OFF_LEVEL;
                }
            }
        }
    }
    else
    {
        /*
         * Si no hay alarma:
         *   - buzzer apagado
         *   - LED apagado
         *   - contador reiniciado
         */
        Buzzer_Off();

        led_state = 0;
        blink_counter = 0;
    }

    previous_alarm_active = alarm_active;

    return alarm_active;
}
