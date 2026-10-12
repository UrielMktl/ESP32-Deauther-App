#ifndef CLONE_H
#define CLONE_H

#include <Arduino.h>

// Clonador de red (Evil-Twin educativo):
// crea un AP con el mismo SSID (y mismo canal) que la red objetivo,
// o con un SSID/canal/password escritos a mano.
// Solo para pruebas en redes propias.

bool clone_active();
String clone_ssid();
String clone_pass();
int clone_channel();
int clone_clients();

// Clona la red escaneada num <net_num> (usa SSID + canal detectados).
// pass: password del AP falso ("" = abierta). Min 8 chars si se usa WPA2.
bool start_clone_net(int net_num, const String& pass);

// Clona con datos manuales: ssid libre, canal 1-13, pass "" o >=8 chars.
bool start_clone_custom(const String& ssid, int channel, const String& pass);

void stop_clone();
String clone_status_json();

#endif
