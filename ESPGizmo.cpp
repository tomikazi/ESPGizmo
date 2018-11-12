#include <ESPGizmo.h>
#include <ESPGizmoHTML.h>
#include <FS.h>

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESP8266httpUpdate.h>
#include <ArduinoOTA.h>

#define WIFI_DATA   "cfg/wifi"
#define MQTT_DATA   "cfg/mqtt"

// WiFi connection attributes
#define WIFI_CHANNEL        6
#define MAX_CONNECTIONS     2

#define MAX_HTML    2048
static char html[MAX_HTML];

#define MQTT_RECONNECT_FREQUENCY    5000

#define MAX_TOPIC_SIZE  64
#define MAX_TOPIC_COUNT 16
static char topics[MAX_TOPIC_COUNT][MAX_TOPIC_SIZE];
static int topicCount = 0;

static boolean callAfterConnection = false;
static boolean disconnected = true;
static boolean mqttConfigured = false;
static uint32_t lastReconnectAttempt = 0;
static uint32_t restartTime = 0;
static uint32_t updateTime = 0;

ESPGizmo::ESPGizmo() {
}

const char *ESPGizmo::getName() {
    return name;
}

const char *ESPGizmo::getHostname() {
    return hostname;
}

const char *ESPGizmo::getSSID() {
    return ssid;
}

const char *ESPGizmo::getTopicPrefix() {
    return topicPrefix[0] != NULL ? topicPrefix : hostname;
}

ESP8266WebServer *ESPGizmo::httpServer() {
    return server;
}

void ESPGizmo::initToSaneValues() {
    ssid[0] = NULL;
    passkey[0] = NULL;
    mqttHost[0] = NULL;
    mqttUser[0] = NULL;
    mqttPass[0] = NULL;
}

void ESPGizmo::setCallback(void (*callback)(char*, uint8_t*, unsigned int)) {
    mqttCallback = callback;
}

void replaceSubstring(char *string,char *sub,char *rep) {
    int stringLen, subLen, newLen;
    int i = 0, j, k;
    int flag = 0, start, end;
    stringLen = strlen(string);
    subLen = strlen(sub);
    newLen = strlen(rep);

    for (i = 0; i < stringLen; i++) {
        flag = 0;
        start = i;
        for (j = 0; string[i] == sub[j]; j++, i++) /* Checks for the substring */
            if (j == subLen - 1)
                flag = 1;
        end = i;
        if (flag == 0)
            i -= j;
        else {
            for (j = start; j < end; j++) {
                for (k = start; k < stringLen; k++)
                    string[k] = string[k + 1];
                stringLen--;
                i--;
            }

            for (j = start; j < start + newLen; j++) {
                for (k = stringLen; k >= j; k--)
                    string[k + 1] = string[k];
                string[j] = rep[j - start];
                stringLen++;
                i++;
            }
        }
    }
}

void ESPGizmo::addTopic(const char *topic) {
    addTopic(topic, getTopicPrefix());
}

void ESPGizmo::addTopic(const char *topic, const char *uniqueName) {
    if (topicCount < MAX_TOPIC_COUNT) {
        strncpy(topics[topicCount], topic, MAX_TOPIC_SIZE - 1);
        replaceSubstring(topics[topicCount], "%s", (char *) uniqueName);
        topicCount = topicCount + 1;
    }
}

void ESPGizmo::publish(char *topic, char *payload) {
    publish(topic, payload, false);
}

void ESPGizmo::publish(char *topic, char *payload, boolean retain) {
    if (mqttConfigured && mqtt) {
        mqtt->publish(topic, payload, retain);
    }
}

void ESPGizmo::beginSetup(const char *_name, const char *_version, const char *_passkey) {
    Serial.begin(115200);
    SPIFFS.begin();

    initToSaneValues();

    strncpy(name, _name, MAX_NAME_SIZE - 1);
    strncpy(version, _version, MAX_VERSION_SIZE - 1);
    strncpy(passkeyLocal, _passkey, MAX_PASSKEY_SIZE - 1);
    Serial.printf("\n\n%s version %s\n\n", name, version);

    setupWiFi();
    setupMQTT();
    setupOTA();
    setupHTTPServer();
}

void ESPGizmo::endSetup() {
    server->begin();
    Serial.println("HTTP server started");
}

void ESPGizmo::scheduleRestart() {
    Serial.printf("Scheduling restart\n");
    restartTime = millis() + 1000;
}

void ESPGizmo::scheduleUpdate() {
    Serial.printf("Scheduling update\n");
    updateTime = millis() + 1000;
}


void ESPGizmo::handleNetworkScanPage() {
    char nets[MAX_HTML/2];
    int n = WiFi.scanNetworks();
    nets[0] = NULL;
    for (int i = 0; i < n; i++) {
        strcat(nets, "<option value=\"");
        strcat(nets, WiFi.SSID(i).c_str());
        strcat(nets, "\"");
        if (!strcmp(WiFi.SSID(i).c_str(), ssid)) {
            strcat(nets, "selected");
        }
        strcat(nets, ">");
        strcat(nets, WiFi.SSID(i).c_str());
        strcat(nets, "</option>");
    }
    snprintf(html, MAX_HTML, NET_HTML, hostname, nets, ssid, passkey,
             disconnected ? "not connected" : WiFi.localIP().toString().c_str());
    server->send(200, "text/html", html);
}

