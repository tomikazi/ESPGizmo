#include <ESPGizmo.h>

ESPGizmo gizmo;

#define PASSKEY "gizmo123"

void defaultMqttCallback(char *topic, uint8_t *payload, unsigned int length) {
    char value[64];
    value[0] = '\0';
    strncat(value, (char *) payload, length);
    Serial.printf("%s: %s\n", topic, value);
    gizmo.handleMQTTMessage(topic, value);
}
