#include <PubSubClient.h>
#include <Ethernet2.h>

/* --------------------------------------------------------------------------
   Definitions
   -------------------------------------------------------------------------- */

#define OUTPUTSER Serial                  // debug output port
#define ARDUINO_BAUDRATE 57600            // debug output port baud rate

#define USEMQTT                           // Comment if you want to disable MQTT and only want console logging
//#define TRACE                           // Dump a lot of data to serial output

/* --------------------------------------------------------------------------
   Main variables
   -------------------------------------------------------------------------- */

#ifdef USEMQTT
// Main network interface
EthernetClient netClient;

// Network parameters
byte netMAC[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress netIP(172, 16, 0, 70);
IPAddress netDNS(171,16,0,1);
IPAddress netGW(172, 16, 0, 1);
IPAddress netSubNet(255, 255, 255, 0);

// The IP address of the server we're connecting to:
IPAddress netMQTTServer(172, 16, 0, 6);
#define MQTTPORT 1883

// MQTT Client
PubSubClient mqttClient(netClient);
long lastReconnectAttempt = 0;

// MQTT Topics
#define MQTTPUBTOPIC_SYSTEM "ZehnderComfoD450-SYS"             // Publish system messages here
#define MQTTPUBTOPIC_DATA "ZehnderComfoD450-DATA"           // Publish measurements here
#define MQTTSUBTOPIC "ZehnderArduino"               // Subscribe here

#endif

/* --------------------------------------------------------------------------
   Zehnder RS232 related data
   -------------------------------------------------------------------------- */

#define CACMD_MINLENGTH 8                       // Minimum Zehnder CA command length 
#define CACMD_MAXLENGTH (uint16_t)(CACMD_MINLENGTH+255)   // Maximum command length; since "data length" is 1 byte in Zehnder command, there can be at most 255 data bytes.

const byte cacmd_StartCMD[] = { 0x07, 0xF0 };
const byte cacmd_StopCMD[] = { 0x07, 0x0F };

// Zehnder serial definitions
HardwareSerial& zehnderPort = Serial1;
//SoftwareSerial zehnderPort(10, 11); // RX, TX
#define ZEHNDER_BAUDRATE 9600
#define ZEHNDER_SERIALSETTINGS SERIAL_8N1

//
// Serial buffers
//
byte serialBuffer[64];                              // To hold serial data 
#define PROCESSBUFFERSIZE 2*CACMD_MAXLENGTH
byte processBuffer[PROCESSBUFFERSIZE];              // Our backlog for processing data
int processSize = 0;                                // How much data we have in the backlog
#define CMDBUFFERSIZE CACMD_MAXLENGTH
byte cmdBuffer[CMDBUFFERSIZE];                      // Our command buffer for parsing
int cmdSize = 0;                                    // How much data we have in the cmdBuffer



/* --------------------------------------------------------------------------
   Misc paraphernalia
   -------------------------------------------------------------------------- */

void(* SoftReset) (void) = 0; // declare reset function @ address 0


/* ===========================================================================
   ===========================================================================
   ===========================================================================
   MAIN SETUP
   ===========================================================================
   ===========================================================================
   =========================================================================== */

void setup() {

      OUTPUTSER.begin(ARDUINO_BAUDRATE);
      OUTPUTSER.println("");
      OUTPUTSER.println(F("=== SETUP START ==="));

      // Init serial port reader...
      OUTPUTSER.println(F("Init Serial port..."));
      zehnderPort.begin(ZEHNDER_BAUDRATE, SERIAL_8N1 );

      // Initialize MQTT
#ifdef USEMQTT
      OUTPUTSER.println(F("Preparing MQTT client..."));
      mqttClient.setServer(netMQTTServer, MQTTPORT);
      mqttClient.setCallback(mqttCallback);

      // Initialize network
      OUTPUTSER.println(F("Init network connection..."));
      OUTPUTSER.println(F("-> Trying to get an IP address using DHCP"));
      if (Ethernet.begin(netMAC) == 0) {
          OUTPUTSER.println(F("-> Failed to configure Ethernet using DHCP; trying static config!"));
          // initialize the Ethernet device not using DHCP:
          Ethernet.begin(netMAC, netIP, netDNS, netGW, netSubNet);
          // Wait until Ethernet client ready
          OUTPUTSER.print(F("-> Connecting..."));
          int loopcounter = 0;
          while(!netClient){
              // Wait until there is a client connected to proceed
              delay(250);
              OUTPUTSER.print(F("."));
              loopcounter++;
              if (loopcounter > 120) {
                  OUTPUTSER.println(); OUTPUTSER.println(F("ERROR! Cannot connect to network; rebooting!"));
                  delay(2000);
                  SoftReset();
              }
          }
          OUTPUTSER.println(F(" DONE!"));
      }
      
      // Print our local IP address:
      OUTPUTSER.print("-> Current IP address: ");
      netIP = Ethernet.localIP();
      for (byte thisByte = 0; thisByte < 4; thisByte++) {
          // print the value of each byte of the IP address:
          OUTPUTSER.print(netIP[thisByte], DEC);
          OUTPUTSER.print(".");
      }
      OUTPUTSER.println();
#endif
      
      OUTPUTSER.println(F("=== SETUP DONE ==="));
}


/* ===========================================================================
   ===========================================================================
   ===========================================================================
   MAIN LOOP
   ===========================================================================
   ===========================================================================
   =========================================================================== */



void loop() {

#ifdef USEMQTT
    //
    // DHCP lease refresh?
    //
    Ethernet.maintain();
    
    //
    // MQTT still connected?
    // 
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
 #endif
 
    // New data available at the serial port?
    int bytesToRead = zehnderPort.available();
    if (bytesToRead > 0) {
        // Read data from the serial port
        zehnderPort.readBytes(serialBuffer, bytesToRead);

#ifdef TRACE
        // Output to serial port what we read
        OUTPUTSER.print("### UART: ");
        dumpByteArray(serialBuffer, bytesToRead);
#endif

        int parseoffset = -1;
        int previousparse = 0;
        // Keep looping through old data + new data, as long as we keep on finding commands 
        while (parseoffset != 0) {
            previousparse = parseoffset;
            // Check if we have a complete Zehnder command
            parseoffset = checkBuffers(serialBuffer, bytesToRead, previousparse);
            //OUTPUTSER.print("### PARSEOFFSET: "); OUTPUTSER.println(parseoffset);

            // Command found?
            if (cmdSize > 0) {
                OUTPUTSER.print("### CMDSIZE: ");
                OUTPUTSER.print(cmdSize);
                OUTPUTSER.print(" / CMDBUFFER: ");
                dumpByteArray(cmdBuffer, cmdSize);
                processCommand();
           }          
        }
        if (previousparse < 0) { previousparse = 0; };
        
        // Retain all data past the "previousparse" = point to which we processed already
        if (previousparse < bytesToRead) {
            int lengthToStore = (bytesToRead - previousparse);
            // Does it fit in our processBuffer?
            if (processSize + lengthToStore > PROCESSBUFFERSIZE){
                // Not enough room! Just forget whatever we have now...
                processSize = 0;
            }
            memcpy(processBuffer+processSize, serialBuffer+previousparse, lengthToStore);
            
            processSize += lengthToStore;
//            OUTPUTSER.print("### KEEPING: ");
//            OUTPUTSER.print(lengthToStore);
//            OUTPUTSER.print(" ### STORESIZE: ");
//            OUTPUTSER.print(processSize);
//            OUTPUTSER.print(" / STORE: ");
//            dumpByteArray(processBuffer, processSize);
        }
    }
}

/* ===========================================================================
   ===========================================================================
   ===========================================================================
   MQTT COMMUNICATION CODE
   ===========================================================================
   ===========================================================================
   =========================================================================== */

#ifdef USEMQTT
boolean mqttReconnect() {
     // Succesfully reconnected
     OUTPUTSER.print(F("Attempting to connect to MQTT server... "));
     if (mqttClient.connect("arduinoClient")) {
        OUTPUTSER.println(F("SUCCESS!"));
        // Once connected, publish an announcement...
        mqttClient.publish(MQTTPUBTOPIC_SYSTEM,"Device booted and ready!");
        // ... and resubscribe
        mqttClient.subscribe(MQTTSUBTOPIC);
      } else {
        OUTPUTSER.println(F("FAILED!"));
      }
      return mqttClient.connected();
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
    // handle message arrived
}

#endif

/* ===========================================================================
   ===========================================================================
   ===========================================================================
   ZEHNDER PARSING CODE
   ===========================================================================
   ===========================================================================
   =========================================================================== */

// ---------------------------------------------------------------------------
// PROCESSCOMMAND
// ---------------------------------------------------------------------------
// This function takes the command in the current cmdBuffer and acts
// accordingly.
//
// INPUTS:
//    none
// OUTPUTS:
//    bool           TRUE if command was succesfully parsed, FALSE otherwise
// ---------------------------------------------------------------------------

bool processCommand() {
//    uint8_t cmdByte1 = cmdBuffer[2];
    uint8_t cmdByte2 = cmdBuffer[3];
    String cmdString = byteToHexString(cmdByte2);
    String cmdData = "";
    
    OUTPUTSER.print(F("CMD = 0x")); OUTPUTSER.print(cmdString);

    bool parsed = false;
    
    switch(cmdByte2) {
        case 0xD2:
            // ReadTemperatures Extended command
            //  - Provides details of the temperature sensors in the unit
            //  - Example raw string: 
            //    07F0 00D2 09504C4D54540F282828A0070F07F3 07F0
            //  - Relevant data interpretation:
            //       Byte[1] - Komfort Temperatur (°C*)
            //       Byte[2] - T1 / Außenluft (°C*)
            //       Byte[3] - T2 / Zuluft (°C*)
            //       Byte[4] - T3 / Abluft (°C*)
            //       Byte[5] - T4 / Fortluft (°C*)
            cmdData = String(getTemperature(cmdBuffer[5])) + "," + String(getTemperature(cmdBuffer[6])) + "," + String(getTemperature(cmdBuffer[7])) + "," + String(getTemperature(cmdBuffer[8])) + "," + String(getTemperature(cmdBuffer[9]));
            OUTPUTSER.print(F(" ::: Comfort,T1 Outside, T2 To Home, T3 From Home, T4 Exhaust = ")), OUTPUTSER.println(cmdData);
            parsed = true;
            break;
        default :
            OUTPUTSER.println(F(" - Not parsed"));
    }


    if (parsed) {
 #ifdef USEMQTT
        // Publish data to MQTT queue
        if (mqttClient.connected()) {
            String dataOut = String(cmdString + "," + cmdData);      // 
            char __dataOut[255];
            dataOut.toCharArray(__dataOut, 255);
            mqttClient.publish(MQTTPUBTOPIC_DATA,__dataOut);
        }
#endif      
    }
    return true;
}

// ---------------------------------------------------------------------------
// GETTEMPERATURE
// ---------------------------------------------------------------------------
// Zehnder returns (TEMP + 20) * 2 as a byte encoding for the temperature. For
// output reasons we'll want to convert that back... 

float getTemperature(byte zehnderTemp) {
    return (((float)zehnderTemp / 2) - 20);
}

// ---------------------------------------------------------------------------
// CHECKBUFFERS
// ---------------------------------------------------------------------------
// Method to check if we have a full Zehnder command in our current memory
// buffers, that is in the "newdata" and our existing processBuffer. This
// method will check if we find a complete Zehnder command either in the
// existing processBuffer, or if we join the processBuffer with the newdata.
// If a command is found, it is copied into the cmdBuffer.
//
// INPUTS:
//    newdata       Array of new data to look for a complete command in
//    newdatasize   Size of the newdata array
//    offset        Starting position in new data to search
// OUTPUTS:
//    result        -1    A command was found completely in the processBuffer
//                  int   The offset in newdata where a command finishes.
// ---------------------------------------------------------------------------

int checkBuffers(byte* newdata, int newdatasize, int offset) {
    int cmdstop = -1;
    cmdSize = 0;                // Reset Zehnder command in memory 

    // CASE1: CMDSTART in OLDDATA, CMDSTOP in OLDDATA
    // Find command start code
    int cmdstart = findSequence(processBuffer, processSize, 0, cacmd_StartCMD, 2);
    if (cmdstart >= 0) {
        // Start of command found in the old data buffer: stop of command also in old data buffer?
        cmdstop = findSequence(processBuffer, processSize, 2, cacmd_StopCMD, 2);
        if (cmdstop >= 0) {
            // Start of command found and stop of command found in old data
            int lengthToStore = cmdstop+2-cmdstart;
            memcpy(cmdBuffer, processBuffer+cmdstart, lengthToStore);
            cmdSize = lengthToStore;
            
            // Clean up: retain unparsed data in the processBuffer
            int remainderSize = processSize - cmdstop - 2;
            if (remainderSize > 0) {
                memmove(processBuffer, processBuffer+cmdstop+2,remainderSize);
            }
            processSize = remainderSize;
            return -1;       // We parsed nothing in the new data... 
        }

        // CASE2: CMDSTART in OLDDATA, CMDSTOP partially in OLDDATA & partially in NEWDATA
        if (processSize > 0) {
            if ((processBuffer[processSize-1] == cacmd_StopCMD[0]) & (newdata[0] == cacmd_StopCMD[1])) {
                // Copy first part of command (all except last stop byte)
                int lengthToStore = processSize-cmdstart;
                memcpy(cmdBuffer, processBuffer+cmdstart, lengthToStore);
                cmdBuffer[lengthToStore] = cacmd_StopCMD[1];      // manually add last byte
                cmdSize = lengthToStore+1;
                // Clean up: we got the entire old dataset, so drop it
                processSize = 0;
                return 1;                                         // we parsed only the first byte from the new data
            }
        }        
        
        // CASE3: CMDSTART in OLDDATA, CMDSTOP in NEWDATA
        // Command stop in new data??
        cmdstop = findSequence(newdata, newdatasize, 0, cacmd_StopCMD, 2);
        if (cmdstop >= 0) {
            // Start of command found in old data and stop of command found in new data
            int lengthToStore = processSize-cmdstart;
            memcpy(cmdBuffer, processBuffer+cmdstart, lengthToStore);
            cmdSize = lengthToStore;               
            memcpy(cmdBuffer+cmdSize, newdata, cmdstop+2);
            cmdSize += cmdstop+2;

            // Clean up: drop processBuffer, return parsing position
            processSize = 0;
            return cmdstop+2;
        }

    } else if (processSize > 0) {
        // CASE4: CMDSTART partially in OLDDATA, CMDSTART partially in NEWDATA, CMDSTOP in NEWDATA
        if ((processBuffer[processSize-1] == cacmd_StartCMD[0]) & (newdata[0] == cacmd_StartCMD[1])) {
            cmdstop = findSequence(newdata, newdatasize, 0, cacmd_StopCMD, 2);
            if (cmdstop >= 0) {
                cmdBuffer[0] = cacmd_StartCMD[0];                   // manually set first byte
                int lengthToStore = cmdstop+2;
                memcpy(cmdBuffer+1, newdata, cmdstop+2);
                cmdSize = cmdstop+3;
                 
                // Clean up: drop processBuffer, return parsing position
                processSize = 0;
                return cmdstop+2;               
            }          
        }
    } 
    // CASE5: CMDSTART in NEWDATA, CMDSTOP in NEWDATA (using OFFSET)
    // Find command start code
    cmdstart = findSequence(newdata, newdatasize, offset, cacmd_StartCMD, 2);
    if (cmdstart >= 0) {
        // Start of command found in the new data: stop of command also in new data buffer?
        cmdstop = findSequence(newdata, newdatasize, 2+cmdstart, cacmd_StopCMD, 2);
        if (cmdstop >= 0) {
            // Start of command found and stop of command found in old data
            int lengthToStore = cmdstop+2-cmdstart;
            memcpy(cmdBuffer, newdata+cmdstart, lengthToStore);
            cmdSize = lengthToStore;
            // Return parsing position
            return cmdstop+2;
        }
    }
    // No command found in any buffer
    cmdSize = 0;
    return 0; 
}

// ---------------------------------------------------------------------------
// FINDSEQUENCE
// ---------------------------------------------------------------------------
// Linear search for a particular byte sequence inside a byte array.
// TODO: can this be optimized using memchr?
//
// INPUTS:
//    dataarray         The array to search within
//    datalength        The length of the dataarray
//    pos               The starting position for the search
//    sequence          The byte sequence to search for within dataarray
//    sequencelength    The length of the sequence to search for
// OUTPUTS:
//    int               The starting position where the sequence was found
// ---------------------------------------------------------------------------

int findSequence(byte* dataarray, int datalength, int pos, const byte* sequence, int sequencelength) {
    if (pos<0) { pos = 0; }                            // Sanity check
    if ((datalength>=sequencelength) && (sequencelength>0)) {
        int endpoint = datalength - sequencelength +1;     // past here no match is possible
        byte firstByte = sequence[0];
        while (pos < endpoint) {
           if (dataarray[pos] == firstByte) {
               // scan for rest of sequence
               for (int offset2 = 1; offset2 < sequencelength; ++offset2) {
                   if (dataarray[pos + offset2] != sequence[offset2]) {
                       break; // mismatch? continue scanning with next byte
                   }
                   else if (offset2 == sequencelength - 1) {
                       return pos; // all bytes matched!
                   } // if
               } // for
           } // if
           ++pos;
       } // while
    } // if
 
    // end of array reached without match
    return -1;
}

/* ===========================================================================
   ===========================================================================
   ===========================================================================
   DATA CONVERSION CODE
   ===========================================================================
   ===========================================================================
   =========================================================================== */

void dumpByteArray(byte* databuffer, int datalength) {
    for (int i=0; i < datalength; i++) {
        OUTPUTSER.print(byteToHexString(databuffer[i]));          
    }
    OUTPUTSER.println();
}

String byteToHexString(char c) {
    char result[3];
    result[0] = ((c & 0xF0) >> 4) + 0x30;
    if (result[0] > 0x39) result[0] +=0x07;
    result[1] = (c & 0x0F) + 0x30;
    if (result[1] > 0x39) result[1] +=0x07;
    result[2] = 0x00;
    return String(result);
}




