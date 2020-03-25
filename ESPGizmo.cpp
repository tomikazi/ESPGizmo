#include <ESPGizmo.h>
#include <ESPGizmoHTML.h>
#include <FS.h>

#include <ESP8266mDNS.h>
#include <ESP8266httpUpdate.h>
#include <ArduinoOTA.h>
#include <DNSServer.h>

#define LED 2

// WiFi connection attributes
#define WIFI_CHANNEL       11
#define MAX_CONNECTIONS     6

#define GIZMO_CONSOLE_TOPIC   "gizmo/console"
#define GIZMO_CONTROL_TOPIC  "gizmo/control"

#define MQTT_RECONNECT_FREQUENCY    5000

#define MAX_TOPIC_SIZE  64
#define MAX_TOPIC_COUNT 16
static char topics[MAX_TOPIC_COUNT][MAX_TOPIC_SIZE];
static int topicCount = 0;

static boolean callAfterConnection = false;
static boolean booted = false;
static boolean disconnected = true;
static boolean mqttConfigured = false;
static uint32_t lastReconnectAttempt = 0;
static uint32_t restartTime = 0;
static uint32_t updateTime = 0;
static uint32_t fileUpdateTime = 0;

static const char *scheduledTopic = NULL;
static char *scheduledPayload = NULL;
static boolean scheduledRetain = false;

#define MAX_ANNOUNCE_MESSAGE_SIZE   128
static char announceMessage[MAX_ANNOUNCE_MESSAGE_SIZE];

#define DNS_PORT    53

DNSServer dnsServer;

#define OFFLINE_TIMEOUT     60000
static uint32_t offlineTime;

WiFiUDP ntpUDP;

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

const char *ESPGizmo::getMAC() {
    return mac;
}

const IPAddress ESPGizmo::getIP() {
    return apIP;
};

const char *ESPGizmo::getTopicPrefix() {
    return topicPrefix[0] != '\0' ? topicPrefix : hostname;
}

ESP8266WebServer *ESPGizmo::httpServer() {
    return server;
}

NTPClient *ESPGizmo::timeClient() {
    return ntpClient;
}

void ESPGizmo::led(boolean on) {
    digitalWrite(LED, on ? LOW : HIGH);
}

void ESPGizmo::initToSaneValues() {
    ssid[0] = '\0';
    passkey[0] = '\0';
    mqttHost[0] = '\0';
    mqttUser[0] = '\0';
    mqttPass[0] = '\0';
}

void ESPGizmo::setCallback(void (*callback)(char *, uint8_t *, unsigned int)) {
    mqttCallback = callback;
}

void replaceSubstring(char *string, const char *sub, const char *rep) {
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
        replaceSubstring(topics[topicCount], "%s", uniqueName);
        topicCount = topicCount + 1;
    }
}

void ESPGizmo::publish(const char *topic, char *payload) {
    publish(topic, payload, false);
}

void ESPGizmo::publish(const char *topic, const char *payload) {
    publish(topic, (char *) payload);
}

void ESPGizmo::publish(const char *topic, char *payload, boolean retain) {
    if (mqttConfigured && mqtt) {
        if (strstr(topic, "%s")) {
            char tt[MAX_TOPIC_SIZE];
            snprintf(tt, MAX_TOPIC_SIZE, topic, getTopicPrefix());
            mqtt->publish(tt, payload, retain);
        } else {
            mqtt->publish(topic, payload, retain);
        }
    } else {
        Serial.printf("no mqtt...");
    }
}

void ESPGizmo::publish(const char *topic, const char *payload, boolean retain) {
    publish(topic, (char *) payload, retain);
}

void ESPGizmo::schedulePublish(const char *topic, char *payload, boolean retain) {
    scheduledRetain = retain;
    scheduledPayload = payload;
    scheduledTopic = topic;
}

void ESPGizmo::schedulePublish(const char *topic, char *payload) {
    schedulePublish(topic, payload, false);
}

void ESPGizmo::schedulePublish(const char *topic, const char *payload, boolean retain) {
    schedulePublish(topic, (char *) payload, retain);
}

void ESPGizmo::schedulePublish(const char *topic, const char *payload) {
    schedulePublish(topic, payload, false);
}

