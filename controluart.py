# control_proyecto2.py
# IE2023 - Proyecto 2 Etapa 3
# Autor: Fernando Jose Guzman 24734

import serial
import time
import threading
import queue
from Adafruit_IO import MQTTClient

# Configuracion Adafruit IO
ADAFRUIT_IO_USERNAME = "Ferg7922"
ADAFRUIT_IO_KEY      = "Yourapi"

FEEDS_CONTROL = ["control1", "control2", "control3", "control4"]
FEEDS_VER     = ["garra", "ver2", "ver3", "ver4"]
FEED_MODO     = "modo"

# Configuracion serial
PUERTO   = "COM6"
BAUDRATE = 9600

# Rate limiting para Adafruit IO
UMBRAL_CAMBIO    = 3     # grados minimos de cambio para publicar
INTERVALO_MINIMO = 2.2   # segundos minimos entre publicaciones

# Estado global
angulos_actuales        = [0, 0, 0, 0]
pos_enviadas            = [-10, -10, -10, -10]
ultimo_envio_io         = 0
escuchando_local        = False
control_adafruit_activo = False
telemetria_pausada      = False

serial_lock = threading.Lock()

# Cola de comandos servo desde MQTT (solo guardamos el mas reciente)
cola_servo = queue.Queue(maxsize=1)
ultimo_aio = [None, None, None, None]

# Nano y cliente MQTT (se inicializan en main)
nano   = None
client = None

# ── Funciones serial ───────────────────────────────────────────

def enviar_comando_raw(cmd):
    with serial_lock:
        nano.write((cmd + "\n").encode("utf-8"))

def enviar(cmd):
    enviar_comando_raw(cmd)
    time.sleep(0.05)

def enviar_esperar(cmd):
    enviar(cmd)
    time.sleep(0.20)
    res = ""
    with serial_lock:
        while nano.in_waiting:
            res += nano.readline().decode("utf-8", errors="ignore").strip() + "\n"
    return res.strip()

def conectar():
    global nano
    try:
        n = serial.Serial(PUERTO, BAUDRATE, timeout=0.1)
        time.sleep(2)
        while n.in_waiting:
            n.readline()
        print(f"[OK] Conectado en {PUERTO}")
        return n
    except Exception as e:
        print(f"[ERROR] {e}")
        return None

# ── Callbacks MQTT ──────────────────────────────────────────────

def connected(client):
    client.subscribe(FEED_MODO)
    for f in FEEDS_CONTROL:
        client.subscribe(f)
    print("[AIO] Conectado y suscrito")

def message(client, feed_id, payload):
    if not control_adafruit_activo:
        return
    if feed_id not in FEEDS_CONTROL:
        return
    try:
        idx   = FEEDS_CONTROL.index(feed_id)
        valor = max(0, min(180, int(float(payload))))
        ultimo_aio[idx] = valor
        while not cola_servo.empty():
            try:
                cola_servo.get_nowait()
            except queue.Empty:
                break
        snapshot      = list(angulos_actuales)
        snapshot[idx] = valor
        try:
            cola_servo.put_nowait(snapshot)
        except queue.Full:
            pass
    except (ValueError, IndexError):
        pass

# ── Hilo consumidor de cola servo (debounce sliders AIO) ────────

def hilo_consumidor_servo():
    while True:
        try:
            angulos = cola_servo.get(timeout=0.5)
            cmd = f"SERVO:{angulos[0]},{angulos[1]},{angulos[2]},{angulos[3]}"
            enviar_comando_raw(cmd)
            for i in range(4):
                angulos_actuales[i] = angulos[i]
            # Publicar el valor movido al feed de visualizacion
            # para que aparezca en la grafica del dashboard
             # Limpiar flags MQTT
            for i in range(4):
                ultimo_aio[i] = None
            time.sleep(0.08)
        except queue.Empty:
            pass
        except Exception:
            pass

# ── Hilo de telemetria ─────────────────────────────────────────

def hilo_telemetria_fondo():
    global angulos_actuales, pos_enviadas, ultimo_envio_io
    while True:
        # Leer lineas ANG: del Nano
        try:
            if nano.in_waiting:
                with serial_lock:
                    linea = nano.readline().decode("utf-8", errors="ignore").strip()
                if linea.startswith("ANG:"):
                    partes = linea[4:].split(",")
                    if len(partes) == 4:
                        nuevos = [int(p) for p in partes]
                        angulos_actuales = nuevos
                        if escuchando_local:
                            print(
                                f"\r  S1:{nuevos[0]:3}  S2:{nuevos[1]:3}  "
                                f"S3:{nuevos[2]:3}  S4:{nuevos[3]:3}  ",
                                end="", flush=True
                            )
        except Exception:
            pass

        # Publicar al dashboard si hay cambio significativo
        if not telemetria_pausada:
            ahora = time.time()
            if (ahora - ultimo_envio_io) >= INTERVALO_MINIMO:
                mejor_idx  = -1
                max_cambio = 0
                for i in range(4):
                    dif = abs(angulos_actuales[i] - pos_enviadas[i])
                    if dif > max_cambio and dif >= UMBRAL_CAMBIO:
                        max_cambio = dif
                        mejor_idx  = i
                if mejor_idx != -1:
                    try:
                        client.publish(FEEDS_VER[mejor_idx], angulos_actuales[mejor_idx])
                        pos_enviadas[mejor_idx] = angulos_actuales[mejor_idx]
                        ultimo_envio_io = ahora
                    except Exception:
                        pass

        time.sleep(0.02)

