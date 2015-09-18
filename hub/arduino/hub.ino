#include <SPI.h>
#include "nRF24L01.h"
#include "RF24.h"
#include "printf.h"

// enables/disable serial debugging
#define SERIAL_DEBUG true
#include <FormattingSerialDebug.h>

// creates a new instance using SPI plus pins 9 and 10
RF24 radio(9, 10);

// nRF24 address family: all addresses will be in the format 0xFACEC0DE## with the last two
// bytes determined by the node identifier.
#define ADDR_FAMILY 0xFACEC0DE00LL
#define MAX_ID_VALUE 26

void setup() {
	SERIAL_DEBUG_SETUP(9600);

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

	// Opens the first input pipe on the hub address
	radio.openReadingPipe(1, ADDR_FAMILY);

	// Puts the transceiver into receive mode
	radio.startListening();

#if (SERIAL_DEBUG)
	// Sends current configuration on serial
	radio.printDetails();
#endif

	reset();
}

#define SECOND 1000
#define INTERVAL 10

// Holds the last time we have reset the counters
unsigned long last;

// Holds each node click counter
unsigned int counters[MAX_ID_VALUE];

// Holds the next ack packet payload, corresponding to total click count + 1
unsigned int next;

void loop() {
	// Checks if the transceiver has received any packet
	if (radio.available()) {

		// Prepare the next ack packet payload
		next++;

		// This will store the node id
		byte nodeId = 0;

		// Read the node id from the reading pipe
		radio.read(&nodeId, 1);

		// Update the node counter and total counter
		counters[nodeId] = counters[nodeId] + 1;

		DEBUG("Got event from node %c, click count is %u", nodeId + 64, counters[nodeId]);

		// prepare the next ack packet payload
		radio.writeAckPayload(1, &next, 2);
	}

	// Check if its time to print out the report
	if (last + (INTERVAL * SECOND) < millis()) {
		last = millis();
		reset();
	}
}

void reset() {
	DEBUG("Received %u clicks in the past %i second(s)", next - 1, INTERVAL);
	for (int i = 0; i < MAX_ID_VALUE; i++) {
		if (counters[i] > 0) {
			DEBUG("* %u click(s) from node %c", counters[i], i + 64);
		}

		// Reset the node counter
		counters[i] = 0;
	}

	// Reset the total click counter
	next = 1;

	// Clear the pending ack packet payload: without this
	// the last ack payload will remain in the ack packets queue.
	radio.stopListening();
	radio.startListening();

	// Prepare the next ack packet payload
	radio.writeAckPayload(1, &next, 2);
}
