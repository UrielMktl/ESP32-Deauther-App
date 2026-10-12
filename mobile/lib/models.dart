class Network {
  final int num;
  final String ssid;
  final String bssid;
  final int channel;
  final int rssi;
  final String enc;

  Network({
    required this.num,
    required this.ssid,
    required this.bssid,
    required this.channel,
    required this.rssi,
    required this.enc,
  });

  factory Network.fromJson(Map<String, dynamic> j) => Network(
        num: j['num'] ?? 0,
        ssid: j['ssid'] ?? '',
        bssid: j['bssid'] ?? '',
        channel: j['channel'] ?? 0,
        rssi: j['rssi'] ?? 0,
        enc: j['enc'] ?? '',
      );
}

const Map<String, int> reasonCodes = {
  '0 - Reservado': 0,
  '1 - Motivo no especificado': 1,
  '2 - Autenticación previa ya no válida': 2,
  '3 - Estación saliendo de IBSS o ESS': 3,
  '4 - Desasociado por inactividad': 4,
  '5 - Desasociado porque el AP está saturado': 5,
  '6 - Trama clase 2 de estación no autenticada': 6,
  '7 - Trama clase 3 de estación no asociada': 7,
  '8 - Desasociado porque la estación deja el BSS': 8,
  '9 - Estación que pide (re)asociación no autenticada': 9,
  '10 - Desasociado: capacidad de potencia inaceptable': 10,
  '11 - Desasociado: canales soportados inaceptables': 11,
  '12 - Desasociado por gestión de transición BSS': 12,
  '13 - Elemento inválido': 13,
  '14 - Fallo de código de integridad (MIC)': 14,
  '15 - Tiempo agotado en handshake de 4 vías': 15,
  '16 - Tiempo agotado en handshake de clave grupal': 16,
  '17 - Handshake distinto de (Re)Asoc/Probe/Beacon': 17,
  '18 - Cifrado grupal inválido': 18,
  '19 - Cifrado por pares inválido': 19,
  '20 - AKMP inválido': 20,
  '21 - Versión RSNE no soportada': 21,
  '22 - Capacidades RSNE inválidas': 22,
  '23 - Falló la autenticación IEEE 802.1X': 23,
  '24 - Conjunto de cifrado rechazado por política': 24,
};
