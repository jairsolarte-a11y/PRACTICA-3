#ifndef FREEZE_SWITCH_H
#define FREEZE_SWITCH_H

#include "system.h"

/*
 * Modulo para interruptor de congelado.
 *
 * Pin usado:
 *   RB2 = interruptor de congelado.
 *
 * Conexion fisica recomendada:
 *
 *   RB2 ---- interruptor ---- GND
 *   RB2 ---- resistencia 10k ---- VDD
 *
 * Funcionamiento:
 *   RB2 = 1 -> modo normal
 *   RB2 = 0 -> modo congelado
 */

void Freeze_Switch_Init(void);
uint8_t Freeze_Switch_Update(void);

#endif