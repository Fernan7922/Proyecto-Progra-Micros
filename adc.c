/*
 * adc.c
 * IE2023 - Proyecto 2 Etapa 2
 * Autor: Fernando Jose Guzman 24734
 */

#include "adc.h"

/*
 * Inicializa el ADC del ATmega328P.
 *
 * Configuración utilizada:
 * - Referencia AVcc (5V)
 * - ADC habilitado
 * - Prescaler = 128
 *
 * El prescaler divide los 16 MHz del microcontrolador
 * para que el ADC trabaje dentro del rango recomendado.
 */
void adc_init(void) {

    // Selecciona AVcc como referencia de voltaje
    ADMUX = (1 << REFS0);

    /*
     * ADEN  -> habilita el ADC
     * ADPS2:0 -> prescaler 128
     *
     * Frecuencia ADC:
     * 16MHz / 128 = 125kHz
     */
    ADCSRA =
        (1 << ADEN)  |
        (1 << ADPS2) |
        (1 << ADPS1) |
        (1 << ADPS0);
}

/*
 * Lee un canal analógico del ADC.
 *
 * Parámetro:
 * canal -> número de canal ADC (0-7)
 *
 * Retorna:
 * valor de 10 bits entre 0 y 1023
 */
uint16_t adc_leer(uint8_t canal) {

    /*
     * Mantiene la configuración superior de ADMUX
     * y cambia únicamente el canal ADC.
     */
    ADMUX = (ADMUX & 0xF0) | (canal & 0x0F);

    // Inicia la conversión ADC
    ADCSRA |= (1 << ADSC);

    // Espera hasta que termine la conversión
    while (ADCSRA & (1 << ADSC));

    // Retorna el valor convertido
    return ADC;
}
