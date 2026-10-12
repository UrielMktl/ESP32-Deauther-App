#include <WebServer.h>
#include <esp_random.h>
#include "web_interface.h"
#include "definitions.h"
#include "deauth.h"
#include "clone.h"

WebServer server(80);
int num_networks;
String last_scan_json = "[]";
String g_token = "";

// Move the function declaration to the top
String getEncryptionType(wifi_auth_mode_t encryptionType);

// Perform a WiFi scan and store the result as JSON for the API
void do_scan() {
  num_networks = WiFi.scanNetworks();
  last_scan_json = "[";
  for (int i = 0; i < num_networks; i++) {
    if (i > 0) last_scan_json += ",";
    last_scan_json += "{";
    last_scan_json += "\"num\":" + String(i) + ",";
    last_scan_json += "\"ssid\":\"" + WiFi.SSID(i) + "\",";
    last_scan_json += "\"bssid\":\"" + WiFi.BSSIDstr(i) + "\",";
    last_scan_json += "\"channel\":" + String(WiFi.channel(i)) + ",";
    last_scan_json += "\"rssi\":" + String(WiFi.RSSI(i)) + ",";
    last_scan_json += "\"enc\":\"" + getEncryptionType(WiFi.encryptionType(i)) + "\"";
    last_scan_json += "}";
  }
  last_scan_json += "]";
}

// ---------------------------------------------------------------------------
// Login / auth helpers
// ---------------------------------------------------------------------------
bool web_authed() {
  if (server.hasHeader("Cookie")) {
    if (server.header("Cookie").indexOf("auth=" + g_token) >= 0) return true;
  }
  return false;
}

bool api_authed() {
  if (server.arg("token") == g_token) return true;
  if (server.hasHeader("X-Token") && server.header("X-Token") == g_token) return true;
  return false;
}

void redirect_root() {
  server.sendHeader("Location", "/");
  server.send(301);
}

void handle_login() {
  if (server.method() == HTTP_POST) {
    String u = server.arg("user");
    String p = server.arg("pass");
    if (u == LOGIN_USER && p == LOGIN_PASS) {
      server.sendHeader("Set-Cookie", "auth=" + g_token + "; Path=/; Max-Age=3600");
      server.sendHeader("Location", "/");
      server.send(301);
      return;
    }
    String html = "<!DOCTYPE html><html><head><meta charset=\"UTF-8\"><title>Acceso</title></head>";
    html += "<body style=\"font-family:Arial;text-align:center;padding-top:40px\">";
    html += "<h2>ESP32-Deauther</h2><p style=\"color:red\">Credenciales incorrectas</p>";
    html += "<form method=\"post\" action=\"/login\">";
    html += "<input name=\"user\" placeholder=\"Usuario\" value=\"" + u + "\"><br>";
    html += "<input name=\"pass\" type=\"password\" placeholder=\"Contraseña\"><br><br>";
    html += "<input type=\"submit\" value=\"Acceder\"></form></body></html>";
    server.send(200, "text/html", html);
    return;
  }
  String html = "<!DOCTYPE html><html><head><meta charset=\"UTF-8\"><title>Acceso</title></head>";
  html += "<body style=\"font-family:Arial;text-align:center;padding-top:40px\">";
  html += "<h2>ESP32-Deauther</h2><form method=\"post\" action=\"/login\">";
  html += "<input name=\"user\" placeholder=\"Usuario\"><br>";
  html += "<input name=\"pass\" type=\"password\" placeholder=\"Contraseña\"><br><br>";
  html += "<input type=\"submit\" value=\"Acceder\"></form></body></html>";
  server.send(200, "text/html", html);
}

