#include <xc.h>

/*
 * ==========================================================
 * Proyecto : Monitor Portatil de Signos Vitales
 * MCU      : PIC18F4550
 * Compilador: XC8
 * Frecuencia: 8 MHz (Oscilador Interno)
 * ==========================================================
 */

/*----------------------------------------------------------
  Configuración del reloj
----------------------------------------------------------*/
#pragma config PLLDIV = 1          // PLL deshabilitado
#pragma config CPUDIV = OSC1_PLL2  // CPU sin división adicional
#pragma config USBDIV = 1          // USB no utiliza PLL

#pragma config FOSC = INTOSCIO_EC  // Oscilador interno, RA6/RA7 como I/O
#pragma config FCMEN = OFF         // Fail-Safe Clock Monitor deshabilitado
#pragma config IESO = OFF          // Internal/External Switchover deshabilitado

/*----------------------------------------------------------
  Alimentación y reset
----------------------------------------------------------*/
#pragma config PWRT = ON           // Power-Up Timer habilitado
#pragma config BOR = OFF           // Brown-Out Reset deshabilitado
#pragma config BORV = 3            // Nivel BOR
#pragma config VREGEN = OFF        // Regulador USB deshabilitado

/*----------------------------------------------------------
  Watchdog Timer
----------------------------------------------------------*/
#pragma config WDT = OFF           // Watchdog deshabilitado
#pragma config WDTPS = 32768       // Postescalador WDT

/*----------------------------------------------------------
  Configuración de pines
----------------------------------------------------------*/
#pragma config MCLRE = ON          // MCLR habilitado
#pragma config LPT1OSC = OFF       // Timer1 en modo normal 
#pragma config PBADEN = OFF        // PORTB digital al iniciar
#pragma config CCP2MX = ON         // CCP2 en RC1

/*----------------------------------------------------------
  Características del sistema
----------------------------------------------------------*/
#pragma config STVREN = ON         // Reset por Stack Overflow
#pragma config LVP = OFF           // Programación de bajo voltaje deshabilitada
#pragma config ICPRT = OFF         // Puerto IC deshabilitado
#pragma config XINST = OFF         // Juego extendido de instrucciones OFF

/*----------------------------------------------------------
  Protección de código
----------------------------------------------------------*/
#pragma config CP0 = OFF
#pragma config CP1 = OFF
#pragma config CP2 = OFF
#pragma config CP3 = OFF
#pragma config CPB = OFF
#pragma config CPD = OFF

/*----------------------------------------------------------
  Protección de escritura
----------------------------------------------------------*/
#pragma config WRT0 = OFF
#pragma config WRT1 = OFF
#pragma config WRT2 = OFF
#pragma config WRT3 = OFF
#pragma config WRTB = OFF
#pragma config WRTC = OFF
#pragma config WRTD = OFF

/*----------------------------------------------------------
  Protección de lectura
----------------------------------------------------------*/
#pragma config EBTR0 = OFF
#pragma config EBTR1 = OFF
#pragma config EBTR2 = OFF
#pragma config EBTR3 = OFF
#pragma config EBTRB = OFF