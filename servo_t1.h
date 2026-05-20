/*
 * servo_t1.h
 * Timer1 (16 bits) Fast PWM Modo 14
 * OC1A  Servo 1 (D9, PB1)
 * OC1B  Servo 2 (D10, PB2)
 * Ambos comparten ICR1  50 Hz exacto
 * IE2023 - Proyecto 2 Etapa 2
 * Autor: Fernando José Guzman 24734
 */
#ifndef SERVO_T1_H_
#define SERVO_T1_H_

#include <avr/io.h>

#define ICR1_TOP    39999   // 50 Hz con prescaler /8
#define SERVO_MIN   1200    // 0.5ms 0°
#define SERVO_MAX   5000    // 2.5ms  180°

void servo_t1_init(void);
void servo1_set(uint16_t pulso);
void servo1_adc(uint16_t adc);
void servo2_set(uint16_t pulso);
void servo2_adc(uint16_t adc);

#endif