void ESPGizmo::handleMQTTMessage(const char *topic, const char *value) {
    if (!strcmp(topic, GIZMO_CONTROL_TOPIC)) {
        if (!strcmp(value, "version")) {
            schedulePublish((char *) GIZMO_CONSOLE_TOPIC, announceMessage, false);
        } else if (!strncmp(value, "update ", 7) &&
                   (strstr(hostname, value+7) || (strlen(topicPrefix) && strstr(topicPrefix, value+7)))) {
            scheduleUpdate();
        } else if (!strncmp(value, "fileUpdate ", 11) &&
                   (strstr(hostname, value+11) || (strlen(topicPrefix) && strstr(topicPrefix, value+11)))) {
            scheduleFileUpdate();
        } else if (!strncmp(value, "restart ", 8) && strstr(hostname, value+8)) {
            scheduleRestart();
        } else if (!strncmp(value, "debug=", 6) && strstr(hostname, value+8)) {
            debugEnabled = value[6] == 'y';
        }
    }
}

bool ESPGizmo::publishBinarySensor(bool nv, bool ov, const char *topic) {
    if (nv && !ov) {
        publish(topic, "on", true);
    } else if (!nv && ov) {
        publish(topic, "off", true);
    }
    return nv;
}

void ESPGizmo::debug(const char *fmt, ...) {
    if (debugEnabled) {
        va_list argList;
        va_start(argList, fmt);
        char dmsg[256];
        dmsg[0] = '\0';
        vsnprintf(dmsg, 255, fmt, argList);
        va_end(argList);

        char line[300];
        line[0] = '\0';
        snprintf(line, 299, "%s: %s", getHostname(), dmsg);
        publish("gizmo/console", line);
    }
}

void ESPGizmo::suggestIP(IPAddress ipAddress) {
    apIP = ipAddress;
}

