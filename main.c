/*
 * main.c
 * Proyecto 2 Etapa 3_Final
 * Modos: Manual, EEPROM, UART
 * Autor: Fernando José Guzman 24734
 *
 * Pines:
 *   A0-A3  Potenciómetros 1-4
 *   D9     Servo 1 (OC1A, Timer1)
 *   D10    Servo 2 (OC1B, Timer1)
 *   D11    Servo 3 (Timer2 SW)
 *   D3     Servo 4 (Timer2 SW)
 *   D2     LED Modo Manual  (verde)
 *   D4     LED Modo EEPROM  (amarillo)
 *   D5     LED Modo UART    (rojo)
 *
 * Límites de ángulo por servo:
 *   Servo 1: 0  - 20°
 *   Servo 2: 0  - 50°
 *   Servo 3: 0  - 180°
 *   Servo 4: 95 - 180°
 *
 * EEPROM: 4 slots fijas (teclas 1-4 en modo manual guardan la posición actual)
 *   Dirección 0: no usada (antes N_FRAMES, ahora fijo en 4)
 *   Dirección 1-4: slot 1 (4 bytes), slot 2 (4 bytes), slot 3 (4 bytes), slot 4 (4 bytes)
 */
#define F_CPU 16000000UL
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include <util/delay.h>
#include <string.h>
#include <stdio.h>
#include "adc.h"
#include "servo_t1.h"
#include "servo_t2.h"

// UART
#define BAUD      9600
#define UBRR_VAL  (F_CPU/16/BAUD - 1)

// LEDs de modo
#define LED_MANUAL_PIN  PD2
#define LED_EEPROM_PIN  PD4
#define LED_UART_PIN    PD5
#define LED_PORT        PORTD
#define LED_DDR         DDRD

// EEPROM — 4 slots fijas, cada una ocupa 4 bytes
// Slot k (1-4) empieza en dirección: EEPROM_DATA_ADDR + (k-1)*4
#define EEPROM_DATA_ADDR  1     // byte 0 libre / no usado
#define MAX_SLOTS         4

// CAMBIO: límites de ángulo por servo (mín y máx en grados)
#define S1_MIN  0
#define S1_MAX  30
#define S2_MIN  0
#define S2_MAX  50
#define S3_MIN  0
#define S3_MAX  180
#define S4_MIN  95
#define S4_MAX  180

// Modos
#define MODO_MANUAL  0
#define MODO_EEPROM  1
#define MODO_UART    2

volatile uint8_t modo_actual = MODO_MANUAL;

// Buffer UART
#define BUF_SIZE 32
char    rx_buf[BUF_SIZE];
uint8_t rx_idx = 0;
volatile uint8_t cmd_listo = 0;

// UART funciones
void uart_init(void) {
    UBRR0H = (UBRR_VAL >> 8);
    UBRR0L =  UBRR_VAL;
    UCSR0B = (1 << RXEN0) | (1 << TXEN0) | (1 << RXCIE0);
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}
void uart_enviar_char(char c) {
    while (!(UCSR0A & (1 << UDRE0)));
    UDR0 = c;
}
void uart_enviar_string(const char *s) {
    while (*s) uart_enviar_char(*s++);
}

ISR(USART_RX_vect) {
    char c = UDR0;
    if (c == '\n') {
        rx_buf[rx_idx] = '\0';
        cmd_listo = 1;
        rx_idx = 0;
    } else if (rx_idx < BUF_SIZE - 1) {
        rx_buf[rx_idx++] = c;
    }
}

// LEDs
void leds_init(void) {
    LED_DDR |= (1 << LED_MANUAL_PIN) | (1 << LED_EEPROM_PIN) | (1 << LED_UART_PIN);
}
void led_set_modo(uint8_t modo) {
    LED_PORT &= ~((1 << LED_MANUAL_PIN) | (1 << LED_EEPROM_PIN) | (1 << LED_UART_PIN));
    if (modo == MODO_MANUAL) LED_PORT |= (1 << LED_MANUAL_PIN);
    if (modo == MODO_EEPROM) LED_PORT |= (1 << LED_EEPROM_PIN);
    if (modo == MODO_UART)   LED_PORT |= (1 << LED_UART_PIN);
}

// CAMBIO: mapeo ADC ? ángulo dentro del rango permitido por servo
// El pot recorre 0-1023, pero el resultado se escala al rango [ang_min, ang_max]
uint8_t adc_a_angulo_limitado(uint16_t adc, uint8_t ang_min, uint8_t ang_max) {
    uint8_t rango = ang_max - ang_min;
    return ang_min + (uint8_t)(((uint32_t)adc * rango) / 1023);
}

// Para reporte general (sin límites)
uint8_t adc_a_angulo(uint16_t adc) {
    return (uint8_t)(((uint32_t)adc * 180) / 1023);
}

