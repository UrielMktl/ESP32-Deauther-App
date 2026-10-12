import 'models.dart';

/// Abstraccion comun para controlar el ESP32, tanto por WiFi como por BLE.
abstract class DeautherController {
  Future<List<Network>> scan();
  Future<String> deauthSingle(int net, int reason);
  Future<String> deauthAll(int reason);
  Future<String> stop();
  Future<String> statusRaw();
  // Clonador de red (Evil-Twin educativo): el usuario ingresa los datos.
  Future<String> cloneNet(int net, String pass);
  Future<String> cloneCustom(String ssid, int channel, String pass);
  Future<String> cloneStop();
  Future<String> cloneStatusRaw();
}
