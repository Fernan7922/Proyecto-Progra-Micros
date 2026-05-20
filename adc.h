/*
 * adc.h
 * IE2023 - Proyecto 2 Etapa 2
 * Autor: Fernando Jose Guzman 24734
 */

#ifndef ADC_H_
#define ADC_H_

#include <avr/io.h>

/*
 * Inicializa el módulo ADC del ATmega328P.
 *
 * Configura:
 * - referencia AVcc
 * - prescaler
 * - habilitación del ADC
 */
void adc_init(void);

/*
 * Lee un canal analógico específico.
 *
 * Parámetro:
 * canal -> canal ADC a utilizar
 *
 * Retorna:
 * valor de 10 bits entre 0 y 1023
 */
uint16_t adc_leer(uint8_t canal);

#endif
