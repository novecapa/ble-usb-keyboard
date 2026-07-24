#!/usr/bin/env python3
"""Listener serie del simulador DCTECH BLE to USB Keyboard.

Alternativa a `pio device monitor` para cuando hace falta redirigir la salida:
miniterm exige un TTY y aborta con `termios.error: Inappropriate ioctl for
device` en cuanto se pipea a `grep`, a `tee` o se lanza desde un script. Este
listener no lo exige, y ademas marca cada linea con el tiempo transcurrido para
poder cruzar los logs del ESP32 con los del central BLE.

Ejemplos:

    # Escuchar hasta Ctrl+C
    python3 tools/ble_monitor.py

    # Reiniciar la placa y capturar 60 s en un fichero
    python3 tools/ble_monitor.py --reset --duration 60 --out sesion.log

    # Ver solo el handshake
    python3 tools/ble_monitor.py | grep -E '\\[gap\\]|\\[gatt-client\\]|\\[protocol\\]'
"""

import argparse
import sys
import time

try:
    import serial
except ImportError:
    sys.exit("Falta pyserial. Instalalo con: pip install pyserial")

DEFAULT_PORT = "/dev/cu.usbserial-110"
DEFAULT_BAUD = 115200


def parse_args():
    parser = argparse.ArgumentParser(description="Listener serie del simulador DCTECH BLE to USB Keyboard")
    parser.add_argument("--port", default=DEFAULT_PORT,
                        help=f"Puerto serie USB (por defecto {DEFAULT_PORT}). "
                             "Nunca uses /dev/cu.Bluetooth-Incoming-Port.")
    parser.add_argument("--baud", type=int, default=DEFAULT_BAUD, help="Baudios")
    parser.add_argument("--duration", type=float, default=0,
                        help="Segundos a capturar; 0 = hasta Ctrl+C")
    parser.add_argument("--out", help="Fichero donde volcar la captura ademas de stdout")
    parser.add_argument("--reset", action="store_true",
                        help="Pulsar RTS para reiniciar la placa y ver el arranque")
    parser.add_argument("--no-timestamps", action="store_true",
                        help="Emitir las lineas tal cual, sin marca de tiempo")
    return parser.parse_args()


def main():
    args = parse_args()

    if "Bluetooth-Incoming-Port" in args.port:
        sys.exit("Ese no es el puerto de la placa: es el puerto Bluetooth de macOS. "
                 "Localiza el real con `pio device list`.")

    try:
        port = serial.Serial(args.port, args.baud, timeout=1)
    except serial.SerialException as error:
        sys.exit(f"No se pudo abrir {args.port}: {error}\n"
                 "Comprueba que no haya otro monitor ocupandolo: lsof " + args.port)

    if args.reset:
        port.setDTR(False)
        port.setRTS(True)
        time.sleep(0.1)
        port.setRTS(False)

    sink = open(args.out, "w", encoding="utf-8") if args.out else None
    started = time.time()
    deadline = started + args.duration if args.duration else None

    print(f"# escuchando {args.port} @ {args.baud} "
          f"({'Ctrl+C para salir' if not deadline else f'{args.duration:g} s'})",
          file=sys.stderr)

    try:
        while deadline is None or time.time() < deadline:
            raw = port.readline()
            if not raw:
                continue
            text = raw.decode("utf-8", "replace").rstrip("\r\n")
            line = text if args.no_timestamps else f"[{time.time() - started:8.3f}s] {text}"
            print(line, flush=True)
            if sink:
                sink.write(line + "\n")
                sink.flush()
    except KeyboardInterrupt:
        pass
    finally:
        port.close()
        if sink:
            sink.close()
            print(f"# captura guardada en {args.out}", file=sys.stderr)


if __name__ == "__main__":
    main()