void ESPGizmo::beginSetup(const char *_name, const char *_version, const char *_passkey) {
    Serial.begin(115200);
    pinMode(LED, OUTPUT);
    led(true);
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

void ESPGizmo::setupNTPClient() {
    ntpClient = new NTPClient(ntpUDP, "pool.ntp.org", -7 * 3600);
}

void ESPGizmo::endSetup() {
    offlineTime = strlen(getSSID()) ? millis() + OFFLINE_TIMEOUT : millis();
    server->begin();
    Serial.println("HTTP server started");
}

void ESPGizmo::scheduleRestart() {
    Serial.printf("Scheduling restart\n");
    restartTime = millis() + 1500;
}

void ESPGizmo::scheduleUpdate() {
    Serial.printf("Scheduling update\n");
    updateTime = millis() + 1500;
}

void ESPGizmo::scheduleFileUpdate() {
    Serial.printf("Scheduling file update\n");
    fileUpdateTime = millis() + 1500;
}

void ESPGizmo::handleRoot() {
    File f = SPIFFS.open("/index.html", "r");
    server->streamFile(f, "text/html");
    f.close();
}

void ESPGizmo::handleNetworkScanPage() {
    server->setContentLength(CONTENT_LENGTH_UNKNOWN);
    server->send(200, "text/html", HTML_HEAD);
    server->sendContent("Network Setup");
    server->sendContent(HTML_TITLE_END);
    server->sendContent(HTML_CSS_MENU);
    server->sendContent(HTML_BODY);
    server->sendContent("Network Setup");
    server->sendContent(HTML_MENU);

    server->sendContent("<form action=\"/netcfg\"><h3>Name</h3><input type=\"text\" name=\"name\" value=\"");
    if (strlen(hostname)) server->sendContent(hostname);
    server->sendContent(
            "\" size=\"30\"><h3>Network</h3><select id=\"netlist\" onchange='document.getElementById(\"net\").value = document.getElementById(\"netlist\").value'>");

    int n = WiFi.scanNetworks();
    for (int i = 0; i < n; i++) {
        server->sendContent("<option value=\"");
        server->sendContent(WiFi.SSID(i).c_str());
        if (!strcmp(WiFi.SSID(i).c_str(), ssid)) {
            server->sendContent("\" selected>");
        } else {
            server->sendContent("\">");
        }
        server->sendContent(WiFi.SSID(i).c_str());
        server->sendContent("</option>");
    }

    server->sendContent("</select><p><input type=\"text\" id=\"net\" name=\"net\" value=\"");
    if (strlen(ssid)) server->sendContent(ssid);
    server->sendContent("\" size=\"30\"><h3>Password</h3><input type=\"password\" name=\"pass\" value=\"");
    if (strlen(passkey)) server->sendContent(passkey);
    server->sendContent("\" size=\"30\"><p><h3>IP Address</h3>");
    server->sendContent(disconnected ? "not connected" : WiFi.localIP().toString().c_str());
    server->sendContent("<p><p><h3>MAC Address</h3>");
    server->sendContent(getMAC());
    server->sendContent("<p><input type=\"submit\" value=\"Apply Changes\"></form>");
    server->sendContent(HTML_END);
    server->sendContent("");
}

void ESPGizmo::handleNetworkConfig() {
    strncpy(hostname, server->arg("name").c_str(), MAX_SSID_SIZE - 1);
    strncpy(ssid, server->arg("net").c_str(), MAX_SSID_SIZE - 1);
    strncpy(passkey, server->arg("pass").c_str(), MAX_PASSKEY_SIZE - 1);
    Serial.printf("Reconfiguring for connection to %s\n", ssid);

    server->setContentLength(CONTENT_LENGTH_UNKNOWN);
    server->send(200, "text/html", HTML_HEAD);
    server->sendContent("Network Configured");
    server->sendContent(HTML_TITLE_END);
    server->sendContent(HTML_REDIRECT_START);
    server->sendContent("/nets");
    server->sendContent(HTML_REDIRECT_END);
    server->sendContent(HTML_CSS_MENU);
    server->sendContent(HTML_BODY);
    server->sendContent("Network Configured");
    server->sendContent(HTML_MENU);
    server->sendContent("<p>Reconfigured WiFi for connection to ");
    if (strlen(ssid)) server->sendContent(ssid);
    server->sendContent(". Restarting...</p>");
    server->sendContent(HTML_END);
    server->sendContent("");

    saveNetworkConfig();
    WiFi.disconnect(true);
    scheduleRestart();
}

void ESPGizmo::handleEraseConfig() {
    Serial.printf("Resetting configuration\n");

    server->setContentLength(CONTENT_LENGTH_UNKNOWN);
    server->send(200, "text/html", HTML_HEAD);
    server->sendContent("Config Reset");
    server->sendContent(HTML_TITLE_END);
    server->sendContent(HTML_REDIRECT_START);
    server->sendContent("/nets");
    server->sendContent(HTML_REDIRECT_END);
    server->sendContent(HTML_CSS_MENU);
    server->sendContent(HTML_BODY);
    server->sendContent("Config Reset");
    server->sendContent(HTML_MENU);
    server->sendContent("<p>Erasing configuration. Restarting...</p>");
    server->sendContent(HTML_END);
    server->sendContent("");

    SPIFFS.remove("/cfg/wifi");
    SPIFFS.remove("/cfg/mqtt");
    WiFi.disconnect(true);
    scheduleRestart();
}

void ESPGizmo::handleMQTTPage() {
    char port[8];

    server->setContentLength(CONTENT_LENGTH_UNKNOWN);
    server->send(200, "text/html", HTML_HEAD);
    server->sendContent("MQTT Setup");
    server->sendContent(HTML_TITLE_END);
    server->sendContent(HTML_CSS_MENU);
    server->sendContent(HTML_BODY);
    server->sendContent("MQTT Setup");
    server->sendContent(HTML_MENU);

    server->sendContent("<form action=\"/mqttcfg\"><h3>MQTT Host</h3><input type=\"text\" name=\"host\" value=\"");
    if (strlen(mqttHost)) server->sendContent(mqttHost);
    server->sendContent("\" size=\"30\"><h3>MQTT Port</h3><input type=\"text\" name=\"port\" value=\"");
    snprintf(port, 8, "%d", mqttPort);
    server->sendContent(port);
    server->sendContent("\" size=\"30\"><p><h3>User Name</h3><input type=\"password\" name=\"user\" value=\"");
    if (strlen(mqttUser)) server->sendContent(mqttUser);
    server->sendContent("\" size=\"30\"><p><h3>Password</h3><input type=\"password\" name=\"pass\" value=\"");
    if (strlen(mqttPass)) server->sendContent(mqttPass);
    server->sendContent("\" size=\"30\"><p><h3>Topic Prefix</h3><input type=\"text\" name=\"prefix\" value=\"");
    if (strlen(topicPrefix)) server->sendContent(topicPrefix);
    server->sendContent("\" size=\"30\"><p><input type=\"submit\" value=\"Apply Changes\"></form>");
    server->sendContent(HTML_END);
    server->sendContent("");
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

    server->setContentLength(CONTENT_LENGTH_UNKNOWN);
    server->send(200, "text/html", HTML_HEAD);
    server->sendContent("MQTT Reconfigured");
    server->sendContent(HTML_TITLE_END);
    server->sendContent(HTML_REDIRECT_START);
    server->sendContent("/mqtt");
    server->sendContent(HTML_REDIRECT_END);
    server->sendContent(HTML_CSS_MENU);
    server->sendContent(HTML_BODY);
    server->sendContent("MQTT Reconfigured");
    server->sendContent(HTML_MENU);
    server->sendContent("<p>Reconfigured MQTT for connection to ");
    if (strlen(mqttHost)) server->sendContent(mqttHost);
    server->sendContent(". Restarting...</p>");
    server->sendContent(HTML_END);
    server->sendContent("");

    saveMQTTConfig();
    WiFi.disconnect(true);
    scheduleRestart();
}

void ESPGizmo::handleUpdate() {
    server->setContentLength(CONTENT_LENGTH_UNKNOWN);
    server->send(200, "text/html", HTML_HEAD);
    server->sendContent("Software Update");
    server->sendContent(HTML_TITLE_END);
    server->sendContent(HTML_CSS_MENU);
    server->sendContent(HTML_BODY);
    server->sendContent("Software Update");
    server->sendContent(HTML_MENU);

    server->sendContent("<h3>Name</h3>");
    if (strlen(name)) server->sendContent(name);
    server->sendContent("<h3>Version</h3>");
    if (strlen(version)) server->sendContent(version);
    server->sendContent("<h3>URL</h3>");
    if (strlen(updateUrl)) server->sendContent(updateUrl);

    server->sendContent("<p><form action=\"/doupdate\"><input type=\"submit\" value=\"Update\"></form>");
    server->sendContent("<p><form action=\"/dofileupdate\"><input type=\"submit\" value=\"Update Files\"></form>");
    server->sendContent("<br><br><form action=\"/reset\"><input type=\"submit\" value=\"Reset\"></form>");
    server->sendContent("<br><br><form action=\"javascript:if (confirm('This will erase custom configuration!')) { window.location.href = '/erase'; }\"><input type=\"submit\" value=\"Erase Config\"></form>");
    server->sendContent(HTML_END);
    server->sendContent("");
}

void ESPGizmo::handleDoUpdate() {
    server->setContentLength(CONTENT_LENGTH_UNKNOWN);
    server->send(200, "text/html", HTML_HEAD);
    server->sendContent("Update Requested");
    server->sendContent(HTML_TITLE_END);
    server->sendContent(HTML_REDIRECT_LONG_START);
    server->sendContent("/update");
    server->sendContent(HTML_REDIRECT_END);
    server->sendContent(HTML_CSS_MENU);
    server->sendContent(HTML_BODY);
    server->sendContent("Update Requested");
    server->sendContent(HTML_MENU);
    server->sendContent("<p>Update requested from ");
    if (strlen(updateUrl)) server->sendContent(updateUrl);
    server->sendContent("</p><p>Restarting...</p>");
    server->sendContent(HTML_END);
    server->sendContent("");
    scheduleUpdate();
}

void ESPGizmo::handleDoFileUpdate() {
    server->setContentLength(CONTENT_LENGTH_UNKNOWN);
    server->send(200, "text/html", HTML_HEAD);
    server->sendContent("Updating Files");
    server->sendContent(HTML_TITLE_END);
    server->sendContent(HTML_REDIRECT_START);
    server->sendContent("/files");
    server->sendContent(HTML_REDIRECT_END);
    server->sendContent(HTML_CSS_MENU);
    server->sendContent(HTML_BODY);
    server->sendContent("Updating Files");
    server->sendContent(HTML_MENU);
    server->sendContent("<p>Update requested from ");
    if (strlen(updateUrl)) server->sendContent(updateUrl);
    server->sendContent("</p><p>Please wait...</p>");
    server->sendContent(HTML_END);
    server->sendContent("");
    scheduleFileUpdate();
}

void ESPGizmo::handleReset() {
    server->setContentLength(CONTENT_LENGTH_UNKNOWN);
    server->send(200, "text/html", HTML_HEAD);
    server->sendContent("Resetting...");
    server->sendContent(HTML_TITLE_END);
    server->sendContent(HTML_REDIRECT_START);
    server->sendContent("/update");
    server->sendContent(HTML_REDIRECT_END);
    server->sendContent(HTML_CSS_MENU);
    server->sendContent(HTML_BODY);
    server->sendContent("Resetting...");
    server->sendContent(HTML_MENU);
    server->sendContent("<p>Reset requested!</p><p>Please wait...</p>");
    server->sendContent(HTML_END);
    server->sendContent("");
    scheduleRestart();
}

void listDir(ESP8266WebServer *server, const char *path) {
    Dir dir = SPIFFS.openDir(path);
    while (dir.next()) {
        char line[128];
        char name[48];
        name[0] = '\0';
        strncat(name, dir.fileName().c_str(), 47);
        Serial.printf("%s\t%d\n", name, dir.fileSize());
        snprintf(line, 127, "%-32s %8d<br>", name, dir.fileSize());
        server->sendContent(line);
    }
}

void ESPGizmo::handleFiles() {
    server->setContentLength(CONTENT_LENGTH_UNKNOWN);
    server->send(200, "text/html", HTML_HEAD);
    server->sendContent("Files");
    server->sendContent(HTML_TITLE_END);
    server->sendContent(HTML_CSS_MENU);
    server->sendContent(HTML_BODY);
    server->sendContent("Files");
    server->sendContent(HTML_MENU);
    server->sendContent("<pre>");

    listDir(server, "");

    server->sendContent("</pre>");
    server->sendContent("<p><form action=\"/dofileupdate\"><input type=\"submit\" value=\"Update Files\"></form>");
    if (updatingFiles) {
        server->sendContent("<p>File update in progress...<p>");
    }
    if (fileUploadFailed) {
        server->sendContent("<p>File update failed!<p>");
    }
    server->sendContent(HTML_END);
    server->sendContent("");
}

static File uploadFile;
static uint32_t uploadSize;

void ESPGizmo::preUpload() {
    if (onUpdate) {
        onUpdate();
    }
    server->send(200, "text/plain", "");
}

void ESPGizmo::startUpload() {
    server->send(200, "text/plain", "");
}

void ESPGizmo::handleUpload() {
    static char name[64];

    HTTPUpload &upload = server->upload();
    snprintf(name, 63, "/%s", upload.filename.c_str());
    Serial.printf("Uploading %s... phase %d, total=%u, current=%u, name=%s, type=%s\n",
                  name, upload.status, upload.totalSize, upload.currentSize,
                  upload.name.c_str(), upload.type.c_str());

    if (upload.status == UPLOAD_FILE_START) {
        Serial.printf("Starting upload for %s\n", name);
        uploadFile = SPIFFS.open(name, "w");
        if (!uploadFile) {
            Serial.printf("Upload failed to open destination file\n");
        }
        uploadSize = 0;

    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (uploadFile) {
            uploadSize += uploadFile.write(upload.buf, upload.currentSize);
        }

    } else if (upload.status == UPLOAD_FILE_END) {
        if (uploadFile) {
            uploadFile.close();
        }
        Serial.printf("Uploaded %u bytes\n", uploadSize);
    }
    yield();
}

void ESPGizmo::restart() {
    Serial.println("Restarting...");
    ESP.restart();
}


int captiveCount = 0;

void ESPGizmo::handleHotSpotDetect() {
    Serial.printf("hotSpotDetect [%d, %s]\n", captiveCount, server->uri().c_str());
    if (captiveCount == 0) {
        server->send(200, "text/html", "<HTML><HEAD><TITLE>Captive</TITLE></HEAD><BODY>Captive</BODY></HTML>");
        captiveCount++;
    } else if (captiveCount == 1) {
        char buf[2048];
        snprintf(buf, 2047, WELCOME_HTML, apIP.toString().c_str(), apIP.toString().c_str());
        server->send(200, "text/html", buf);
        captiveCount++;
    } else {
        server->send(200, "text/html", "<HTML><HEAD><TITLE>Success</TITLE></HEAD><BODY>Success</BODY></HTML>");
        captiveCount = 0;
    }
}

void ESPGizmo::handleNotFound() {
    Serial.printf("notFound [%d, %s]\n", captiveCount, server->uri().c_str());
    handleNetworkScanPage();
    captiveCount = 0;
}

void ESPGizmo::setNetworkConfig(const char *filename) {
    networkConfig = filename;
    Serial.printf("Switching WiFi configuration to %s...\n", networkConfig);
    WiFi.disconnect(true);
    setupWiFi();
}

void ESPGizmo::setNoNetworkConfig() {
    networkConfig = "";
    Serial.printf("Switching WiFi configuration to AP only\n");
    WiFi.disconnect(true);
    setupWiFi();
}

void ESPGizmo::setupWiFi() {
    WiFi.hostname(hostname);
    WiFi.setAutoConnect(false);
    ssid[0] = '\0';

    if (strlen(networkConfig)) {
        loadNetworkConfig();
    }

    boolean isStation = strlen(ssid);
    if (isStation) {
        Serial.printf("Attempting connection to %s\n", ssid);
        WiFi.persistent(false);
        WiFi.begin(ssid, passkey);
    } else {
        Serial.printf("No WiFi connection configured\n");
    }

    WiFi.softAPmacAddress(macAddr);
    sprintf(mac, "%02X:%02X:%02X:%02X:%02X:%02X",
            macAddr[0], macAddr[1], macAddr[2], macAddr[3], macAddr[4], macAddr[5]);
    sprintf(defaultHostname, "%s-%02X%02X%02X", name, macAddr[3], macAddr[4], macAddr[5]);
    if (strlen(hostname) < 2) {
        strcpy(hostname, defaultHostname);
    }

    uint8_t mode = 0;
    wifi_softap_set_dhcps_offer_option(OFFER_ROUTER, &mode);

    snprintf(announceMessage, MAX_ANNOUNCE_MESSAGE_SIZE, "%s (%s)", hostname, version);

    // If we don't have an SSID configured to which to connect to,
    // start as a visible access point otherwise, start as a hidden access point/station
    WiFi.mode(isStation ? WIFI_AP_STA : WIFI_AP);
    WiFi.softAP(hostname, passkeyLocal, WIFI_CHANNEL, isStation, MAX_CONNECTIONS);

    Serial.printf("WiFi is %s\n", isStation ? "hidden" : "visible");

    IPAddress netMask = IPAddress(255, 255, 255, 0);
    WiFi.softAPConfig(apIP, apIP, netMask);

    dnsServer.start(DNS_PORT, "*", apIP);

    Serial.printf("WiFi %s started with gateway IP %d.%d.%d.%d\n", hostname, apIP[0], apIP[1], apIP[2], apIP[3]);
    delay(100);
}

void ESPGizmo::setupMQTT() {
    loadMQTTConfig();
    if (mqttHost && strlen(mqttHost)) {
        Serial.printf("Attempting connection to MQTT server %s\n", mqttHost);
        mqttConfigured = true;
    } else {
        Serial.println("No MQTT server configured");
    }
    delay(100);
}

void ESPGizmo::setupHTTPServer() {
    server = new ESP8266WebServer(80);
    server->on("/nets", std::bind(&ESPGizmo::handleNetworkScanPage, this));
    server->on("/netcfg", std::bind(&ESPGizmo::handleNetworkConfig, this));
    server->on("/mqtt", std::bind(&ESPGizmo::handleMQTTPage, this));
    server->on("/mqttcfg", std::bind(&ESPGizmo::handleMQTTConfig, this));
    server->on("/uploadprep", std::bind(&ESPGizmo::preUpload, this));
    server->on("/upload", HTTP_POST, std::bind(&ESPGizmo::startUpload, this), std::bind(&ESPGizmo::handleUpload, this));
    server->on("/reset", std::bind(&ESPGizmo::handleReset, this));
    server->on("/files", std::bind(&ESPGizmo::handleFiles, this));
    server->on("/erase", std::bind(&ESPGizmo::handleEraseConfig, this));
    server->on("/hotspot-detect.html", std::bind(&ESPGizmo::handleHotSpotDetect, this));
}

void ESPGizmo::setUpdateURL(const char *url) {
    setUpdateURL(url, NULL);
}

void ESPGizmo::setUpdateURL(const char *url, void (*callback)()) {
    onUpdate = callback;
    updateUrl = (char *) url;
    if (strlen(updateUrl)) {
        server->on("/update", std::bind(&ESPGizmo::handleUpdate, this));
        server->on("/doupdate", std::bind(&ESPGizmo::handleDoUpdate, this));
        server->on("/dofileupdate", std::bind(&ESPGizmo::handleDoFileUpdate, this));
    }
}

void ESPGizmo::setupWebRoot() {
    server->on("/", std::bind(&ESPGizmo::handleRoot, this));
    server->serveStatic("/", SPIFFS, "/", "max-age=86400");
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
    Serial.printf("Updating software from %s; current version %s\n", url, version);
    t_httpUpdate_return ret = ESPhttpUpdate.update(url, version);
    switch (ret) {
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

bool isUpTodate(const char *file, const char *etag) {
    char efn[48], et[32];
    snprintf(efn, 47, "/etags%s", file);
    File f = SPIFFS.open(efn, "r");
    if (f) {
        int l = f.readBytesUntil('\n', et, 31);
        et[l] = '\0';
        f.close();
        return !strcmp(et, etag);
    }
    return false;
}

void saveEtag(const char *file, const char *etag) {
    char efn[48];
    snprintf(efn, 47, "/etags%s", file);
    File f = SPIFFS.open(efn, "w");
    if (f) {
        f.printf("%s\n", etag);
        f.close();
    }
}

int ESPGizmo::downloadAndSave(const char *url, const char *file) {
    char xurl[256];
    snprintf(xurl, 255, "%s.data%s", url, file);
    Serial.printf("Starting download of %s...\n", file);

    HTTPClient httpClient;
    httpClient.begin(xurl);

    const char *headerKeys[] = {"Content-Length", "ETag"};
    httpClient.collectHeaders(headerKeys, 2);

    int code = httpClient.GET();
    if (code != HTTP_CODE_OK) {
        Serial.printf("Unable to download %s\n", xurl);
        return 0;
    }

    if (!isUpTodate(file, httpClient.header("ETag").c_str())) {
        int length = httpClient.header("Content-Length").toInt();
        int downloaded = 0;
        WiFiClient *stream = httpClient.getStreamPtr();
        if (stream) {
            File f = SPIFFS.open(file, "w");
            if (f) {
                Serial.printf("Downloading %d bytes of %s ... ", length, file);
                uint8_t buf[1024];
                size_t rl;
                while ((rl = stream->read(buf, 1024)) > 0) {
                    f.write(buf, rl);
                    downloaded += rl;
                    delay(20);
                    yield();
                }
                f.close();
                Serial.printf("%d bytes\n", downloaded);
                if (length == downloaded) {
                    saveEtag(file, httpClient.header("ETag").c_str());
                }
            }
        }
        httpClient.end();
        delay(100);
        return length == downloaded;
    }
    return 1;
}

int ESPGizmo::updateFiles(const char *url) {
    updatingFiles = true;
    fileUploadFailed = !downloadAndSave(url, "/catalog");
    File cat = SPIFFS.open("/catalog", "r");
    if (cat && !fileUploadFailed) {
        char file[32];
        int l;
        while ((l = cat.readBytesUntil('\n', file, 31)) > 0) {
            file[l] = '\0';
            int t = 10;
            while (!downloadAndSave(url, file) && t > 0) {
                Serial.printf("Failed to download file %s completely; retrying\n", file);
                t--;
                delay(500);
            }
            if (t == 0) {
                Serial.printf("Failed to download file %s\n", file);
                fileUploadFailed = true;
            }
        };
        cat.close();
    }
    updatingFiles = false;
    return 0;
}

boolean ESPGizmo::mqttReconnect() {
    Serial.printf("Attempting connection to MQTT server %s as %s/%s\n",
                  mqttHost, mqttUser, mqttPass);
    if (mqtt->connect(defaultHostname, mqttUser, mqttPass)) {
        // Once connected, publish an announcement and subscribe...
        mqtt->publish(GIZMO_CONSOLE_TOPIC, announceMessage, false);
        booted = true;
        updateAnnounceMessage();

        mqtt->subscribe(GIZMO_CONTROL_TOPIC);
        for (int i = 0; i < topicCount; i++) {
            mqtt->subscribe(topics[i]);
        }
    }
    return mqtt->connected();
}

void ESPGizmo::updateAnnounceMessage() {
    snprintf(announceMessage, MAX_ANNOUNCE_MESSAGE_SIZE, "%s (%s) %s/%s %s",
             hostname, version, WiFi.localIP().toString().c_str(), mac,
             booted ? "reconnect" : "boot");
}

bool ESPGizmo::isNetworkAvailable(void (*afterConnection)()) {
    boolean wifiReady = WiFi.status() == WL_CONNECTED;
    boolean mqttReady = (mqttConfigured && mqtt && mqtt->connected()) || !mqttConfigured;

    if (wifiReady) {
        if (disconnected) {
            disconnected = false;
            callAfterConnection = true;
            Serial.printf("Connected to %s with IP %s\n", ssid, WiFi.localIP().toString().c_str());

            updateAnnounceMessage();
            mqtt = new PubSubClient(mqttHost, mqttPort, wifiClient);
            mqtt->setCallback(mqttCallback);

            ArduinoOTA.begin();
            if (MDNS.begin(hostname)) {
                MDNS.addService("http", "tcp", 80);
            }
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
                if (scheduledTopic) {
                    publish(scheduledTopic, scheduledPayload, scheduledRetain);
                    scheduledTopic = NULL;
                    scheduledPayload = NULL;
                }
                mqtt->loop();
            }
        }

        if (callAfterConnection && mqttReady && afterConnection) {
            if (ntpClient) {
                ntpClient->begin();
            }
            callAfterConnection = false;
            offlineTime = 0;
            afterConnection();
            led(false);
        }

        ArduinoOTA.handle();
    }
    dnsServer.processNextRequest();
    server->handleClient();

    if (ntpClient) {
        ntpClient->update();
    }

    if (updateTime && updateTime < millis()) {
        if (onUpdate) {
            onUpdate();
        }
        updateSoftware(updateUrl);
        updateTime = 0;
    }

    if (fileUpdateTime && fileUpdateTime < millis()) {
        if (onUpdate) {
            onUpdate();
        }
        updateFiles(updateUrl);
        fileUpdateTime = 0;
    }

    if (restartTime && restartTime < millis()) {
        restart();
    }

    if (!wifiReady) {
        disconnected = true;
        led(true);
    }

    // If we're still not ready and the offline time grace period ran-out, run without WiFi.
    if (!(wifiReady && mqttReady) && offlineTime && offlineTime < millis()) {
        setNoNetworkConfig();
        callAfterConnection = false;
        offlineTime = 0;
        afterConnection();
    }

    return wifiReady && mqttReady;
}

char *trimWhiteSpace(char *str) {
    char *end;

    // Trim leading space
    while (isspace((unsigned char) *str)) str++;

    if (*str == 0)  // All spaces?
        return str;

    // Trim trailing space
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char) *end)) end--;

    // Write new null terminator character
    end[1] = '\0';

    return str;
}

