
#include "mqtt.h"
#include "network.h"
#include "zehnder.h"

/* --------------------------------------------------------------------------
   Definitions
   -------------------------------------------------------------------------- */

#define ARDUINO_BAUDRATE 9600             // debug output port baud rate

/* --------------------------------------------------------------------------
   Main variables
   -------------------------------------------------------------------------- */

// Network parameters
byte netMAC[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress netIP(172, 16, 0, 70);
IPAddress netDNS(171,16,0,1);
IPAddress netGW(172, 16, 0, 1);
IPAddress netSubNet(255, 255, 255, 0);

// The IP address of the server we're connecting to:
IPAddress netMQTTServer_IP(172, 16, 0, 6);
const char* netMQTTServer_DNS = "mqtt.home.local";

/* ===========================================================================
   ===========================================================================
   ===========================================================================
   MAIN SETUP
   ===========================================================================
   ===========================================================================
   =========================================================================== */

void setup() {
    DEBUGOUT.begin(ARDUINO_BAUDRATE);
    DEBUGOUT.println("");
    DEBUGOUT.println(F("=== SETUP START ==="));

    // Init serial port reader...
    zehnderInit();

    // Initialize MQTTClient
    mqttInit();

    // Initialize EthernetClient
    networkInit();

      
    DEBUGOUT.println(F("=== SETUP DONE ==="));
    pinMode(LED_BUILTIN, OUTPUT);
}


/* ===========================================================================
   ===========================================================================
   ===========================================================================
   MAIN LOOP
   ===========================================================================
   ===========================================================================
   =========================================================================== */

void loop() {
    // Maintain Ethernet connection (DHCP lease)
    Ethernet.maintain();
    
    // Maintain MQTT server connection
    mqttMaintain();

    // New data available at the serial port?
    checkCommand();
}
