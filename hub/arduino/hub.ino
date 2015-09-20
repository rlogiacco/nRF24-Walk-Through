#include <SPI.h>
#include "nRF24L01.h"
#include "RF24.h"
#include "printf.h"

// Enables/disables serial debugging
#define SERIAL_DEBUG true
#include <FormattingSerialDebug.h>

// Creates a new instance using SPI plus pins 9 and 10
RF24 radio(9, 10);

// nRF24 address family: all addresses will be in the format 0xFACEC0DE## with the last two
// bytes determined by the node identifier.
#define ADDR_FAMILY 0xFACEC0DE00LL
#define MAX_ID_VALUE 26

void setup() {
	SERIAL_DEBUG_SETUP(57600);

	// Initialize the transceiver
	radio.begin();
	radio.setAutoAck(true);        // Enables auto ack: this is true by default, but better to be explicit
	radio.enableAckPayload();      // Enables payload in ack packets
	radio.setRetries(1, 15);       // Sets 15 retries, each every 0.5ms, useful with ack payload
	radio.setPayloadSize(2);       // Sets the payload size to 2 bytes
	radio.setDataRate(RF24_2MBPS); // Sets fastest data rate, useful with ack payload

	// Opens the first input pipe on the hub address, this allows
	// the hub to receive packets transmitted to the hub address
	radio.openReadingPipe(1, ADDR_FAMILY);

	// Puts the transceiver into receive mode
	radio.startListening();

#if (SERIAL_DEBUG)
	// Prints current configuration on serial
	radio.printDetails();
#endif

	reset();
}

#define SECOND 1000
#define INTERVAL 30

// Holds the last time we have reset the counters
unsigned long last;

// Holds each node click counter
unsigned int counters[MAX_ID_VALUE];

// Holds the next ack packet payload, corresponding to total click count + 1
unsigned int next;

void loop() {

	// Checks if the transceiver has received any packet: when this
	// statement returns true the ack packet payload has been
	// already sent out
	if (radio.available()) {

		// This will store the node id
		byte nodeId = 0;

		// Read the node id from the reading pipe
		radio.read(&nodeId, 1);

		// Update the node counter
		counters[nodeId] = counters[nodeId] + 1;

		// Send the node counter to the originating node showing how the
		// hub can reply back to the transmitting node
		sendNodeCount(nodeId);

		// Prepare the next ack packet payload
		radio.writeAckPayload(1, &(++next), 2);

		DEBUG("Got event from node %c, click count is %u", nodeId + 64, counters[nodeId]);
	}

	// Check if its time to print out the report
	if (last + (INTERVAL * SECOND) < millis()) {
		last = millis();
		reset();
	}
}

/**
 * This function is executed periodically to print out the report
 * and reset all the counters.
 */
void reset() {
	unsigned int total = next - 1;

	DEBUG("Received %u clicks in the past %i second(s)", total, INTERVAL);
	for (int i = 0; i < MAX_ID_VALUE; i++) {
		if (counters[i] > 0) {
			DEBUG("* %u click(s) from node %c", counters[i], i + 64);
		}

		// Reset the node counter
		counters[i] = 0;
	}

	// Clear the pending ack packet payload: without this
	// the last ack payload will remain in the ack packets queue.
	radio.stopListening();
	radio.startListening();

	// Reset the total click counter: next is always 1 click ahead
	next = 1;

	// Reset the ack packet payload
	radio.writeAckPayload(1, &next, 2);
}

void sendNodeCount(const byte nodeId) {
	// Put transceiver into transmit mode
	radio.stopListening();

	// Sets the destination address to the node address
	radio.openWritingPipe(ADDR_FAMILY + nodeId);

	// Allow the node to switch transceiver into receive mode
	// before initiating the transmission.
	delay(50);

	// Send node count back to originating node
	boolean write = radio.write(&counters[nodeId], 2);

	// Put transceiver back into receive mode
	radio.startListening();

	DEBUG("Sending of counter %u to node %c was %s", counters[nodeId], 64 + nodeId, write ? "successful" : "UNSUCCESSFUL");
}