# ── Modos ──────────────────────────────────────────────────────

def modo_manual():
    global escuchando_local
    print("\n" + "="*50)
    print("  MODO MANUAL")
    print("  Mueve los potenciometros para controlar los servos.")
    print("  Presiona 1, 2, 3 o 4 y Enter para guardar esa posicion.")
    print("  La posicion se sobreescribe si ya habia algo guardado.")
    print("  Escribe 'salir' para volver al menu.")
    print("="*50)
    enviar("MODE:0")
    escuchando_local = True
    while True:
        cmd = input("\n  [Manual] > ").strip()
        if cmd.lower() == "salir":
            break
        elif cmd in ("1", "2", "3", "4"):
            # Enviar el numero al Nano para que guarde la posicion actual
            resp = enviar_esperar(cmd)
            if resp:
                # Formato respuesta: GUARDADO:S1:ang1,ang2,ang3,ang4
                for linea in resp.split("\n"):
                    linea = linea.strip()
                    if linea.startswith("GUARDADO:"):
                        partes = linea.split(":")
                        if len(partes) == 3:
                            slot   = partes[1]   # "S1"
                            angstr = partes[2]   # "12,45,90,120"
                            vals   = angstr.split(",")
                            if len(vals) == 4:
                                print(
                                    f"  Posicion {slot} guardada: "
                                    f"S1={vals[0]}  S2={vals[1]}  "
                                    f"S3={vals[2]}  S4={vals[3]}"
                                )
                    elif linea.startswith("ERR"):
                        print(f"  Error: {linea}")
            else:
                print("  Sin respuesta del Nano.")
        else:
            print("  Comandos validos: 1, 2, 3, 4, salir")
    escuchando_local = False
    print()

def modo_eeprom():
    enviar("MODE:1")
    while True:
        print("\n" + "="*50)
        print("  MODO EEPROM")
        print("  Hay 4 posiciones guardadas (slots 1 a 4).")
        print("  Para guardar: ir al Modo Manual y presiona 1-4.")
        print("  Aqui solo puedes cargar y reproducir posiciones.")
        print("-"*50)
        print("  1. Cargar una posicion guardada")
        print("  2. Ver todas las posiciones guardadas")
        print("  3. Reproducir secuencia completa")
        print("  4. Borrar todas las posiciones")
        print("  0. Volver")
        print("="*50)
        op = input("  Opcion > ").strip()

        if op == "1":
            slot = input("  Que posicion cargar? (1-4) > ").strip()
            if slot not in ("1", "2", "3", "4"):
                print("  Valor invalido. Ingresa 1, 2, 3 o 4.")
                continue
            resp = enviar_esperar(slot)
            if not resp:
                print("  Sin respuesta del Nano.")
                continue
            for linea in resp.split("\n"):
                linea = linea.strip()
                # Formato: SLOT1:ang1,ang2,ang3,ang4
                if linea.startswith("SLOT"):
                    partes = linea.split(":")
                    if len(partes) == 2:
                        num  = partes[0].replace("SLOT", "")
                        vals = partes[1].split(",")
                        if len(vals) == 4:
                            print(
                                f"  Posicion {num} cargada: "
                                f"S1={vals[0]}  S2={vals[1]}  "
                                f"S3={vals[2]}  S4={vals[3]}"
                            )
                elif linea.startswith("ERR"):
                    print(f"  Error: {linea}")

        elif op == "2":
            resp = enviar_esperar("EEPROM:READ")
            if not resp:
                print("  Sin respuesta del Nano.")
                continue
            print()
            for linea in resp.split("\n"):
                linea = linea.strip()
                if linea.startswith("EEPROM:SLOTS:"):
                    pass  # no hace falta mostrar el header
                elif linea.startswith("S") and ":" in linea:
                    # Formato: S1:ang1,ang2,ang3,ang4
                    partes = linea.split(":")
                    if len(partes) == 2:
                        slot = partes[0]
                        vals = partes[1].split(",")
                        if len(vals) == 4:
                            print(
                                f"  {slot} ->  "
                                f"S1={vals[0]:>3}  S2={vals[1]:>3}  "
                                f"S3={vals[2]:>3}  S4={vals[3]:>3}"
                            )

        elif op == "3":
            print("  Reproduciendo las 4 posiciones...")
            enviar("EEPROM:PLAY")
            tiempo_limite = time.time() + 30
            while time.time() < tiempo_limite:
                if nano.in_waiting:
                    with serial_lock:
                        linea = nano.readline().decode("utf-8", errors="ignore").strip()
                    # Durante reproduccion el Nano manda SLOT1:..., SLOT2:... etc
                    if linea.startswith("SLOT"):
                        partes = linea.split(":")
                        if len(partes) == 2:
                            num  = partes[0].replace("SLOT", "")
                            vals = partes[1].split(",")
                            if len(vals) == 4:
                                print(
                                    f"  Posicion {num}: "
                                    f"S1={vals[0]:>3}  S2={vals[1]:>3}  "
                                    f"S3={vals[2]:>3}  S4={vals[3]:>3}"
                                )
                    elif linea == "EEPROM:FIN":
                        print("  Secuencia completada.")
                        break
                    elif linea == "EEPROM:VACIA":
                        print("  No hay posiciones guardadas.")
                        break
                time.sleep(0.05)

        elif op == "4":
            confirmar = input("  Borrar todas las posiciones? (s/n) > ").strip().lower()
            if confirmar == "s":
                resp = enviar_esperar("EEPROM:CLEAR")
                if "BORRADA" in resp:
                    print("  Posiciones borradas.")
                else:
                    print(f"  Respuesta: {resp}")

        elif op == "0":
            break

