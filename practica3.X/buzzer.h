#ifndef BUZZER_H
#define BUZZER_H

#include "system.h"

/*
 * Rango normal de frecuencia cardiaca.
 * Si BPM esta fuera de este rango, se activa la alarma.
 */
#define BPM_MIN_NORMAL              60u
#define BPM_MAX_NORMAL              100u

void Buzzer_Init(void);
void Buzzer_On(void);
void Buzzer_Off(void);

/*
 * Esta funcion evalua la condicion de alarma y controla:
 *   - Buzzer en RD3.
 *   - LED de alarma en RD4.
 *
 * Retorna:
 *   1 = alarma activa
 *   0 = alarma inactiva
 */
uint8_t Buzzer_Update(uint8_t finger_detected,
                      uint8_t bpm_valid,
                      uint16_t bpm);

#endif