/* =============================================================================
   MQTT.h
   Written in 2018 by Tim Jacobs
  ============================================================================= */

#ifndef __COMFOAIR_ARDUINO_MQTT_H
#define __COMFOAIR_ARDUINO_MQTT_H

#define MQTT_MAX_PACKET_SIZE 256
#include <PubSubClient.h>
#include "network.h"


// MQTT IP configuration
#define MQTTPORT 1883
#define MQTTCLIENTNAME "arduinoClient"

// MQTT Topics
#define MQTTPUBTOPIC_SYSTEM "smarthome/ventilation/zehnder450D/system"            // Publish system messages here
#define MQTTPUBTOPIC_DATA "smarthome/ventilation/zehnder450D/data"                // Publish measurements here
#define MQTTSUBTOPIC "smarthome/ventilation/zehnder450D/board"                    // Subscribe here

// MQTT Messages
#define MQTT_MSG_CONNECTIONOK "command=00 status=1"

// Function declarations
void mqttInit();
boolean mqttReconnect();
void mqttMaintain();
void mqttCallback(char* topic, byte* payload, unsigned int length);

boolean mqttPublishData(String payload);

#endif
