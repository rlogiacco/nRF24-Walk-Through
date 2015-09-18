#include <SPI.h>
#include <EEPROM.h>
#include "nRF24L01.h"
#include "RF24.h"
#include "printf.h"

// enables/disable serial debugging
#define SERIAL_DEBUG true
#include <FormattingSerialDebug.h>

// creates a new instance using SPI plus pins 9 and 10
RF24 radio(9, 10);

// Where in EEPROM is the address stored?
#define EEPROM_ADDR 0

// nRF24 address family: all addresses will be in the format 0xFACEC0DE## with the last two
// bytes determined by the node identifier.
#define ADDR_FAMILY 0xFACEC0DE00LL
#define MAX_ID_VALUE 26

#define BUTTON_PIN 3

// This node unique identifier: 0 is used for the hub, anything above MAX_ID_VALUE is considered
// not valid
byte nodeId = 255;

void setup() {
	SERIAL_DEBUG_SETUP(9600);


	// Setup the push button
	pinMode(BUTTON_PIN, INPUT_PULLUP);

	// Read the address from EEPROM
	byte reading = EEPROM.read(EEPROM_ADDR);

	// If it is in a valid range for node addresses, it is our address
	if (reading > 0 && reading <= MAX_ID_VALUE) {
		nodeId = reading;

		DEBUG("Node identifier is %c", nodeId + 64);

		// Starts the transciever
		radio.begin();
		// Enables auto ack
		radio.setAutoAck(true);
		// Enables payload in ack packet
		radio.enableAckPayload();

		// Sets 15 retries, each every 0.5ms, useful with ack payload
		radio.setRetries(1, 15);
		// Sets the payload size to 2 bytes
		radio.setPayloadSize(2);
		// Sets fastest data rate, useful with ack payload
		radio.setDataRate(RF24_2MBPS);

		// Opens the first input pipe on this node address
		radio.openReadingPipe(1, ADDR_FAMILY + nodeId);

#if (SERIAL_DEBUG)
		// Sends current configuration on serial
		radio.printDetails();
#endif
	} else {
		DEBUG("Invalid node id %u found: use S## with a value between 1 and %u to configure the board", reading, MAX_ID_VALUE);
	}
}

// Button debouncing macros
#define DEBOUNCE 15
#define DMASK ((uint16_t)(1<<DEBOUNCE)-1)
#define DF (1<<(uint16_t)(DEBOUNCE-1))

// Macro for detection of falling edge and debouncing
#define DFE(signal, state) ((state=((state<<1)|(signal&1))&DMASK)==DF)

// Button debouncing status store
unsigned int buttonStatus;

void loop() {
	// Checks if we are trying to configure the node identifier
	config();

	delay(1);
	// Checks the push button
	if (DFE(digitalRead(BUTTON_PIN), buttonStatus)) {

		// Sets the destination address for packets to the hub address
		radio.openWritingPipe(ADDR_FAMILY);

		// Put transceiver in transmit mode
		radio.stopListening();

		// If we are here the button has been pressed: send our id to notify the event
		bool write = radio.write(&nodeId, 1);
		DEBUG("Send attempt from node %c was %s", nodeId + 64, write ? "successful" : "UNSUCCESSFUL");

		if (write) {
			// Get acknowledge packet payload from the hub
			while (radio.available()) {
				unsigned int count;
				radio.read(&count, 2);
				DEBUG("Got response from hub: total click count is %u", count);
			}

			receiveCount();
		}
	}
}

#define TIMEOUT 200
void receiveCount() {

	unsigned long time = millis();
	bool timeout = false;
	// Put transceiver in receive mode
	radio.startListening();

	while (!timeout && !radio.available()) {
		timeout = millis() - time > TIMEOUT;
	}

	if (timeout) {
		DEBUG("No hub response packet received within %u ms", TIMEOUT);
	} else {
		unsigned int count = 0;
		radio.read(&count, 2);
		DEBUG("Hub response packet received with value %u", count);
	}
}

/*
 * This function configures the node identifier reading from serial console.
 * Send the `s` character followed by a number between 1 and MAX_ID_VALUE to set
 * this node unique identifier and store it into EEPROM.
 */
void config() {
	if (Serial.available() > 0) {
		DEBUG("available %u", Serial.available());
		int c = Serial.read();
		if (c == 's' || c == 'S') {
			char* buffer = new char[4];
			uint8_t i = 0;
			delay(100); // let the serial buffer fill up
			while(Serial.available() && i < 3) {
				c = Serial.read();
				if (!isdigit(c))
					break;
				buffer[i++] = c;
			}
			buffer[i++] = '\0';
			uint16_t reading = atoi(buffer);
			if (reading > 0 && reading <= MAX_ID_VALUE) {
				nodeId = (uint8_t) reading;
				DEBUG("Setting node identifier to %c and resetting the transceiver", nodeId + 64);
				EEPROM.write(EEPROM_ADDR, nodeId);
				setup();
			} else {
				DEBUG("Invalid node identifier %u (must be greater than 0 and lesser than %u)", reading, MAX_ID_VALUE + 1);
			}
		}
	}
}
