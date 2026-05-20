/*
 * servo_t2.c
 * IE2023 - Proyecto 2 Etapa 2
 * Autor: Fernando José Guzmán 24734
 */
#include "servo_t2.h"

// Ticks actuales de cada servo (acceso atómico desde main via funciones)
static volatile uint16_t s3_ticks = S_MID_TICKS;
static volatile uint16_t s4_ticks = S_MID_TICKS;
static volatile uint16_t ovf_cnt  = 0;

ISR(TIMER2_OVF_vect) {
    TCNT2 = 56;     // recargar → próximo overflow en 200 ticks = 100us
    ovf_cnt++;

    // Inicio de período: levantar ambas señales
    if (ovf_cnt == 1) {
        SERVO3_PORT |= (1 << SERVO3_PIN);
        SERVO4_PORT |= (1 << SERVO4_PIN);
    }

    // Bajar cuando se cumple el pulso de cada servo
    if (ovf_cnt >= s3_ticks) SERVO3_PORT &= ~(1 << SERVO3_PIN);
    if (ovf_cnt >= s4_ticks) SERVO4_PORT &= ~(1 << SERVO4_PIN);

    // Reiniciar período
    if (ovf_cnt >= PERIOD_TICKS) ovf_cnt = 0;
}

static uint16_t _map_ticks(uint16_t adc) {
    // ADC 0-1023 → S_MIN_TICKS a S_MAX_TICKS
    return S_MIN_TICKS + (uint16_t)(((uint32_t)adc * (S_MAX_TICKS - S_MIN_TICKS)) / 1023);
}

void servo_t2_init(void) {
    SERVO3_DDR |= (1 << SERVO3_PIN);
    SERVO4_DDR |= (1 << SERVO4_PIN);

    TCCR2A = 0x00;              // modo normal
    TCCR2B = (1 << CS21);      // prescaler /8
    TCNT2  = 56;                // precargar
    TIMSK2 = (1 << TOIE2);     // habilitar overflow ISR
}

void servo3_adc(uint16_t adc) {
    uint16_t t = _map_ticks(adc);
    cli(); s3_ticks = t; sei();     // escritura atómica
}

void servo4_adc(uint16_t adc) {
    uint16_t t = _map_ticks(adc);
    cli(); s4_ticks = t; sei();
}
void servo3_set(uint16_t pulso) {
	// convierte pulso a ticks: ticks = pulso_us / 100
	// pulso viene en unidades de timer (1000-5000)
	// 1 tick = 100us * 2MHz = 200 unidades de timer
	uint16_t t = pulso / 200;
	if (t < S_MIN_TICKS) t = S_MIN_TICKS;
	if (t > S_MAX_TICKS) t = S_MAX_TICKS;
	cli(); s3_ticks = t; sei();
}

void servo4_set(uint16_t pulso) {
	uint16_t t = pulso / 200;
	if (t < S_MIN_TICKS) t = S_MIN_TICKS;
	if (t > S_MAX_TICKS) t = S_MAX_TICKS;
	cli(); s4_ticks = t; sei();
}