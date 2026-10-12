#include "definitions.h"
#include "ble_com.h"
#include "deauth.h"
#include "clone.h"
#include "web_interface.h"
#ifdef ENABLE_BLE
#include <WiFi.h>
#include <NimBLEDevice.h>

static NimBLEServer* pServer = nullptr;
static NimBLECharacteristic* pTx = nullptr;
static bool deviceConnected = false;
static bool ble_authed = false;

static String pendingCmd = "";
static bool pending = false;

#define SERVICE_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define RX_UUID      "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define TX_UUID      "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

class MyServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* pServer) {
    deviceConnected = true;
    ble_authed = false;
    // En ESP32-S3 WiFi y BLE comparten el radio; apagar el AP libera ancho
    // de banda y evita que las notificaciones BLE se pierdan (timeout).
    WiFi.mode(WIFI_OFF);
    ble_send("CONNECTED. Envia: LOGIN <tu password>");
  }
  void onDisconnect(NimBLEServer* pServer) {
    deviceConnected = false;
    ble_authed = false;
    // Restaurar WiFi al desconectar BLE. Si habia un clon activo, se
    // re-levanta el AP clonado; si no, el AP de gestion original.
    WiFi.mode(WIFI_MODE_AP);
    if (clone_active() && clone_ssid().length()) {
      String cp = clone_pass();
      if (cp.length() >= 8) WiFi.softAP(clone_ssid().c_str(), cp.c_str(), clone_channel(), 0, 4);
      else WiFi.softAP(clone_ssid().c_str(), NULL, clone_channel(), 0, 4);
    } else {
      WiFi.softAP(AP_SSID, AP_PASS);
    }
    NimBLEDevice::startAdvertising();
  }
};

class RxCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* pCharacteristic) {
    std::string v = pCharacteristic->getValue();
    if (v.length()) {
      pendingCmd = String(v.c_str());
      pending = true;
    }
  }
};

void ble_send(const String& msg) {
  if (pTx && deviceConnected) {
    pTx->setValue(msg.c_str());
    pTx->notify();
  }
}

void start_ble() {
  NimBLEDevice::init(BLE_DEVICE_NAME);
  pServer = NimBLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  NimBLEService* pService = pServer->createService(SERVICE_UUID);
  pTx = pService->createCharacteristic(TX_UUID, NIMBLE_PROPERTY::NOTIFY);
  NimBLECharacteristic* pRx = pService->createCharacteristic(RX_UUID, NIMBLE_PROPERTY::WRITE);
  pRx->setCallbacks(new RxCallbacks());

  NimBLEAdvertising* pAdv = NimBLEDevice::getAdvertising();
  NimBLEAdvertisementData advData;
  advData.setFlags(0x06);  // LE General Discoverable, sin BR/EDR
  advData.setName(BLE_DEVICE_NAME);
  advData.addServiceUUID(NimBLEUUID(SERVICE_UUID));
  pAdv->setAdvertisementData(advData);
  NimBLEAdvertisementData scanData;
  scanData.setName(BLE_DEVICE_NAME);
  pAdv->setScanResponseData(scanData);
  pAdv->start();
  DEBUG_PRINTLN("BLE iniciado. Conecta por BLE y envia LOGIN.");
}

// Procesa los comandos recibidos por BLE fuera del callback (en el loop)
void ble_process() {
  if (!pending) return;
  pending = false;
  String cmd = pendingCmd;
  cmd.trim();

  if (cmd.startsWith("LOGIN ")) {
    String p = cmd.substring(6);
    p.trim();
    if (p == LOGIN_PASS) {
      ble_authed = true;
      ble_send("OK autenticado");
    } else {
      ble_send("ERR password incorrecto");
    }
    return;
  }

  if (!ble_authed) {
    ble_send("ERR no autenticado (envia LOGIN <pass>)");
    return;
  }

  if (cmd == "SCAN") {
    do_scan();
    ble_send("SCAN " + last_scan_json);
    return;
  }

  if (cmd.startsWith("DEAUTH ")) {
    int sp = cmd.indexOf(' ', 7);
    if (sp < 0) { ble_send("ERR uso: DEAUTH <net> <reason>"); return; }
    int net = cmd.substring(7, sp).toInt();
    int reason = cmd.substring(sp + 1).toInt();
    if (net < num_networks) {
      start_deauth(net, DEAUTH_TYPE_SINGLE, (uint16_t)reason);
      ble_send("OK deauth iniciado en red " + String(net));
    } else {
      ble_send("ERR numero de red invalido");
    }
    return;
  }

  if (cmd.startsWith("DEAUTHALL ")) {
    int reason = cmd.substring(10).toInt();
    start_deauth(0, DEAUTH_TYPE_ALL, (uint16_t)reason);
    ble_send("OK deauth global iniciado");
    return;
  }

  if (cmd == "STOP") {
    stop_deauth();
    ble_send("OK detenido");
    return;
  }

  if (cmd == "STATUS") {
    String s = "STATUS eliminadas=" + String(eliminated_stations) + " tipo=" + String(deauth_type);
    s += " clone=" + clone_status_json();
    ble_send(s);
    return;
  }

  // --- Clonador de red ---
  // CLONE_NET <num> [pass]  -> clona red escaneada (mismo SSID/canal)
  // CLONE <ssid>|<canal>|<pass> -> clon manual (el usuario ingresa los datos)
  // CLONE_STOP / CLONE_STATUS
  if (cmd.startsWith("CLONE_NET ")) {
    String rest = cmd.substring(10);
    rest.trim();
    int sp = rest.indexOf(' ');
    int net = (sp < 0 ? rest : rest.substring(0, sp)).toInt();
    String pass = (sp < 0 ? "" : rest.substring(sp + 1));
    pass.trim();
    if (start_clone_net(net, pass)) {
      ble_send("OK clon iniciado " + clone_status_json());
    } else {
      ble_send("ERR clon fallo (red invalida u oculta)");
    }
    return;
  }

  if (cmd.startsWith("CLONE ")) {
    // Formato con pipes para permitir espacios en el SSID: CLONE Mi Red|6|pass12345
    String rest = cmd.substring(6);
    int p1 = rest.indexOf('|');
    if (p1 < 0) { ble_send("ERR uso: CLONE <ssid>|<canal>|<pass>  ej: CLONE MiCasa|6|"); return; }
    String ssid = rest.substring(0, p1);
    String tail = rest.substring(p1 + 1);
    int p2 = tail.indexOf('|');
    String chStr = (p2 < 0 ? tail : tail.substring(0, p2));
    String pass = (p2 < 0 ? "" : tail.substring(p2 + 1));
    ssid.trim(); pass.trim();
    int ch = chStr.toInt();
    if (ssid.length() == 0) { ble_send("ERR ssid vacio"); return; }
    if (start_clone_custom(ssid, ch <= 0 ? 1 : ch, pass)) {
      ble_send("OK clon iniciado " + clone_status_json());
    } else {
      ble_send("ERR clon fallo");
    }
    return;
  }

  if (cmd == "CLONE_STOP") {
    stop_clone();
    ble_send("OK clon detenido");
    return;
  }

  if (cmd == "CLONE_STATUS") {
    ble_send("CLONE " + clone_status_json());
    return;
  }

  ble_send("ERR comando desconocido");
}
#endif
