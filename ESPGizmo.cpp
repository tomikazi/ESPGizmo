#include <ESPGizmo.h>
#include <ESPGizmoHTML.h>
#include <FS.h>

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>

#define WIFI_DATA   "cfg/wifi"
#define MQTT_DATA   "cfg/mqtt"

// WiFi connection attributes
#define WIFI_CHANNEL        6
#define MAX_CONNECTIONS     2

#define MAX_HTML    4096
static char html[MAX_HTML];

static boolean disconnected = true;
static uint32_t restartTime;

ESPGizmo::ESPGizmo() {
}

const char *ESPGizmo::getName() {
    return name;
}

const char *ESPGizmo::getLocalSSID() {
    return ssidLocal;
}

const char *ESPGizmo::getSSID() {
    return ssid;
}

ESP8266WebServer *ESPGizmo::httpServer() {
    return server;
}

void ESPGizmo::beginSetup(char *_name, char *_passkey) {
    Serial.begin(115200);
    Serial.println();

    strncpy(name, _name, MAX_NAME_SIZE - 1);
    strncpy(passkeyLocal, _passkey, MAX_PASSKEY_SIZE - 1);

    uint8_t macAddr[6];
    WiFi.softAPmacAddress(macAddr);
    sprintf(ssidLocal, "%s-%02X%02X%02X", name, macAddr[3], macAddr[4], macAddr[5]);

    SPIFFS.begin();

    setupWiFi();
    setupHTTPServer();
}

void ESPGizmo::endSetup() {
    server->begin();
    Serial.println("HTTP server started");
}

void ESPGizmo::handleNetworkScan() {
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
    snprintf(html, MAX_HTML, NET_HTML, nets, ssid, passkey);
    server->send(200, "text/html", html);
}

void ESPGizmo::handleNetworkConfig() {
    strncpy(ssid, server->arg("net").c_str(), MAX_SSID_SIZE - 1);
    strncpy(passkey, server->arg("pwd").c_str(), MAX_PASSKEY_SIZE - 1);
    Serial.printf("Reconfiguring for connection to %s\n", ssid);

    snprintf(html, MAX_HTML, NETCFG_HTML, ssid);
    server->send(200, "text/html", html);

    saveNetworkConfig();
    restartTime = millis() + 1000;
}

void ESPGizmo::restart() {
    Serial.println("Restarting...");
    ESP.restart();
}

void ESPGizmo::setupWiFi() {
    loadNetworkConfig();
    if (strlen(ssid)) {
        Serial.printf("Attempting connection to %s\n", ssid);
        WiFi.begin(ssid, passkey);
    } else {
        Serial.printf("No WiFi connection configured\n", ssid);
    }

    apIP = new IPAddress(10, 10, 10, 1);
    netMask = new IPAddress(255, 255, 255, 0);

    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(ssidLocal, passkeyLocal, WIFI_CHANNEL, false, MAX_CONNECTIONS);
    WiFi.softAPConfig(*apIP, *apIP, *netMask);
    Serial.printf("WiFi %s started\n", ssidLocal);
}

void ESPGizmo::setupHTTPServer() {
    server = new ESP8266WebServer(80);
    server->on("/nets", std::bind(&ESPGizmo::handleNetworkScan, this));
    server->on("/netcfg", std::bind(&ESPGizmo::handleNetworkConfig, this));
}

bool ESPGizmo::isNetworkAvailable(void (*afterConnection)()) {
    if (WiFi.status() == WL_CONNECTED) {
        if (disconnected) {
            disconnected = false;
            if (afterConnection) {
                afterConnection();
            }

            // ArduinoOTA.begin();

            // Add service to MDNS-SD
            MDNS.addService("http", "tcp", 80);
        }
    }
    server->handleClient();

    if (restartTime && restartTime < millis()) {
        restart();
    }
    return !disconnected;
}


void ESPGizmo::loadNetworkConfig() {
    File f = SPIFFS.open(WIFI_DATA, "r");
    if (f) {
        int l = f.readBytesUntil('|', ssid, MAX_SSID_SIZE);
        ssid[l] = NULL;
        l = f.readBytesUntil('|', passkey, MAX_PASSKEY_SIZE);
        passkey[l] = NULL;
        f.close();
    }
}

void ESPGizmo::saveNetworkConfig() {
    File f = SPIFFS.open(WIFI_DATA, "w");
    if (f) {
        f.printf("%s|%s|\n", ssid, passkey);
        f.close();
    }
}