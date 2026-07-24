# DCTECH BLE to USB Keyboard Simulator

Firmware ESP32 que expone un servicio BLE GATT personalizado, recibe un payload
por BLE, lo convierte a texto y lo emite como pulsaciones de un teclado USB HID.
Pensado para pruebas de laboratorio y desarrollo.

## Descripción

El dispositivo se anuncia como periférico BLE DCTECH. Un central BLE (por
ejemplo una app iOS) se conecta e intercambia un payload sobre la característica
GATT del protocolo. Cuando el ESP32 recibe correctamente ese payload:

1. Lo valida y aplica un tamaño máximo configurable.
2. Lo convierte a texto (UTF-8 imprimible tal cual; binario a hexadecimal).
3. Lo encola y, fuera del contexto del callback BLE, lo teclea por USB HID
   añadiendo un salto de línea final.

El host USB conectado recibe las pulsaciones como si procedieran de un teclado
físico.

## Características

- Servidor/cliente BLE GATT con UUID personalizado de 128 bits.
- Escritura de payload desde iOS u otro central BLE.
- Conversión de texto UTF-8 imprimible.
- Conversión a hexadecimal para datos binarios.
- Salida de teclado USB HID (en hardware con USB nativo).
- Procesamiento diferido mediante cola FreeRTOS (no bloquea el callback BLE).
- Configuración centralizada en `src/BleSimulatorConfig.h`.
- Logs de diagnóstico por puerto serie.

## Hardware compatible

| Placa | BLE | USB HID |
| --- | --- | --- |
| **ESP32 clásico** (`esp32dev`, placa actual del proyecto) | Sí | **No** (sin USB nativo) |
| **ESP32-S2 / ESP32-S3** (`esp32-s3-devkitc-1`) | Sí | Sí (USB-OTG nativo) |

El ESP32 clásico **no dispone de USB nativo**: su puerto USB es un adaptador
serie y no puede enumerar como teclado HID. En ese target el firmware BLE
funciona con normalidad y la salida de teclado queda deshabilitada de forma
segura mediante un stub. Para la salida USB HID real se necesita un SoC con
USB-OTG (ESP32-S2 o ESP32-S3).

## Flujo

```
iOS App
   |
   | BLE write
   v
ESP32 BLE GATT Server
   |
   | Payload queue (FreeRTOS)
   v
USB HID Keyboard
   |
   v
Connected computer or USB host
```

## Configuración BLE

Todos estos valores son internos de laboratorio y viven en
`src/BleSimulatorConfig.h`:

| Parámetro | Valor |
| --- | --- |
| Nombre anunciado | `DCTECH` |
| Service UUID | `fe029319-5f83-447a-8f1c-c7c19fa19201` |
| Characteristic UUID | `fe029319-5f83-447a-8f1c-c7c19fa19202` |
| Propiedades de la característica | notify + write (`.write` / `.writeWithoutResponse`) |
| Manufacturer ID (interno) | `0xFFF0` (primario), `0xFFF1` (secundario) |
| Tamaño máximo de payload | `256` bytes |

Los manufacturer IDs están en el rango alto `0xFFxx` para no colisionar con
Company Identifiers oficiales de Bluetooth SIG. **No** representan a ningún
fabricante real.

## Formato del payload

- **UTF-8 imprimible** → se teclea tal cual como texto.
- **Binario / no imprimible** → se convierte a hexadecimal en mayúsculas y sin
  separadores. Ejemplo: `01 A4 FF 20` → `01A4FF20`.
- Se añade un **salto de línea (Enter)** al final de cada payload
  (`kAppendEnterAfterPayload`).
- Los payloads vacíos o mayores que el máximo configurado se descartan con un
  log de aviso.

## Documentación para el central iOS

El central BLE debe usar los siguientes valores:

| Elemento | Valor |
| --- | --- |
| Service UUID | `fe029319-5f83-447a-8f1c-c7c19fa19201` |
| Characteristic UUID | `fe029319-5f83-447a-8f1c-c7c19fa19202` |
| Escritura | `.write` o `.writeWithoutResponse` |
| Tamaño máximo | 256 bytes por payload |
| Payload final | siempre hexadecimal en mayúsculas, dos caracteres por byte, sin separadores (`kAlwaysEncodePayloadAsHex`) |
| Ejemplo | `01 23 45 67 89 AB CD EF` → se teclea `0123456789ABCDEF` |
| Enter final | desactivado por defecto (`kAppendEnterAfterPayload = false`) |
| Duplicados | el mismo payload no se teclea dos veces dentro de una misma conexión BLE; el filtro se reinicia al desconectar |

## Compilación y carga

