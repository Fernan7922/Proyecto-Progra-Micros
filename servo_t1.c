/*
 * servo_t1.c
 * IE2023 - Proyecto 2 Etapa 2
 * Autor: Fernando José Guzman 24734
 */
#include "servo_t1.h"

static uint16_t _map(uint16_t adc) {
    return SERVO_MIN + (uint16_t)(((uint32_t)adc * (SERVO_MAX - SERVO_MIN)) / 1023);
}

void servo_t1_init(void) {
    // PB1 (D9) y PB2 (D10) como salidas
    DDRB |= (1 << DDB1) | (1 << DDB2);

    // Fast PWM Modo 14, non-inverting ambos canales
    TCCR1A = (1 << COM1A1) | (1 << COM1B1) | (1 << WGM11);
    TCCR1B = (1 << WGM13)  | (1 << WGM12)  | (1 << CS11);

    ICR1  = ICR1_TOP;
    OCR1A = 3000;   // Servo 1 → 90° inicial
    OCR1B = 3000;   // Servo 2 → 90° inicial
}

void servo1_set(uint16_t pulso) {
    if (pulso < SERVO_MIN) pulso = SERVO_MIN;
    if (pulso > SERVO_MAX) pulso = SERVO_MAX;
    OCR1A = pulso;
}

void servo1_adc(uint16_t adc) { servo1_set(_map(adc)); }

void servo2_set(uint16_t pulso) {
    if (pulso < SERVO_MIN) pulso = SERVO_MIN;
    if (pulso > SERVO_MAX) pulso = SERVO_MAX;
    OCR1B = pulso;
}

void servo2_adc(uint16_t adc) { servo2_set(_map(adc)); }