// Conversión ángulo ? pulso (igual que antes)
uint16_t angulo_a_pulso(uint8_t angulo) {
    return 1000 + ((uint32_t)angulo * 4000) / 180;
}

void mover_servo(uint8_t num, uint8_t angulo) {
    uint16_t pulso = angulo_a_pulso(angulo);
    if (num == 1) servo1_set(pulso);
    if (num == 2) servo2_set(pulso);
    if (num == 3) servo3_set(pulso);
    if (num == 4) servo4_set(pulso);
}

// CAMBIO: ángulos actuales leídos del ADC (con límites aplicados)
// Se actualizan en el loop de modo manual para poder guardarlos desde cualquier punto
static uint8_t ang_actual[4] = {0, 0, 0, 95};  // valores iniciales seguros

// EEPROM: guardar en slot fija (1-4)
void eeprom_guardar_slot(uint8_t slot, uint8_t a1, uint8_t a2, uint8_t a3, uint8_t a4) {
    if (slot < 1 || slot > MAX_SLOTS) return;
    uint16_t addr = EEPROM_DATA_ADDR + (uint16_t)(slot - 1) * 4;
    eeprom_write_byte((uint8_t*)addr,     a1);
    eeprom_write_byte((uint8_t*)addr + 1, a2);
    eeprom_write_byte((uint8_t*)addr + 2, a3);
    eeprom_write_byte((uint8_t*)addr + 3, a4);
    char resp[32];
    sprintf(resp, "GUARDADO:S%d:%d,%d,%d,%d\n", slot, a1, a2, a3, a4);
    uart_enviar_string(resp);
}

// EEPROM: leer slot y moverservos + reportar
void eeprom_ir_a_slot(uint8_t slot) {
    if (slot < 1 || slot > MAX_SLOTS) {
        uart_enviar_string("ERR:SLOT\n");
        return;
    }
    uint16_t addr = EEPROM_DATA_ADDR + (uint16_t)(slot - 1) * 4;
    uint8_t a1 = eeprom_read_byte((uint8_t*)addr);
    uint8_t a2 = eeprom_read_byte((uint8_t*)addr + 1);
    uint8_t a3 = eeprom_read_byte((uint8_t*)addr + 2);
    uint8_t a4 = eeprom_read_byte((uint8_t*)addr + 3);

    // Validar que estén dentro de rango antes de mover
    if (a1 > S1_MAX) a1 = S1_MAX;
    if (a2 > S2_MAX) a2 = S2_MAX;
    if (a3 > S3_MAX) a3 = S3_MAX;
    if (a4 < S4_MIN) a4 = S4_MIN;
    if (a4 > S4_MAX) a4 = S4_MAX;

    mover_servo(1, a1);
    mover_servo(2, a2);
    mover_servo(3, a3);
    mover_servo(4, a4);

    char resp[40];
    sprintf(resp, "SLOT%d:%d,%d,%d,%d\n", slot, a1, a2, a3, a4);
    uart_enviar_string(resp);
}

// EEPROM: mostrar todas las slots
void eeprom_enviar_todos(void) {
    uart_enviar_string("EEPROM:SLOTS:4\n");
    for (uint8_t i = 1; i <= MAX_SLOTS; i++) {
        uint16_t addr = EEPROM_DATA_ADDR + (uint16_t)(i - 1) * 4;
        uint8_t a1 = eeprom_read_byte((uint8_t*)addr);
        uint8_t a2 = eeprom_read_byte((uint8_t*)addr + 1);
        uint8_t a3 = eeprom_read_byte((uint8_t*)addr + 2);
        uint8_t a4 = eeprom_read_byte((uint8_t*)addr + 3);
        char linea[32];
        sprintf(linea, "S%d:%d,%d,%d,%d\n", i, a1, a2, a3, a4);
        uart_enviar_string(linea);
    }
}

// EEPROM: reproducir las 4 slots en secuencia
void eeprom_reproducir(void) {
    uart_enviar_string("EEPROM:REPRODUCIENDO\n");
    for (uint8_t i = 1; i <= MAX_SLOTS; i++) {
        eeprom_ir_a_slot(i);
        _delay_ms(1000);
    }
    uart_enviar_string("EEPROM:FIN\n");
}