void ESPGizmo::handleNetworkConfig() {
    strncpy(hostname, server->arg("name").c_str(), MAX_SSID_SIZE - 1);
    strncpy(ssid, server->arg("net").c_str(), MAX_SSID_SIZE - 1);
    strncpy(passkey, server->arg("pass").c_str(), MAX_PASSKEY_SIZE - 1);
    Serial.printf("Reconfiguring for connection to %s\n", ssid);

    snprintf(html, MAX_HTML, NETCFG_HTML, ssid);
    server->send(200, "text/html", html);

    saveNetworkConfig();
    WiFi.disconnect(true);
    scheduleRestart();
}

void ESPGizmo::handleMQTTPage() {
    char nets[MAX_HTML/2];
    snprintf(html, MAX_HTML, MQTT_HTML, mqttHost, mqttPort, mqttUser, mqttPass, topicPrefix);
    server->send(200, "text/html", html);
}

void ESPGizmo::handleMQTTConfig() {
    char port[8];
    strncpy(mqttHost, server->arg("host").c_str(), MAX_MQTT_HOST_SIZE - 1);
    strncpy(port, server->arg("port").c_str(), 7);
    mqttPort = atoi(port);
    strncpy(mqttUser, server->arg("user").c_str(), MAX_MQTT_USER_SIZE - 1);
    strncpy(mqttPass, server->arg("pass").c_str(), MAX_MQTT_PASS_SIZE - 1);
    strncpy(topicPrefix, server->arg("prefix").c_str(), MAX_SSID_SIZE - 1);
    Serial.printf("Reconfiguring for connection to %s\n", mqttHost);

    snprintf(html, MAX_HTML, MQTTCFG_HTML, mqttHost);
    server->send(200, "text/html", html);

    saveMQTTConfig();
    scheduleRestart();
}

void ESPGizmo::handleUpdate() {
    snprintf(html, MAX_HTML, UPDATE_HTML, updateUrl, version);
    server->send(200, "text/html", html);
    scheduleUpdate();
}

void ESPGizmo::restart() {
    Serial.println("Restarting...");
    ESP.restart();
}

void ESPGizmo::setupWiFi() {
    WiFi.hostname(hostname);

    loadNetworkConfig();
    if (strlen(ssid)) {
        Serial.printf("Attempting connection to %s\n", ssid);
        WiFi.begin(ssid, passkey);
    } else {
        Serial.printf("No WiFi connection configured\n");
    }

    WiFi.softAPmacAddress(macAddr);
    sprintf(defaultHostname, "%s-%02X%02X%02X", name, macAddr[3], macAddr[4], macAddr[5]);
    if (strlen(hostname) < 2) {
        strcpy(hostname, defaultHostname);
    }

    Serial.printf("Hostname: %s\n", hostname);

    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(hostname, passkeyLocal, WIFI_CHANNEL, false, MAX_CONNECTIONS);

    IPAddress apIP = IPAddress(10, 10, 10, 1);
    IPAddress netMask = IPAddress(255, 255, 255, 0);
    WiFi.softAPConfig(apIP, apIP, netMask);

    Serial.printf("WiFi %s started\n", hostname);
    delay(100);
}

void ESPGizmo::setupMQTT() {
    loadMQTTConfig();
    if (strlen(mqttHost)) {
        Serial.printf("Attempting connection to MQTT server %s\n", mqttHost);
        mqttConfigured = true;
    } else {
        Serial.printf("No MQTT server configured\n");
    }
}

void ESPGizmo::setupHTTPServer() {
    server = new ESP8266WebServer(80);
    server->on("/nets", std::bind(&ESPGizmo::handleNetworkScanPage, this));
    server->on("/netcfg", std::bind(&ESPGizmo::handleNetworkConfig, this));
    server->on("/mqtt", std::bind(&ESPGizmo::handleMQTTPage, this));
    server->on("/mqttcfg", std::bind(&ESPGizmo::handleMQTTConfig, this));
}

void ESPGizmo::setUpdateURL(const char *url) {
    updateUrl = (char *) url;
    if (strlen(updateUrl)) {
        server->on("/update", std::bind(&ESPGizmo::handleUpdate, this));
    }
}

