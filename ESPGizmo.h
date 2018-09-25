#pragma once

#include <ESP8266WebServer.h>

#define MAX_NAME_SIZE       16
#define MAX_VERSION_SIZE    16
#define MAX_SSID_SIZE       32
#define MAX_PASSKEY_SIZE    32

#define MAX_MQTT_HOST_SIZE  32
#define MAX_MQTT_USER_SIZE  32
#define MAX_MQTT_PASS_SIZE  32

class ESPGizmo {
public:
    ESPGizmo();

    void beginSetup(char *name, char *version, char *passkey);
    void endSetup();

    const char *getName();
    const char *getHostname();
    const char *getSSID();

    ESP8266WebServer *httpServer();

    bool isNetworkAvailable(void (*afterConnection)());

    int updateSoftware(char *url, char *version);

private:
    char name[MAX_NAME_SIZE];
    char version[MAX_VERSION_SIZE];

    char hostname[MAX_SSID_SIZE];
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
    void setupOTA();
    void setupHTTPServer();

    void loadNetworkConfig();
    void saveNetworkConfig();

    void handleNetworkScan();
    void handleNetworkConfig();

    void restart();
};