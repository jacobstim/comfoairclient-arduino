/* =============================================================================
   MQTT.cpp
   Written in 2018 by Tim Jacobs
  ============================================================================= */

#include "mqtt.h"

/*=============================================================================
   GLOBAL VARIABLES 
  ============================================================================= */

// External variables
extern IPAddress netMQTTServer_IP;
extern const char* netMQTTServer_DNS;
extern EthernetClient netClient;

// MQTT Client
PubSubClient mqttClient(netClient);
long lastReconnectAttempt = 0;

/*=============================================================================
   FUNCTIONS
  ============================================================================= */

void mqttInit() { 
    DEBUGOUT.println(F("Preparing MQTT client..."));
    mqttClient.setServer(netMQTTServer_DNS, MQTTPORT);
    mqttClient.setCallback(mqttCallback);
}

// Publish a payload to the MQTTPUBTOPIC_DATA topic
boolean mqttPublishData(String payload) {
    // Publish data to MQTT queue
    if (mqttClient.connected()) {
        char __dataOut[MQTT_MAX_PACKET_SIZE];
        payload.toCharArray(__dataOut, MQTT_MAX_PACKET_SIZE);
        if(mqttClient.publish(MQTTPUBTOPIC_DATA,__dataOut)) {
            return true;
        };
        DEBUGOUT.println(F("ERROR: Failed to publish to MQTT server!"));
    }    
    return false;
}


void mqttMaintain() {
    if (!mqttClient.connected()) {
      long now = millis();
      if (now - lastReconnectAttempt > 5000) {
        lastReconnectAttempt = now;
        // Attempt to reconnect
        if (mqttReconnect()) {
          lastReconnectAttempt = 0;
        }
      }
    } else {
      // Check MQTT situation
      mqttClient.loop();
    }
}

boolean mqttReconnect() {
     // Succesfully reconnected
     DEBUGOUT.print(F("Attempting to connect to MQTT server... "));
     if (mqttClient.connect(MQTTCLIENTNAME)) {
        DEBUGOUT.println(F("SUCCESS!"));
        // Once connected, publish an announcement...
        if(!mqttClient.publish(MQTTPUBTOPIC_SYSTEM, MQTT_MSG_CONNECTIONOK)) {
             DEBUGOUT.println(F("ERROR: Failed to publish to MQTT server!"));
        }
        // ... and resubscribe
        mqttClient.subscribe(MQTTSUBTOPIC);
      } else {
        DEBUGOUT.println(F("FAILED!"));
      }
      return mqttClient.connected();
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
    // handle message arrived
}
