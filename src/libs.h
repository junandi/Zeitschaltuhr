#ifndef LIBS_H
#define LIBS_H

#define DEBUG
// Telegram and MQTT interface can't be used both
//#define MQTT
//#define WEBAPP
#define TELEGRAM
#define NTP
#define WIFIMANAGER
//#define OTA

#include <ESP8266WiFi.h>

#include <ESP8266mDNS.h>
#include <TimeLib.h>
#include <ArduinoJson.h>
#include <Hash.h>

#ifdef NTP
#include <WiFiUdp.h>
#include <NTPClient.h>
#endif

#ifdef WIFIMANAGER
#include <WiFiManager.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#endif

#ifdef OTA
#include <ArduinoOTA.h>
#endif

#ifdef TELEGRAM
#include <UniversalTelegramBot.h>
#endif

#ifdef MQTT
#include <PubSubClient.h>
#endif

#ifdef WEBAPP
#include <WebSocketsServer.h>

#include <FS.h>
#endif

#endif
