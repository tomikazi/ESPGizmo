#include <ESPGizmo.h>
#include <ESPGizmoHTML.h>
#include <FS.h>

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <ESP8266httpUpdate.h>
#include <ArduinoOTA.h>

#define WIFI_DATA   "cfg/wifi"
#define MQTT_DATA   "cfg/mqtt"

// WiFi connection attributes
#define WIFI_CHANNEL        6
#define MAX_CONNECTIONS     2

#define MAX_HTML    2048
static char html[MAX_HTML];

static boolean disconnected = true;
static uint32_t restartTime;

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

ESP8266WebServer *ESPGizmo::httpServer() {
    return server;
}

void ESPGizmo::beginSetup(char *_name, char *_version, char *_passkey) {
    Serial.begin(115200);
    SPIFFS.begin();

    strncpy(name, _name, MAX_NAME_SIZE - 1);
    strncpy(version, _version, MAX_VERSION_SIZE - 1);
    strncpy(passkeyLocal, _passkey, MAX_PASSKEY_SIZE - 1);
    Serial.printf("\n\n%s version %s\n\n", name, version);

    setupWiFi();
    setupOTA();
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
    snprintf(html, MAX_HTML, NET_HTML, hostname, nets, ssid, passkey,
             disconnected ? "not connected" : WiFi.localIP().toString().c_str());
    server->send(200, "text/html", html);
}

void ESPGizmo::handleNetworkConfig() {
    strncpy(hostname, server->arg("name").c_str(), MAX_SSID_SIZE - 1);
    strncpy(ssid, server->arg("net").c_str(), MAX_SSID_SIZE - 1);
    strncpy(passkey, server->arg("pwd").c_str(), MAX_PASSKEY_SIZE - 1);
    Serial.printf("Reconfiguring for connection to %s\n", ssid);

    snprintf(html, MAX_HTML, NETCFG_HTML, ssid);
    server->send(200, "text/html", html);

    saveNetworkConfig();
    WiFi.disconnect(true);
    restartTime = millis() + 1000;
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
        Serial.printf("No WiFi connection configured\n", ssid);
    }

    if (strlen(hostname) < 2) {
        uint8_t macAddr[6];
        WiFi.softAPmacAddress(macAddr);
        sprintf(hostname, "%s-%02X%02X%02X", name, macAddr[3], macAddr[4], macAddr[5]);
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

void ESPGizmo::setupHTTPServer() {
    server = new ESP8266WebServer(80);
    server->on("/nets", std::bind(&ESPGizmo::handleNetworkScan, this));
    server->on("/netcfg", std::bind(&ESPGizmo::handleNetworkConfig, this));
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

int ESPGizmo::updateSoftware(char *url, char *version) {
    Serial.printf("Updating software from %s; current version %s\n", url, version);
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

bool ESPGizmo::isNetworkAvailable(void (*afterConnection)()) {
    if (WiFi.status() == WL_CONNECTED) {
        if (disconnected) {
            disconnected = false;
            if (afterConnection) {
                afterConnection();
            }
            ArduinoOTA.begin();
            MDNS.addService("http", "tcp", 80);
        }
    }
    ArduinoOTA.handle();
    server->handleClient();

    if (restartTime && restartTime < millis()) {
        restart();
    }
    return !disconnected;
}

char *trimwhitespace(char *str) {
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
        trimwhitespace(hostname);
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
