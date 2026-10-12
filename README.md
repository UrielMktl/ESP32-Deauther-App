# ESP32-Deauther + App móvil (WiFi / Bluetooth) con login + clonador de red

Proyecto para ESP32 que permite desautenticar estaciones WiFi y clonar redes
(Evil-Twin educativo), controlado desde una
**app móvil (Flutter, Android + iOS)** que se conecta al ESP32 por **su WiFi** o por
**Bluetooth Low Energy (BLE)**. Todas las funciones originales se conservan y se añade
un **login** que protege la Web UI y la API.

> DISCLAIMER: solo para fines educativos y pruebas en redes propias. El mal uso es
> responsabilidad del usuario.

## Estructura

```
firmware/   -> código ESP32 (PlatformIO)
              - Mantiene AP WiFi + Web UI + API JSON (funciones originales)
              - Añade LOGIN (credenciales) y BLE (control por Bluetooth)
app/        -> (opcional) app de escritorio Python + tkinter, por WiFi
mobile/     -> app móvil Flutter (WiFi + Bluetooth + login)  <-- la que pediste
```

## Funciones (ninguna se pierde del repo original)

- AP WiFi `ESP32-Deauther` / `esp32wroom32` en `192.168.4.1`
- Rescan, Deauth single (red + reason code), Deauth all, Stop
- **Clonador de red**: clona la red escaneada (mismo SSID/canal) o ingresa SSID (máx 32) + canal 1-13 + pass del AP falso (vacío = abierta, ≥8 = WPA2). Incluye Stop del clon.
- Tabla de reason codes (0–24) y LED que parpadea
- Web UI original en el navegador (ahora tras login)
- Nuevo: **login** (usuario/contraseña) y **control por Bluetooth BLE**

Credenciales por defecto (en `firmware/include/definitions.h`):
- Usuario: `admin`
- Password: `deauther`

## 1. Firmware (ESP32)

1. VSCode + PlatformIO.
2. Abre `firmware/`.
3. Elige tu placa en `platformio.ini` (esp32doit-devkit-v1 por defecto; hay env para
   S3 y C3).
4. "Upload". Al arrancar crea el WiFi `ESP32-Deauther` y se anuncia por BLE como
   `ESP32-Deauther`.

Nota BLE: usa la librería `NimBLE-Arduino` (se instala sola vía `lib_deps`). El BLE
funciona en Android **e iOS** (el Bluetooth clásico SPP no funciona en iOS).

## 2. App móvil (Flutter)

### Preparar
```
cd mobile
flutter pub get
flutter create .        # genera las carpetas android/ e ios si no existen
```
Permisos Bluetooth (ya casi listos, pero confirma):
- `android/app/src/main/AndroidManifest.xml` debe incluir:
  ```xml
  <uses-permission android:name="android.permission.BLUETOOTH"/>
  <uses-permission android:name="android.permission.BLUETOOTH_SCAN"/>
  <uses-permission android:name="android.permission.BLUETOOTH_CONNECT"/>
  <uses-permission android:name="android.permission.ACCESS_FINE_LOCATION"/>
  ```
- `ios/Runner/Info.plist` debe incluir:
  ```xml
  <key>NSBluetoothAlwaysUsageDescription</key>
  <string>Usar BLE para controlar el ESP32</string>
  ```

### Ejecutar en el móvil
```
flutter run
```
(o genera el APK: `flutter build apk`).

### Uso
1. Conéctate al WiFi `ESP32-Deauther` (modo WiFi) **o** deja el Bluetooth activo (modo BLE).
2. En la app elige **WiFi** o **Bluetooth**:
   - WiFi: pon IP `192.168.4.1`, usuario `admin`, password `deauther` → Iniciar sesión.
   - Bluetooth: pulsa "Escanear", elige `ESP32-Deauther`, escribe el password `deauther`.
3. En el panel: **Escanear redes**, selecciona una, elige **Reason code** y pulsa
   **Deauth red**, **Deauth TODAS** o **Detener**.
4. Para clonar: en la sección **Clonador** pulsa **Clonar selec.** (usa la red
   marcada + pass del campo) o escribe **SSID + canal + pass a mano** y pulsa
   **Clonar manual**. **Detener clon** restaura el AP original.

La app habla con el ESP32 así:
- WiFi → endpoints JSON `/api/login`, `/api/scan`, `/api/deauth`, `/api/deauth_all`,
  `/api/stop`, `/api/status`, `/api/clone` (con `net_num+pass` o `ssid+channel+pass`),
  `/api/clone_stop`, `/api/clone_status` (requieren el token del login).
- BLE → servicio UART Nordic (6E400001...): comandos `LOGIN <pass>`, `SCAN`,
  `DEAUTH <net> <reason>`, `DEAUTHALL <reason>`, `STOP`, `STATUS`,
  `CLONE_NET <num> [pass]`, `CLONE <ssid>|<canal>|<pass>`, `CLONE_STOP`, `CLONE_STATUS`.

## 3. App de escritorio (opcional, solo WiFi)
```
cd app
pip install -r requirements.txt
python esp32_deauther_app.py
```
(Usa la API JSON por WiFi; introduce usuario/contraseña si quieres ampliarla, pero
esta versión de escritorio asume que ya hiciste login en el navegador. Puedes adaptarla
fácilmente reusando la lógica del login de la app móvil.)

## Notas
- "Deauth all networks" apaga el AP del ESP32; para parar hay que resetear la placa
  (limitación del hardware, igual que en el repo original). Por BLE puedes enviar
  `STOP` antes de que se apague, pero el ataque global está pensado para reset.
- El **clonador y el deauth se excluyen** (mismo radio): al iniciar uno se apaga el otro.
- Placas: ESP32, ESP32-S3, ESP32-C3.

## Página web (GitHub Pages)
- `index.html` → https://uriel5523l.github.io/ESP32-Deauther-App/ (botón APK + flasheador + guía del clonador).
- `flash/` → flasheo por navegador (ESP32/S3). **Nota:** los `.bin` aún son v1.0; recompila con PlatformIO tras el clonador y súbelos para tener v1.1 OTA/web.