void handle_root() {
  if (!web_authed()) { server.sendHeader("Location", "/login"); server.send(301); return; }

  String html = R"(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ESP32-Deauther</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            line-height: 1.6;
            color: #333;
            max-width: 800px;
            margin: 0 auto;
            padding: 20px;
            background-color: #f4f4f4;
        }
        h1, h2 {
            color: #2c3e50;
        }
        table {
            width: 100%;
            border-collapse: collapse;
            margin-bottom: 20px;
        }
        th, td {
            padding: 12px;
            text-align: left;
            border-bottom: 1px solid #ddd;
        }
        th {
            background-color: #3498db;
            color: white;
        }
        tr:nth-child(even) {
            background-color: #f2f2f2;
        }
        form {
            background-color: white;
            padding: 20px;
            border-radius: 5px;
            box-shadow: 0 2px 5px rgba(0,0,0,0.1);
            margin-bottom: 20px;
        }
        input[type="text"], input[type="submit"] {
            width: 100%;
            padding: 10px;
            margin-bottom: 10px;
            border: 1px solid #ddd;
            border-radius: 4px;
        }
        input[type="submit"] {
            background-color: #3498db;
            color: white;
            border: none;
            cursor: pointer;
            transition: background-color 0.3s;
        }
        input[type="submit"]:hover {
            background-color: #2980b9;
        }
    </style>
</head>
<body>
    <h1>ESP32-Deauther</h1>
    
    <h2>Redes WiFi</h2>
    <table>
        <tr>
            <th>Número</th>
            <th>SSID</th>
            <th>BSSID</th>
            <th>Canal</th>
            <th>RSSI</th>
            <th>Cifrado</th>
        </tr>
)";

  for (int i = 0; i < num_networks; i++) {
    String encryption = getEncryptionType(WiFi.encryptionType(i));
    html += "<tr><td>" + String(i) + "</td><td>" + WiFi.SSID(i) + "</td><td>" + WiFi.BSSIDstr(i) + "</td><td>" + 
            String(WiFi.channel(i)) + "</td><td>" + String(WiFi.RSSI(i)) + "</td><td>" + encryption + "</td></tr>";
  }

  html += R"(
    </table>

    <form method="post" action="/rescan">
        <input type="submit" value="Buscar redes">
    </form>

    <form method="post" action="/deauth">
        <h2>Lanzar ataque de desautenticación</h2>
        <input type="text" name="net_num" placeholder="Número de red">
        <input type="text" name="reason" placeholder="Código de motivo">
        <input type="submit" value="Lanzar ataque">
    </form>

    <p>Estaciones eliminadas: )" + String(eliminated_stations) + R"(</p>

    <form method="post" action="/deauth_all">
        <h2>Desautenticar TODAS las redes</h2>
        <input type="text" name="reason" placeholder="Código de motivo">
        <input type="submit" value="Desautenticar todas">
    </form>

    <form method="post" action="/stop">
        <input type="submit" value="Detener ataque">
    </form>

    <form method="post" action="/clone">
        <h2>Clonador de red (Evil-Twin educativo)</h2>
        <p>Estado clon: )" + String(clone_active() ? ("ACTIVO \"" + clone_ssid() + "\" ch " + String(clone_channel()) + " clientes " + String(clone_clients())) : "inactivo") + R"(</p>
        <p style="font-size:13px">Clona una red escaneada (mismo SSID y canal) o escribe los datos a mano. Solo en redes propias.</p>
        <input type="text" name="net_num" placeholder="Nro de red a clonar (de la tabla)">
        <input type="text" name="pass" placeholder="Password AP falso (vacio=abierta, min 8 para WPA2)">
        <input type="submit" value="Clonar red seleccionada">
    </form>

    <form method="post" action="/clone_custom">
        <h2>Clon manual: ingresa los datos</h2>
        <input type="text" name="ssid" placeholder="SSID a clonar (ej. MiCasa_WiFi)" maxlength="32">
        <input type="text" name="channel" placeholder="Canal 1-13">
        <input type="text" name="pass" placeholder="Password AP falso (vacio=abierta)">
        <input type="submit" value="Clonar con estos datos">
    </form>

    <form method="post" action="/clone_stop">
        <input type="submit" value="Detener clonador">
    </form>

    <h2>Códigos de motivo</h2>
    <table>
        <tr>
            <th>Código</th>
            <th>Significado</th>
        </tr>
        <tr><td>0</td><td>Reservado.</td></tr>
        <tr><td>1</td><td>Motivo no especificado.</td></tr>
        <tr><td>2</td><td>Autenticación previa ya no válida.</td></tr>
        <tr><td>3</td><td>Desautenticado porque la estación deja el IBSS o ESS.</td></tr>
        <tr><td>4</td><td>Desasociado por inactividad.</td></tr>
        <tr><td>5</td><td>Desasociado porque el AP no puede con todas las estaciones.</td></tr>
        <tr><td>6</td><td>Trama clase 2 de estación no autenticada.</td></tr>
        <tr><td>7</td><td>Trama clase 3 de estación no asociada.</td></tr>
        <tr><td>8</td><td>Desasociado porque la estación deja el BSS.</td></tr>
        <tr><td>9</td><td>Estación que pide (re)asociación no autenticada.</td></tr>
        <tr><td>10</td><td>Desasociado: capacidad de potencia inaceptable.</td></tr>
        <tr><td>11</td><td>Desasociado: canales soportados inaceptables.</td></tr>
        <tr><td>12</td><td>Desasociado por gestión de transición BSS.</td></tr>
        <tr><td>13</td><td>Elemento inválido.</td></tr>
        <tr><td>14</td><td>Fallo de código de integridad (MIC).</td></tr>
        <tr><td>15</td><td>Tiempo agotado en handshake de 4 vías.</td></tr>
        <tr><td>16</td><td>Tiempo agotado en handshake de clave grupal.</td></tr>
        <tr><td>17</td><td>Handshake distinto de (Re)Asociación/Probe/Beacon.</td></tr>
        <tr><td>18</td><td>Cifrado grupal inválido.</td></tr>
        <tr><td>19</td><td>Cifrado por pares inválido.</td></tr>
        <tr><td>20</td><td>AKMP inválido.</td></tr>
        <tr><td>21</td><td>Versión RSNE no soportada.</td></tr>
        <tr><td>22</td><td>Capacidades RSNE inválidas.</td></tr>
        <tr><td>23</td><td>Falló la autenticación IEEE 802.1X.</td></tr>
        <tr><td>24</td><td>Conjunto de cifrado rechazado por política.</td></tr>
    </table>
