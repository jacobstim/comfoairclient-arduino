/* =============================================================================
   Network.cpp
   Written in 2018 by Tim Jacobs
  ============================================================================= */

#include "network.h"

/*=============================================================================
   GLOBAL VARIABLES 
  ============================================================================= */

// Main network interface
EthernetClient netClient;

// External variables
extern IPAddress netMQTTServer;
extern byte netMAC[];
extern IPAddress netIP;
extern IPAddress netDNS;
extern IPAddress netGW;
extern IPAddress netSubNet;

/*=============================================================================
   FUNCTIONS
  ============================================================================= */

void(* SoftReset) (void) = 0;       // declare reset function @ address 0

void networkInit() {
    // Initialize network
    DEBUGOUT.println(F("Init network connection..."));
    DEBUGOUT.println(F("-> Trying to get an IP address using DHCP"));
    if (Ethernet.begin(netMAC) == 0) {
        DEBUGOUT.println(F("-> Failed to configure Ethernet using DHCP; trying static config!"));
        // initialize the Ethernet device not using DHCP:
        Ethernet.begin(netMAC, netIP, netDNS, netGW, netSubNet);
        // Wait until Ethernet client ready
        DEBUGOUT.print(F("-> Connecting..."));
        int loopcounter = 0;
        while(!netClient){
            // Wait until there is a client connected to proceed
            delay(250);
            DEBUGOUT.print(F("."));
            loopcounter++;
            if (loopcounter > 120) {
                DEBUGOUT.println(); DEBUGOUT.println(F("ERROR! Cannot connect to network; rebooting!"));
                delay(2000);
                SoftReset();
            }
        }
        DEBUGOUT.println(F(" DONE!"));
    }
    
    // Print our local IP address:
    DEBUGOUT.print("-> Current IP address: ");
    netIP = Ethernet.localIP();
    for (byte thisByte = 0; thisByte < 4; thisByte++) {
        // print the value of each byte of the IP address:
        DEBUGOUT.print(netIP[thisByte], DEC);
        DEBUGOUT.print(".");
    }
    DEBUGOUT.println();
}