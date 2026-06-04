#include "freeze_switch.h"

/*
 * Interruptor de congelado:
 * RB2 = entrada digital.
 *
 * Conexion recomendada:
 *   RB2 ---- interruptor ---- GND
 *   RB2 ---- resistencia 10k ---- VDD
 *
 * Activo en 0 porque al cerrar el interruptor RB2 queda en GND.
 */
#define FREEZE_SWITCH_TRIS          TRISBbits.TRISB2
#define FREEZE_SWITCH_PORT          PORTBbits.RB2
#define FREEZE_SWITCH_ACTIVE_LEVEL  0u

/*
 * Antirrebote:
 * El ciclo principal del main trabaja cada 10 ms.
 * 5 conteos equivalen aproximadamente a 50 ms.
 */
#define FREEZE_DEBOUNCE_COUNT       5u

void Freeze_Switch_Init(void)
{
    /*
     * RB2 como entrada.
     */
    FREEZE_SWITCH_TRIS = 1;

    /*
     * Habilita los pull-ups debiles internos del PORTB.
     * Aun asi, se recomienda usar resistencia externa de 10k.
     *
     * Importante:
     * Esto no reemplaza las resistencias pull-up externas del bus I2C.
     */
    INTCON2bits.RBPU = 0;
}

uint8_t Freeze_Switch_Update(void)
{
    static uint8_t stable_state = 0;
    static uint8_t last_raw_state = 0;
    static uint8_t debounce_counter = 0;

    uint8_t raw_state;

    /*
     * Si RB2 esta en 0, el interruptor esta activo.
     */
    if (FREEZE_SWITCH_PORT == FREEZE_SWITCH_ACTIVE_LEVEL)
    {
        raw_state = 1;
    }
    else
    {
        raw_state = 0;
    }

    /*
     * Antirrebote por software.
     */
    if (raw_state != last_raw_state)
    {
        last_raw_state = raw_state;
        debounce_counter = 0;
    }
    else
    {
        if (debounce_counter < FREEZE_DEBOUNCE_COUNT)
        {
            debounce_counter++;
        }
        else
        {
            stable_state = raw_state;
        }
    }

    return stable_state;
}