void ESPGizmo::loadNetworkConfig() {
    File f = SPIFFS.open(normalizeFile(networkConfig), "r");
    if (f) {
        int l = f.readBytesUntil('|', ssid, MAX_SSID_SIZE - 1);
        ssid[l] = '\0';
        l = f.readBytesUntil('|', passkey, MAX_PASSKEY_SIZE - 1);
        passkey[l] = '\0';
        l = f.readBytesUntil('|', hostname, MAX_SSID_SIZE - 1);
        hostname[l] = '\0';
        trimWhiteSpace(hostname);
        f.close();
    }
}

void ESPGizmo::saveNetworkConfig() {
    File f = SPIFFS.open("/cfg/wifi", "w");
    if (f) {
        f.printf("%s|%s|%s|\n", ssid, passkey, hostname);
        f.close();
    }
}

void ESPGizmo::setMQTTLastWill(const char *willTopic, const char *willMessage,
                               uint8_t willQos, bool willRetain) {
    Serial.printf("Not implemented yet: %s, %s, %d, %d", willTopic, willMessage, willQos, willRetain);
}

void ESPGizmo::loadMQTTConfig() {
    File f = SPIFFS.open(normalizeFile("cfg/mqtt"), "r");
    if (f) {
        int l = f.readBytesUntil('|', mqttHost, MAX_MQTT_HOST_SIZE - 1);
        char port[8];
        mqttHost[l] = '\0';
        l = f.readBytesUntil('|', port, 7);
        port[l] = '\0';
        mqttPort = atoi(port);
        l = f.readBytesUntil('|', mqttUser, MAX_MQTT_USER_SIZE - 1);
        mqttUser[l] = '\0';
        l = f.readBytesUntil('|', mqttPass, MAX_MQTT_PASS_SIZE - 1);
        mqttPass[l] = '\0';
        l = f.readBytesUntil('|', topicPrefix, MAX_SSID_SIZE - 1);
        topicPrefix[l] = '\0';
        f.close();
    }
}

void ESPGizmo::saveMQTTConfig() {
    File f = SPIFFS.open("/cfg/mqtt", "w");
    if (f) {
        f.printf("%s|%d|%s|%s|%s|\n", mqttHost, mqttPort, mqttUser, mqttPass, topicPrefix);
        f.close();
    }
}

static char normalized[36];

char *normalizeFile(const char *file) {
    if (file[0] == '/') {
        return (char *) file;
    }
    snprintf(normalized, 32, "/%s", file);
    if (SPIFFS.exists(file)) {
        Serial.printf("Normalizing %s\n", file);
        SPIFFS.rename(file, normalized);
    }
    return normalized;
}
