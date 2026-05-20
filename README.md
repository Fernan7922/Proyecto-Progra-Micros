# Proyecto 2 - Garra Robótica
## IE2023 - Programación de Microcontroladores

### Autor
Fernando José Guzmán González  
Carné: 24734

---

# Descripción

Este proyecto consiste en una garra robótica controlada mediante un ATmega328P utilizando:

- ADC
- PWM
- UART
- EEPROM
- MQTT
- Adafruit IO

El sistema posee tres modos principales:

1. Modo Manual
2. Modo EEPROM
3. Modo UART

Además, el proyecto puede controlarse remotamente desde un dashboard en Adafruit IO.

---

# Características

- Control de 4 servomotores
- Lectura de 4 potenciómetros
- PWM por hardware y software
- Comunicación serial UART
- Guardado de posiciones en EEPROM
- Dashboard IoT con Adafruit IO
- Telemetría en tiempo real
- Control remoto mediante MQTT

---

# Estructura del Proyecto

```text
Firmware/
│
├── main.c
├── adc.c
├── adc.h
├── servo_t1.c
├── servo_t1.h
├── servo_t2.c
└── servo_t2.h

Python/
│
└── controluart.py
