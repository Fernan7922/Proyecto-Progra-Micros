/*
 * adc.c
 * IE2023 - Proyecto 2 Etapa 2
 * Autor: Fernando José Guzman 24734
 */
#include "adc.h"

void adc_init(void) {
    ADMUX  = (1 << REFS0);
    ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
}

uint16_t adc_leer(uint8_t canal) {
    ADMUX = (ADMUX & 0xF0) | (canal & 0x0F);
    ADCSRA |= (1 << ADSC);
    while (ADCSRA & (1 << ADSC));
    return ADC;
}