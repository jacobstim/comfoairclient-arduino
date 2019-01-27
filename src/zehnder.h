/* =============================================================================
   Zehnder.h
   Written in 2018 by Tim Jacobs
  ============================================================================= */

#ifndef __COMFOAIR_ARDUINO_ZEHNDER_H
#define __COMFOAIR_ARDUINO_ZEHNDER_H

#include <Arduino.h> 

/* --------------------------------------------------------------------------
   Zehnder RS232 related data
   -------------------------------------------------------------------------- */

//SoftwareSerial zehnderPort(10, 11); // RX, TX
#define ZEHNDER_PORT Serial1
#define ZEHNDER_BAUDRATE 9600
#define ZEHNDER_SERIALSETTINGS SERIAL_8N1

// Parser configuration
#define CACMD_MINLENGTH 8                       // Minimum Zehnder CA command length 
#define CACMD_MAXLENGTH (uint16_t)(CACMD_MINLENGTH+255)   // Maximum command length; since "data length" is 1 byte in Zehnder command, there can be at most 255 data bytes.

// Start and stop of command
const byte cacmd_StartCMD[] = { 0x07, 0xF0 };
const byte cacmd_StopCMD[] = { 0x07, 0x0F };

// Function declarations
void zehnderInit();
void checkCommand();
bool processCommand();
float getTemperature(byte zehnderTemp);
int checkBuffers(byte* newdata, int newdatasize, int offset);
int findSequence(byte* dataarray, int datalength, int pos, const byte* sequence, int sequencelength);
void dumpByteArray(byte* databuffer, int datalength);
String byteToHexString(char c);

#endif
