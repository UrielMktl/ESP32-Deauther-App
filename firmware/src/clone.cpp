#include <WiFi.h>
#include "clone.h"
#include "deauth.h"
#include "definitions.h"

static bool _active = false;
static String _ssid = "";
static String _pass = "";
static int _channel = 1;

static String sanitize_ssid(String s) {
  s.trim();
  if (s.length() > 32) s = s.substring(0, 32);
  return s;
}

static int sanitize_channel(int ch) {
  if (ch < 1) return 1;
  if (ch > CHANNEL_MAX) return CHANNEL_MAX;
  return ch;
}

static bool bring_up(const String& ssid, int ch, const String& pass) {
  // El clonador y el deauth usan el mismo radio: se excluyen.
  stop_deauth();
  stop_clone();

  String s = sanitize_ssid(ssid);
  if (s.length() == 0) return false;
  ch = sanitize_channel(ch);

  WiFi.mode(WIFI_MODE_AP);
  bool ok;
  if (pass.length() == 0) {
    ok = WiFi.softAP(s.c_str(), NULL, ch, 0, 4);
  } else {
    // WPA2 exige minimo 8 caracteres; si es mas corta se crea abierta.
    if (pass.length() < 8) {
      ok = WiFi.softAP(s.c_str(), NULL, ch, 0, 4);
    } else {
      ok = WiFi.softAP(s.c_str(), pass.c_str(), ch, 0, 4);
    }
  }
  if (ok) {
    _active = true;
    _ssid = s;
    _pass = pass;
    _channel = ch;
    DEBUG_PRINTF("Clone AP activo: %s ch=%d %s\n", s.c_str(), ch,
                 pass.length() >= 8 ? "(WPA2)" : "(abierta)");
    BLINK_LED(3, 100);
  }
  return ok;
}

bool clone_active() { return _active; }
String clone_ssid() { return _ssid; }
String clone_pass() { return _pass; }
int clone_channel() { return _channel; }
int clone_clients() { return WiFi.softAPgetStationNum(); }

bool start_clone_net(int net_num, const String& pass) {
  extern int num_networks;
  if (net_num < 0 || net_num >= num_networks) return false;
  // WiFi.SSID()/channel() requieren que el scan siga en memoria: es el caso
  // normal tras /api/scan. Si el SSID sale vacio (red oculta) se rechaza.
  String target = WiFi.SSID(net_num);
  int ch = WiFi.channel(net_num);
  target.trim();
  if (target.length() == 0) return false;
  return bring_up(target, ch, pass);
}

bool start_clone_custom(const String& ssid, int channel, const String& pass) {
  return bring_up(ssid, channel, pass);
}

void stop_clone() {
  if (!_active) return;
  _active = false;
  _ssid = "";
  _pass = "";
  // Restaurar el AP de gestion original.
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_MODE_AP);
  WiFi.softAP(AP_SSID, AP_PASS);
  DEBUG_PRINTLN("Clone detenido, AP original restaurado.");
}

String clone_status_json() {
  String j = "{";
  j += "\"active\":" + String(_active ? "true" : "false") + ",";
  j += "\"ssid\":\"" + _ssid + "\",";
  j += "\"channel\":" + String(_channel) + ",";
  j += "\"clients\":" + String(clone_clients()) + ",";
  j += "\"open\":" + String(_pass.length() >= 8 ? "false" : "true");
  j += "}";
  return j;
}
