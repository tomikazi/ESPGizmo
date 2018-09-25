#pragma once

#include <ESP8266WebServer.h>

#define MAX_NAME_SIZE       16
#define MAX_SSID_SIZE       32
#define MAX_PASSKEY_SIZE    32

#define MAX_MQTT_HOST_SIZE  32
#define MAX_MQTT_USER_SIZE  32
#define MAX_MQTT_PASS_SIZE  32

class ESPGizmo {
public:
    ESPGizmo();

    void beginSetup(char *name, char *passkey);
    void endSetup();

    const char *getName();
    const char *getLocalSSID();
    const char *getSSID();

    ESP8266WebServer *httpServer();

    bool isNetworkAvailable(void (*afterConnection)());

private:
    char name[MAX_NAME_SIZE];

    char ssidLocal[MAX_SSID_SIZE];
    char passkeyLocal[MAX_PASSKEY_SIZE];

    char ssid[MAX_SSID_SIZE];
    char passkey[MAX_PASSKEY_SIZE];

    char mqttHost[MAX_MQTT_HOST_SIZE];
    char mqttUser[MAX_MQTT_USER_SIZE];
    char mqttPass[MAX_MQTT_PASS_SIZE];

    IPAddress *apIP;
    IPAddress *netMask;
    ESP8266WebServer *server;

    void setupWiFi();
    void setupHTTPServer();

    void loadNetworkConfig();
    void saveNetworkConfig();

    void handleNetworkScan();
    void handleNetworkConfig();

    void restart();
};