</body>
</html>
)";

  server.send(200, "text/html", html);
}


void handle_deauth() {
  if (!web_authed()) { server.sendHeader("Location", "/login"); server.send(301); return; }

  int wifi_number = server.arg("net_num").toInt();
  uint16_t reason = server.arg("reason").toInt();

  String html = R"(
<!DOCTYPE html>
<html lang="es">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Ataque de desautenticación</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            display: flex;
            justify-content: center;
            align-items: center;
            height: 100vh;
            margin: 0;
            background-color: #f0f0f0;
        }
        .alert {
            background-color: #4CAF50;
            color: white;
            padding: 20px;
            border-radius: 5px;
            box-shadow: 0 2px 5px rgba(0,0,0,0.2);
            text-align: center;
        }
        .alert.error {
            background-color: #f44336;
        }
        .button {
            display: inline-block;
            padding: 10px 20px;
            margin-top: 20px;
            background-color: #008CBA;
            color: white;
            text-decoration: none;
            border-radius: 5px;
            transition: background-color 0.3s;
        }
        .button:hover {
            background-color: #005f73;
        }
    </style>
</head>
<body>
    <div class="alert)";

  if (wifi_number < num_networks) {
    html += R"(">
        <h2>¡Ataque iniciado!</h2>
        <p>Desautenticando la red número: )" + String(wifi_number) + R"(</p>
        <p>Código de motivo: )" + String(reason) + R"(</p>
    </div>)";
    start_deauth(wifi_number, DEAUTH_TYPE_SINGLE, reason);
  } else {
    html += R"( error">
        <h2>Error: número de red inválido</h2>
        <p>Elige un número de red válido.</p>
    </div>)";
  }

  html += R"(
    <a href="/" class="button">Volver al inicio</a>
</body>
</html>
  )";

  server.send(200, "text/html", html);
}

void handle_deauth_all() {
  if (!web_authed()) { server.sendHeader("Location", "/login"); server.send(301); return; }

  uint16_t reason = server.arg("reason").toInt();

  String html = R"(
<!DOCTYPE html>
<html lang="es">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Desautenticar todas</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            display: flex;
            justify-content: center;
            align-items: center;
            height: 100vh;
            margin: 0;
            background-color: #f0f0f0;
        }
        .alert {
            background-color: #ff9800;
            color: white;
            padding: 20px;
            border-radius: 5px;
            box-shadow: 0 2px 5px rgba(0,0,0,0.2);
            text-align: center;
        }
        .button {
            display: inline-block;
            padding: 10px 20px;
            margin-top: 20px;
            background-color: #008CBA;
            color: white;
            text-decoration: none;
            border-radius: 5px;
            transition: background-color 0.3s;
        }
        .button:hover {
            background-color: #005f73;
        }
    </style>