def modo_uart():
    global control_adafruit_activo, telemetria_pausada
    enviar("MODE:2")

    while True:
        print("\n" + "="*50)
        print("  MODO UART")
        print("  1. Control por comandos desde PC")
        print("  2. Control por sliders en Adafruit IO")
        print("  3. Solo monitorear pots en el Dashboard")
        print("  0. Volver")
        print("="*50)
        op = input("  Sub-modo > ").strip()

        if op == "1":
            control_adafruit_activo = False
            telemetria_pausada      = False
            print("  Comandos disponibles: mover, centro, salir")
            while True:
                cmd = input("  [UART-PC] > ").strip().lower()
                if cmd == "salir":
                    break
                elif cmd == "centro":
                    resp = enviar_esperar("SERVO:90,90,90,135")
                    print(f"  Nano: {resp}")
                elif cmd == "mover":
                    try:
                        print("  Ingresa angulo para cada servo:")
                        vals = [int(input(f"    Servo {i+1} > ")) for i in range(4)]
                        vals = [max(0, min(180, v)) for v in vals]
                        resp = enviar_esperar(
                            f"SERVO:{vals[0]},{vals[1]},{vals[2]},{vals[3]}"
                        )
                        print(f"  Nano: {resp}")
                    except ValueError:
                        print("  Valor invalido.")
                else:
                    print("  Comandos validos: mover, centro, salir")

        elif op == "2":
            telemetria_pausada      = True
            control_adafruit_activo = True
            print()
            print("  Control por sliders activo.")
            print("  Mueve los sliders en el Dashboard de Adafruit IO.")
            print("  Telemetria pausada para no exceder el rate limit.")
            print("  Presiona Enter para detener...")
            input()
            control_adafruit_activo = False
            telemetria_pausada      = False
            while not cola_servo.empty():
                try:
                    cola_servo.get_nowait()
                except queue.Empty:
                    break
            print("  Control remoto detenido.")

        elif op == "3":
            enviar("MODE:0")
            telemetria_pausada = False
            print("  Pots activos. El Dashboard se actualiza solo.")
            print("  Presiona Enter para volver...")
            input()
            enviar("MODE:2")

        elif op == "0":
            break

def menu_principal():
    while True:
        estado_aio = "CONECTADO" if client.is_connected() else "DESCONECTADO"
        print("\n" + "="*50)
        print("  Proyecto 2 - Control de Servos")
        print(f"  Adafruit IO: {estado_aio}")
        print(
            f"  Posicion actual: "
            f"S1={angulos_actuales[0]}  "
            f"S2={angulos_actuales[1]}  "
            f"S3={angulos_actuales[2]}  "
            f"S4={angulos_actuales[3]}"
        )
        print("="*50)
        print("  1. Modo Manual  (potenciometros + guardar posiciones)")
        print("  2. Modo EEPROM  (cargar y reproducir posiciones)")
        print("  3. Modo UART    (control PC o Adafruit IO)")
        print("  0. Salir")
        print("="*50)
        op = input("  Selecciona > ").strip()
        if   op == "1": modo_manual()
        elif op == "2": modo_eeprom()
        elif op == "3": modo_uart()
        elif op == "0":
            nano.close()
            print("  Hasta luego.")
            break

# ── Entry point ────────────────────────────────────────────────

if __name__ == "__main__":
    nano = conectar()
    if not nano:
        exit(1)

    client = MQTTClient(ADAFRUIT_IO_USERNAME, ADAFRUIT_IO_KEY)
    client.on_connect = connected
    client.on_message = message
    client.connect()
    client.loop_background()

    t_consumidor = threading.Thread(target=hilo_consumidor_servo, daemon=True)
    t_consumidor.start()

    t_telemetria = threading.Thread(target=hilo_telemetria_fondo, daemon=True)
    t_telemetria.start()

    menu_principal()