void ESPGizmo::setupOTA() {
    ArduinoOTA.setHostname(hostname);
    ArduinoOTA.onStart([]() {
        Serial.println("OTA Started");
    });
    ArduinoOTA.onEnd([]() {
        Serial.println("\nEnd");
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    });
    ArduinoOTA.onError([](ota_error_t error) {
        Serial.printf("Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
        else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
        else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
        else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
        else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });
}

int ESPGizmo::updateSoftware(const char *url) {
    Serial.printf("Updating software from %s ; current version %s\n", url, version);
    t_httpUpdate_return ret = ESPhttpUpdate.update(url, version);
    switch(ret) {
        case HTTP_UPDATE_FAILED:
            Serial.println("Software update failed.");
            return -1;
        case HTTP_UPDATE_NO_UPDATES:
            Serial.println("Software update not required.");
            return 0;
        case HTTP_UPDATE_OK:
            // may not be called due to race with reboot
            Serial.println("Software updated!");
            return 1;
    }
    return 0;
}

boolean ESPGizmo::mqttReconnect() {
    Serial.printf("Attempting connection to MQTT server %s as %s/%s\n",
            mqttHost, mqttUser, mqttPass);
    if (mqtt->connect(defaultHostname, mqttUser, mqttPass)) {
        // Once connected, publish an announcement...
        char message[64];
        snprintf(message, 64, "%s (%s)", hostname, version);
        mqtt->publish("gizmo/started", message);
        for (int i = 0; i < topicCount; i++) {
            mqtt->subscribe(topics[i]);
        }
    }
    return mqtt->connected();
}

bool ESPGizmo::isNetworkAvailable(void (*afterConnection)()) {
    boolean wifiReady = WiFi.status() == WL_CONNECTED;
    boolean mqttReady = (mqttConfigured && mqtt && mqtt->connected()) || !mqttConfigured;

    if (wifiReady) {
        if (disconnected) {
            disconnected = false;
            callAfterConnection = true;
            Serial.printf("Connected to %s with IP ", ssid);
            Serial.println(WiFi.localIP());

            mqtt = new PubSubClient(mqttHost, mqttPort, wifiClient);
            mqtt->setCallback(mqttCallback);

            ArduinoOTA.begin();
            MDNS.addService("http", "tcp", 80);
        }

        if (mqtt && mqttConfigured) {
            if (!mqtt->connected()) {
                uint32_t now = millis();
                if (now - lastReconnectAttempt > MQTT_RECONNECT_FREQUENCY) {
                    lastReconnectAttempt = now;
                    if (mqttReconnect()) {
                        lastReconnectAttempt = 0;
                    }
                }
            } else {
                mqtt->loop();
            }
        }

        if (callAfterConnection && mqttReady && afterConnection) {
            callAfterConnection = false;
            afterConnection();
        }
    }
    ArduinoOTA.handle();
    server->handleClient();

    if (updateTime && updateTime < millis()) {
        updateSoftware(updateUrl);
        updateTime = 0;
    }

    if (restartTime && restartTime < millis()) {
        restart();
    }

    if (!wifiReady) {
        disconnected = true;
    }

    return wifiReady && mqttReady;
}

char *trimWhiteSpace(char *str) {
    char *end;

    // Trim leading space
    while(isspace((unsigned char)*str)) str++;

    if(*str == 0)  // All spaces?
        return str;

    // Trim trailing space
    end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;

    // Write new null terminator character
    end[1] = '\0';

    return str;
}

void ESPGizmo::loadNetworkConfig() {
    File f = SPIFFS.open(WIFI_DATA, "r");
    if (f) {
        int l = f.readBytesUntil('|', ssid, MAX_SSID_SIZE - 1);
        ssid[l] = NULL;
        l = f.readBytesUntil('|', passkey, MAX_PASSKEY_SIZE - 1);
        passkey[l] = NULL;
        l = f.readBytesUntil('|', hostname, MAX_SSID_SIZE - 1);
        hostname[l] = NULL;
        trimWhiteSpace(hostname);
        f.close();
    }
}

void ESPGizmo::saveNetworkConfig() {
    File f = SPIFFS.open(WIFI_DATA, "w");
    if (f) {
        f.printf("%s|%s|%s|\n", ssid, passkey, hostname);
        f.close();
    }
}

void ESPGizmo::setMQTTLastWill(const char* willTopic, const char* willMessage,
                               uint8_t willQos, bool willRetain) {

}

void ESPGizmo::loadMQTTConfig() {
    File f = SPIFFS.open(MQTT_DATA, "r");
    if (f) {
        int l = f.readBytesUntil('|', mqttHost, MAX_MQTT_HOST_SIZE - 1);
        char port[8];
        mqttHost[l] = NULL;
        l = f.readBytesUntil('|', port, 7);
        port[l] = NULL;
        mqttPort = atoi(port);
        l = f.readBytesUntil('|', mqttUser, MAX_MQTT_USER_SIZE - 1);
        mqttUser[l] = NULL;
        l = f.readBytesUntil('|', mqttPass, MAX_MQTT_PASS_SIZE - 1);
        mqttPass[l] = NULL;
        l = f.readBytesUntil('|', topicPrefix, MAX_SSID_SIZE - 1);
        topicPrefix[l] = NULL;
        f.close();
    }
}

void ESPGizmo::saveMQTTConfig() {
    File f = SPIFFS.open(MQTT_DATA, "w");
    if (f) {
        f.printf("%s|%d|%s|%s|%s|\n", mqttHost, mqttPort, mqttUser, mqttPass, topicPrefix);
        f.close();
    }
}