</head>
<body>
    <div class="alert">
        <h2>¡Ataque a TODAS las redes!</h2>
        <p>El WiFi se apagará ahora. Para detener el ataque, reinicia el ESP32.</p>
        <p>Código de motivo: )" + String(reason) + R"(</p>
    </div>
</body>
</html>
  )";

  server.send(200, "text/html", html);
  server.stop();
  start_deauth(0, DEAUTH_TYPE_ALL, reason);
}

void handle_rescan() {
  if (!web_authed()) { server.sendHeader("Location", "/login"); server.send(301); return; }
  do_scan();
  redirect_root();
}

void handle_stop() {
  if (!web_authed()) { server.sendHeader("Location", "/login"); server.send(301); return; }
  stop_deauth();
  redirect_root();
}

void handle_clone() {
  if (!web_authed()) { server.sendHeader("Location", "/login"); server.send(301); return; }
  int net = server.arg("net_num").toInt();
  String pass = server.arg("pass");
  pass.trim();
  if (net < 0 || net >= num_networks) { redirect_root(); return; }
  start_clone_net(net, pass);
  redirect_root();
}

void handle_clone_custom() {
  if (!web_authed()) { server.sendHeader("Location", "/login"); server.send(301); return; }
  String ssid = server.arg("ssid");
  int ch = server.arg("channel").toInt();
  String pass = server.arg("pass");
  ssid.trim(); pass.trim();
  if (ssid.length() == 0) { redirect_root(); return; }
  if (ch < 1 || ch > CHANNEL_MAX) ch = 1;
  start_clone_custom(ssid, ch, pass);
  redirect_root();
}

void handle_clone_stop() {
  if (!web_authed()) { server.sendHeader("Location", "/login"); server.send(301); return; }
  stop_clone();
  redirect_root();
}

// ---------------------------------------------------------------------------
// JSON API for the companion app (additive: no original function removed)
// ---------------------------------------------------------------------------

void handle_api_login() {
  String u = server.arg("user");
  String p = server.arg("pass");
  if (u == LOGIN_USER && p == LOGIN_PASS) {
    server.send(200, "application/json", "{\"status\":\"ok\",\"token\":\"" + g_token + "\"}");
  } else {
    server.send(401, "application/json", "{\"status\":\"error\",\"message\":\"bad credentials\"}");
  }
}

void handle_api_scan() {
  if (!api_authed()) { server.send(401, "application/json", "{\"error\":\"unauthorized\"}"); return; }
  do_scan();
  server.send(200, "application/json", last_scan_json);
}

void handle_api_networks() {
  if (!api_authed()) { server.send(401, "application/json", "{\"error\":\"unauthorized\"}"); return; }
  server.send(200, "application/json", last_scan_json);
}

void handle_api_deauth() {
  if (!api_authed()) { server.send(401, "application/json", "{\"error\":\"unauthorized\"}"); return; }
  int wifi_number = server.arg("net_num").toInt();
  uint16_t reason = server.arg("reason").toInt();
  if (wifi_number < num_networks) {
    start_deauth(wifi_number, DEAUTH_TYPE_SINGLE, reason);
    server.send(200, "application/json", "{\"status\":\"ok\",\"net_num\":" + String(wifi_number) + ",\"reason\":" + String(reason) + "}");
  } else {
    server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"invalid network number\"}");
  }
}

void handle_api_deauth_all() {
  if (!api_authed()) { server.send(401, "application/json", "{\"error\":\"unauthorized\"}"); return; }
  uint16_t reason = server.arg("reason").toInt();
  server.send(200, "application/json", "{\"status\":\"ok\",\"attack\":\"all\",\"reason\":" + String(reason) + "}");
  server.stop();
  start_deauth(0, DEAUTH_TYPE_ALL, reason);
}

void handle_api_stop() {
  if (!api_authed()) { server.send(401, "application/json", "{\"error\":\"unauthorized\"}"); return; }
  stop_deauth();
  server.send(200, "application/json", "{\"status\":\"ok\"}");
}

