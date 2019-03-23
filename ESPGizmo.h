#pragma once

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ESP8266WebServer.h>

#define MAX_NAME_SIZE       32
#define MAX_VERSION_SIZE    16
#define MAX_SSID_SIZE       32
#define MAX_PASSKEY_SIZE    32

#define MAX_MQTT_HOST_SIZE  32
#define MAX_MQTT_USER_SIZE  32
#define MAX_MQTT_PASS_SIZE  32

class ESPGizmo {
public:
    ESPGizmo();

    void beginSetup(const char *name, const char *version, const char *passkey);
    void endSetup();

    const char *getName();
    const char *getHostname();
    const char *getTopicPrefix();
    const char *getSSID();

    void led(boolean on);

    void setCallback(void (*callback)(char*, uint8_t*, unsigned int));
    void addTopic(const char *topic);
    void addTopic(const char *topic, const char *uniqueName);
    void publish(char *topic, char *payload);
    void publish(char *topic, char *payload, boolean retain);
    void schedulePublish(char *topic, char *payload);
    void schedulePublish(char *topic, char *payload, boolean retain);

    void publish(char *topic, const char *payload);
    void publish(char *topic, const char *payload, boolean retain);
    void schedulePublish(char *topic, const char *payload);
    void schedulePublish(char *topic, const char *payload, boolean retain);

    bool publishBinarySensor(bool nv, bool ov, char *topic);

    ESP8266WebServer *httpServer();
    void setUpdateURL(const char *url);

    bool isNetworkAvailable(void (*afterConnection)());

    void scheduleRestart();
    void scheduleUpdate();

    int updateSoftware(const char *url);

    void handleMQTTMessage(const char *topic, const char *value);

    // Not implemented yet
    void setMQTTLastWill(const char* willTopic, const char* willMessage,
                         uint8_t willQos, bool willRetain);

private:
    uint8_t macAddr[6];

    char name[MAX_NAME_SIZE];
    char version[MAX_VERSION_SIZE];

    char defaultHostname[MAX_SSID_SIZE];
    char hostname[MAX_SSID_SIZE];
    char passkeyLocal[MAX_PASSKEY_SIZE];

    char ssid[MAX_SSID_SIZE];
    char passkey[MAX_PASSKEY_SIZE];

    char mqttHost[MAX_MQTT_HOST_SIZE];
    char mqttUser[MAX_MQTT_USER_SIZE];
    char mqttPass[MAX_MQTT_PASS_SIZE];
    int mqttPort = 1883;
    void (*mqttCallback)(char*, uint8_t*, unsigned int);
    char topicPrefix[MAX_SSID_SIZE];

    char *willTopic, *willMessage;
    uint8_t willQos;
    bool willRetain;

    WiFiClient wifiClient;
    PubSubClient *mqtt;
    ESP8266WebServer *server;

    char *updateUrl = NULL;

    void setupWiFi();
    void setupMQTT();
    void setupOTA();
    void setupHTTPServer();

    void loadNetworkConfig();
    void saveNetworkConfig();

    void loadMQTTConfig();
    void saveMQTTConfig();

    void handleNetworkScanPage();
    void handleNetworkConfig();
    void handleMQTTPage();
    void handleMQTTConfig();
    void handleUpdate();

    void restart();
    boolean mqttReconnect();

    void initToSaneValues();
};