// Procesar comandos UART
void procesar_comando(char *cmd) {
    // Cambio de modo
    if (strcmp(cmd, "MODE:0") == 0) {
        modo_actual = MODO_MANUAL;
        led_set_modo(MODO_MANUAL);
        uart_enviar_string("OK:MANUAL\n");

    } else if (strcmp(cmd, "MODE:1") == 0) {
        modo_actual = MODO_EEPROM;
        led_set_modo(MODO_EEPROM);
        uart_enviar_string("OK:EEPROM\n");

    } else if (strcmp(cmd, "MODE:2") == 0) {
        modo_actual = MODO_UART;
        led_set_modo(MODO_UART);
        uart_enviar_string("OK:UART\n");

    // CAMBIO: en modo MANUAL, teclas 1-4 guardan posición actual en slot correspondiente
    } else if (modo_actual == MODO_MANUAL &&
               cmd[0] >= '1' && cmd[0] <= '4' && cmd[1] == '\0') {
        uint8_t slot = cmd[0] - '0';
        eeprom_guardar_slot(slot,
            ang_actual[0], ang_actual[1],
            ang_actual[2], ang_actual[3]);

    // CAMBIO: en modo EEPROM, teclas 1-4 van a esa slot
    } else if (modo_actual == MODO_EEPROM &&
               cmd[0] >= '1' && cmd[0] <= '4' && cmd[1] == '\0') {
        uint8_t slot = cmd[0] - '0';
        eeprom_ir_a_slot(slot);

    // EEPROM: leer todas las slots
    } else if (strcmp(cmd, "EEPROM:READ") == 0) {
        eeprom_enviar_todos();

    // EEPROM: reproducir secuencia completa
    } else if (strcmp(cmd, "EEPROM:PLAY") == 0) {
        eeprom_reproducir();

    // EEPROM: borrar todo (pone 0xFF en todo)
    } else if (strcmp(cmd, "EEPROM:CLEAR") == 0) {
        for (uint8_t i = 0; i < MAX_SLOTS; i++) {
            uint16_t addr = EEPROM_DATA_ADDR + (uint16_t)i * 4;
            eeprom_write_byte((uint8_t*)addr,     0xFF);
            eeprom_write_byte((uint8_t*)addr + 1, 0xFF);
            eeprom_write_byte((uint8_t*)addr + 2, 0xFF);
            eeprom_write_byte((uint8_t*)addr + 3, 0xFF);
        }
        uart_enviar_string("EEPROM:BORRADA\n");

    // UART: mover servos directamente "SERVO:a1,a2,a3,a4"
    } else if (strncmp(cmd, "SERVO:", 6) == 0) {
        uint8_t a1, a2, a3, a4;
        sscanf(cmd + 6, "%hhu,%hhu,%hhu,%hhu", &a1, &a2, &a3, &a4);
        // Aplicar límites también en modo UART
        if (a1 > S1_MAX) a1 = S1_MAX;
        if (a2 > S2_MAX) a2 = S2_MAX;
        if (a3 > S3_MAX) a3 = S3_MAX;
        if (a4 < S4_MIN) a4 = S4_MIN;
        if (a4 > S4_MAX) a4 = S4_MAX;
        mover_servo(1, a1);
        mover_servo(2, a2);
        mover_servo(3, a3);
        mover_servo(4, a4);
        char resp[32];
        sprintf(resp, "OK:SERVO:%d,%d,%d,%d\n", a1, a2, a3, a4);
        uart_enviar_string(resp);

    } else {
        uart_enviar_string("ERR:DESCONOCIDO\n");
    }
}

// Main
int main(void) {
    uart_init();
    leds_init();
    adc_init();
    servo_t1_init();
    servo_t2_init();
    sei();

    led_set_modo(MODO_MANUAL);
    uart_enviar_string("NANO:LISTO\n");

    uint16_t contador_manual = 0;

    while (1) {
        if (cmd_listo) {
            cmd_listo = 0;
            procesar_comando(rx_buf);
        }

        if (modo_actual == MODO_MANUAL) {
            uint16_t adc1 = adc_leer(0);
            uint16_t adc2 = adc_leer(1);
            uint16_t adc3 = adc_leer(2);
            uint16_t adc4 = adc_leer(3);

            // CAMBIO: calcular ángulos con límites y guardarlos globalmente
            ang_actual[0] = adc_a_angulo_limitado(adc1, S1_MIN, S1_MAX);
            ang_actual[1] = adc_a_angulo_limitado(adc2, S2_MIN, S2_MAX);
            ang_actual[2] = adc_a_angulo_limitado(adc3, S3_MIN, S3_MAX);
            ang_actual[3] = adc_a_angulo_limitado(adc4, S4_MIN, S4_MAX);

            // Mover servos con los ángulos ya limitados
            mover_servo(1, ang_actual[0]);
            mover_servo(2, ang_actual[1]);
            mover_servo(3, ang_actual[2]);
            mover_servo(4, ang_actual[3]);

            // Reportar cada ~200ms
            contador_manual++;
            if (contador_manual >= 10) {
                contador_manual = 0;
                char linea[40];
                sprintf(linea, "ANG:%d,%d,%d,%d\n",
                    ang_actual[0], ang_actual[1],
                    ang_actual[2], ang_actual[3]);
                uart_enviar_string(linea);
            }
        }

        _delay_ms(20);
    }
}