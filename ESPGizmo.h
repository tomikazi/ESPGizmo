#pragma once

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <NTPClient.h>

#define MAX_NAME_SIZE       64
#define MAX_VERSION_SIZE    16
#define MAX_SSID_SIZE       32
#define MAX_PASSKEY_SIZE    32
#define MAX_MAC_SIZE        18

#define MAX_MQTT_HOST_SIZE  32
#define MAX_MQTT_USER_SIZE  32
#define MAX_MQTT_PASS_SIZE  32

class ESPGizmo {
public:
    ESPGizmo();

    void setNetworkConfig(const char *filename);
    void setNoNetworkConfig();
    void suggestIP(IPAddress ipAddress);
    void beginSetup(const char *name, const char *version, const char *passkey);
    void endSetup();

    const IPAddress getIP();
    const char *getName();
    const char *getHostname();
    const char *getTopicPrefix();
    const char *getSSID();
    const char *getMAC();

    void led(boolean on);

    void setCallback(void (*callback)(char*, uint8_t*, unsigned int));
    void addTopic(const char *topic);
    void addTopic(const char *topic, const char *uniqueName);
    void publish(const char *topic, char *payload);
    void publish(const char *topic, char *payload, boolean retain);
    void schedulePublish(const char *topic, char *payload);
    void schedulePublish(const char *topic, char *payload, boolean retain);

    void publish(const char *topic, const char *payload);
    void publish(const char *topic, const char *payload, boolean retain);
    void schedulePublish(const char *topic, const char *payload);
    void schedulePublish(const char *topic, const char *payload, boolean retain);

    bool publishBinarySensor(bool nv, bool ov, const char *topic);

    ESP8266WebServer *httpServer();
    void setUpdateURL(const char *url);
    void setUpdateURL(const char *url, void (*callback)());
    void setupWebRoot();

    void setupPinger();
    void handlePinger();

    void setupNTPClient();
    NTPClient *timeClient();

    bool isNetworkAvailable(void (*afterConnection)());

    void scheduleRestart();
    void scheduleUpdate();
    void scheduleFileUpdate();

    int updateSoftware(const char *url);
    int updateFiles(const char *url);

    void handleMQTTMessage(const char *topic, const char *value);

    // Not implemented yet
    void setMQTTLastWill(const char* willTopic, const char* willMessage,
                         uint8_t willQos, bool willRetain);

    void debug(const char *msg, ...);
    boolean debugEnabled = false;

private:
    IPAddress apIP = IPAddress(10, 10, 10, 1);
    uint8_t macAddr[6];
    const char *networkConfig = "cfg/wifi";

    char name[MAX_NAME_SIZE];
    char version[MAX_VERSION_SIZE];
    char mac[MAX_MAC_SIZE];

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

    const char *willTopic = NULL;
    const char *willMessage = NULL;
    uint8_t willQos = 0;
    bool willRetain = false;

    bool updatingFiles = false;
    bool fileUploadFailed = false;

    WiFiClient wifiClient;
    PubSubClient *mqtt = NULL;
    ESP8266WebServer *server = NULL;
    NTPClient *ntpClient = NULL;

    char *updateUrl = NULL;

    void setupWiFi();
    void setupMQTT();
    void setupOTA();
    void setupHTTPServer();

    void (*onUpdate)();
    int downloadAndSave(const char *url, const char *file);

    void loadNetworkConfig();
    void saveNetworkConfig();

    void loadMQTTConfig();
    void savePasskey(const char *psk);
    void saveMQTTConfig();

    void handleRoot();
    void handleNetworkScanPage();
    void handleNetworkConfig();
    void handleEraseConfig();
    void handleMQTTPage();
    void handleMQTTConfig();
    void handlePasskey();
    void handleFiles();
    void handleUpdate();
    void handleDoUpdate();
    void handleDoFileUpdate();
    void handleErase();
    void handleReset();
    void handleHotSpotDetect();
    void handleNotFound();
    void preUpload();
    void startUpload();
    void handleUpload();
    void updateAnnounceMessage();
    void readCustomPasskey(const char *defaultPasskey);

    void restart();
    boolean mqttReconnect();

    void initToSaneValues();
};

extern char *normalizeFile(const char *file);