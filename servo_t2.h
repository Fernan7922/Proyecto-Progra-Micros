/*
 * servo_t2.h
 * Timer2 PWM por software usando ISR overflow
 * Servo 3 D11 (PB3)
 * Servo 4  D3  (PD3)
 *
 * Timer2 modo normal, prescaler /8  Ftimer = 2MHz
 * TCNT2 precargado en 56 overflow cada 200 ticks = 100us
 * 200 overflows × 100us = 20ms  período servo 50Hz
 *
 * Resolución: 1 tick = 100us
 * 1ms   10 ticks
 * 1.5ms 15 ticks
 * 2ms  20 ticks
 * Rango: S_MIN_TICKS=10 a S_MAX_TICKS=20
 *
 * IE2023 - Proyecto 2 Etapa 2
 * Autor: Fernando José Guzman 24734
 */
#ifndef SERVO_T2_H_
#define SERVO_T2_H_

#include <avr/io.h>
#include <avr/interrupt.h>

// Rango en ticks de 100us
#define S_MIN_TICKS  5     // 1ms   0°
#define S_MAX_TICKS  30    // 2ms   180°
#define S_MID_TICKS  15     // 1.5ms 90°
#define PERIOD_TICKS 200    // 20ms  período completo

// Pines
#define SERVO3_DDR  DDRB
#define SERVO3_PORT PORTB
#define SERVO3_PIN  PB3

#define SERVO4_DDR  DDRD
#define SERVO4_PORT PORTD
#define SERVO4_PIN  PD3

void servo_t2_init(void);
void servo3_adc(uint16_t adc);
void servo4_adc(uint16_t adc);
void servo3_set(uint16_t pulso);
void servo4_set(uint16_t pulso);

#endif