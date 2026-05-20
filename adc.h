/*
 * adc.h
 * IE2023 - Proyecto 2 Etapa 2
 * Autor: Fernando José Guzmán 24734
 */
#ifndef ADC_H_
#define ADC_H_

#include <avr/io.h>

void     adc_init(void);
uint16_t adc_leer(uint8_t canal);

#endif