Requiere [PlatformIO](https://platformio.org/). Desde la raíz del proyecto:

```bash
# ESP32 clásico (BLE; sin USB HID)
pio run -e esp32dev
pio run -e esp32dev --target upload

# ESP32-S3 (BLE + USB HID). Logs por el puerto UART, HID por el puerto USB nativo
pio run -e esp32-s3
pio run -e esp32-s3 --target upload

# ESP32-S3 con un solo cable: CDC + HID compuestos por el puerto USB nativo
pio run -e esp32-s3-cdc
pio run -e esp32-s3-cdc --target upload

# Monitor serie
pio device monitor
python3 tools/ble_monitor.py                            # env esp32dev / esp32-s3
python3 tools/ble_monitor.py --port /dev/cu.usbmodem101 # env esp32-s3-cdc
```

Los dos entornos S3 compilan con `-DARDUINO_USB_MODE=0` (pila TinyUSB, única que
ofrece HID). Sólo cambian en `ARDUINO_USB_CDC_ON_BOOT`, es decir en a qué puerto
sale `Serial`.

## Monitor serie (`ble_monitor.py`)

`tools/ble_monitor.py` es la alternativa a `pio device monitor` cuando hace falta
redirigir la salida a `grep`, `tee` o un fichero: miniterm exige un TTY y aborta
con `termios.error` en cuanto se pipea. Además marca cada línea con el tiempo
transcurrido, lo que permite cruzar los logs del ESP32 con los del central BLE.

Sólo necesita `pyserial`. Preparación del entorno, **desde `tools/`**:

```bash
cd tools
python3 -m venv .venv
source .venv/bin/activate
python3 -m pip install --upgrade pip
python3 -m pip install pyserial
python3 ble_monitor.py
```

En sesiones posteriores basta con `source tools/.venv/bin/activate` (el
directorio `tools/.venv/` está ignorado por git).

Opciones útiles:

```bash
python3 ble_monitor.py --reset                    # reinicia la placa y captura el arranque
python3 ble_monitor.py --duration 60 --out s.log  # captura 60 s a fichero
python3 ble_monitor.py --port /dev/cu.usbmodem101 # env esp32-s3-cdc
python3 ble_monitor.py | grep '\[keyboard-bridge\]'
```

Puerto por defecto: `/dev/cu.usbserial-110`. Si no coincide, localiza el real con
`pio device list`; **nunca** uses `/dev/cu.Bluetooth-Incoming-Port`, que es el
puerto Bluetooth de macOS y no la placa.

## Uso

1. Compila y carga el firmware en la placa (usa `esp32-s3` para salida USB HID).
2. Conecta el puerto **USB nativo** de la placa al equipo host.
3. Busca el dispositivo BLE `DCTECH` desde iOS u otro central.
4. Conéctate.
5. Descubre el servicio y la característica.
6. Escribe el payload en la característica.
7. Comprueba que el equipo host recibe las pulsaciones de teclado.

## Seguridad y alcance

- Proyecto para **pruebas controladas y laboratorio**.
- Los UUID, el manufacturer data y la clave AES son **valores internos**, no un
  sistema de autenticación o cifrado de producción.
- La clave de laboratorio (`kMasterKeyHex`) **debe sustituirse** antes de
  cualquier uso real.
- El host USB recibirá las pulsaciones como si procedieran de un teclado físico.

## Arquitectura

El ESP32 es **GAP Peripheral** (anuncia; el central conecta) y **GATT Client**
(descubre, se suscribe y escribe sobre la misma conexión entrante). Como el
central nunca anuncia su servicio, la única vía de acceso a su GATT es el
`conn_handle` de la conexión ya establecida. El payload obtenido por BLE se
reenvía a la salida de teclado USB HID.

Las tres responsabilidades —BLE, transformación de payload y salida USB HID—
están en componentes separados que se comunican por callbacks y una cola:

| Fichero | Responsabilidad |
| --- | --- |
| `main.cpp` | Globales, orden de arranque y cableado entre componentes |
| `BleSimulatorConfig.h` | Configuración: UUIDs, clave AES, manufacturer data, tiempos y opciones USB |
| `BlePeripheralServer.{h,cpp}` | GAP Peripheral: advertising DCTECH, acepta la conexión y entrega el `conn_handle` |
| `MobileBadgeClient.{h,cpp}` | GATT Client sobre la conexión entrante: MTU, discovery, CCCD y bombeo del handshake |
| `ProtocolParser.{h,cpp}` | Máquina de estados del handshake y AES-128-ECB (mbedtls). Sin dependencias de BLE |
| `PayloadConverter.{h,cpp}` | Convierte el payload BLE a texto (UTF-8) o hexadecimal |
| `UsbKeyboardOutput.{h,cpp}` | Abstracción de teclado USB HID: real en ESP32-S2/S3, stub en ESP32 clásico |
| `KeyboardBridge.{h,cpp}` | Cola FreeRTOS: desacopla el callback BLE del tecleo por USB |
| `PayloadStorage.{h,cpp}` | Formateo hex/ASCII y logging de pasos y payload final |
| `tools/ble_monitor.py` | Listener serie apto para pipes (`pio device monitor` exige TTY) |

El callback BLE no realiza trabajo pesado: copia el payload a una cola de
FreeRTOS (por valor, sin estado compartido) y retorna. El tecleo real ocurre en
`loop()`:

```
callback BLE  -> KeyboardBridge::enqueuePayload()   (copia rápida, no bloqueante)
     loop()   -> KeyboardBridge::process()          (convierte y teclea por USB)
```

`UsbKeyboardOutput` decide en tiempo de compilación según el SoC: implementación
real con `USBHIDKeyboard` (TinyUSB, incluido en el core Arduino) en ESP32-S2/S3,
o stub en ESP32 clásico. Un fallo al inicializar USB nunca detiene el servidor
BLE.

## Licencia

MIT. Ver [`LICENSE`](LICENSE).