void handle_api_status() {
  if (!api_authed()) { server.send(401, "application/json", "{\"error\":\"unauthorized\"}"); return; }
  String json = "{";
  json += "\"eliminated_stations\":" + String(eliminated_stations) + ",";
  json += "\"deauth_type\":" + String(deauth_type) + ",";
  json += "\"ap_ssid\":\"" + String(AP_SSID) + "\",";
  json += "\"ap_ip\":\"192.168.4.1\",";
  json += "\"clone\":" + clone_status_json();
  json += "}";
  server.send(200, "application/json", json);
}

void handle_api_clone() {
  if (!api_authed()) { server.send(401, "application/json", "{\"error\":\"unauthorized\"}"); return; }
  bool ok = false;
  if (server.hasArg("ssid")) {
    // Clon manual: el usuario ingresa SSID + canal + pass
    String ssid = server.arg("ssid");
    int ch = server.arg("channel").toInt();
    String pass = server.arg("pass");
    ssid.trim(); pass.trim();
    if (ch < 1 || ch > CHANNEL_MAX) ch = 1;
    ok = start_clone_custom(ssid, ch, pass);
  } else {
    // Clon de red escaneada
    int net = server.arg("net_num").toInt();
    String pass = server.arg("pass");
    pass.trim();
    ok = start_clone_net(net, pass);
  }
  if (ok) {
    server.send(200, "application/json", "{\"status\":\"ok\",\"clone\":" + clone_status_json() + "}");
  } else {
    server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"datos de clon invalidos (ssid/canal/pass o red oculta)\"}");
  }
}

void handle_api_clone_stop() {
  if (!api_authed()) { server.send(401, "application/json", "{\"error\":\"unauthorized\"}"); return; }
  stop_clone();
  server.send(200, "application/json", "{\"status\":\"ok\",\"clone\":" + clone_status_json() + "}");
}

void handle_api_clone_status() {
  if (!api_authed()) { server.send(401, "application/json", "{\"error\":\"unauthorized\"}"); return; }
  server.send(200, "application/json", clone_status_json());
}

void start_web_interface() {
  g_token = String((uint32_t)esp_random(), HEX) + String((uint32_t)esp_random(), HEX);

  server.on("/", handle_root);
  server.on("/login", handle_login);
  server.on("/deauth", handle_deauth);
  server.on("/deauth_all", handle_deauth_all);
  server.on("/rescan", handle_rescan);
  server.on("/stop", handle_stop);
  server.on("/clone", HTTP_POST, handle_clone);
  server.on("/clone_custom", HTTP_POST, handle_clone_custom);
  server.on("/clone_stop", HTTP_POST, handle_clone_stop);

  // JSON API endpoints for the app
  server.on("/api/login", HTTP_POST, handle_api_login);
  server.on("/api/scan", handle_api_scan);
  server.on("/api/networks", handle_api_networks);
  server.on("/api/deauth", HTTP_POST, handle_api_deauth);
  server.on("/api/deauth_all", HTTP_POST, handle_api_deauth_all);
  server.on("/api/stop", HTTP_POST, handle_api_stop);
  server.on("/api/status", handle_api_status);
  server.on("/api/clone", HTTP_POST, handle_api_clone);
  server.on("/api/clone_stop", HTTP_POST, handle_api_clone_stop);
  server.on("/api/clone_status", handle_api_clone_status);

  server.begin();
}

void web_interface_handle_client() {
  server.handleClient();
}

// The function implementation can stay where it is
String getEncryptionType(wifi_auth_mode_t encryptionType) {
  switch (encryptionType) {
    case WIFI_AUTH_OPEN:
      return "Open";
    case WIFI_AUTH_WEP:
      return "WEP";
    case WIFI_AUTH_WPA_PSK:
      return "WPA_PSK";
    case WIFI_AUTH_WPA2_PSK:
      return "WPA2_PSK";
    case WIFI_AUTH_WPA_WPA2_PSK:
      return "WPA_WPA2_PSK";
    case WIFI_AUTH_WPA2_ENTERPRISE:
      return "WPA2_ENTERPRISE";
    default:
      return "UNKNOWN";
  }
}
