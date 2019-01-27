# ComfoAirClient-Arduino
Arduino Client for intercepting &amp; parsing Zehnder RS232 communications between the main unit and the control panel. In my Comfo D450, the serial port is exposed on the main board of the ventilation unit using a standard DB9 serial connector. 

# Hardware Setup
Connect the Zehnder unit's serial port to your Arduino device using a proper level shifter (e.g. the MAX232 for 5V or MAX3232 for 3.3V devices). The current code assumes connectivity over Ethernet. I'm using a PoE setup for an Arduino Mega 2560.

# Software Setup
The code is written such that resulting temperature measurements are transmitted to an MQTT server on the local network.

