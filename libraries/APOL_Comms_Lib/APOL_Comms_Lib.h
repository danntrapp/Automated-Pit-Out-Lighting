/*
  APOL_Comms_Lib.h - Library for Automated Pit Out Lighting (APOL) device communications.
  Created by Zachary Trager MacDonald, May 15, 2023.
*/


#ifndef APOL_Comms_Lib_h
#define APOL_Comms_Lib_h

#include <SPI.h>
#include <RH_RF95.h>
#include <Seeed_Arduino_FreeRTOS.h>

//M0 RF95 Pins
#define RFM95_CS (8) //???
#define RFM95_RST (4) //reset pin
#define RFM95_INT (3) //interrupt pin
#define RF95_FREQ (915.0) //MHz
#define PING_TIMEOUT (100) //How long the transmitter will wait to receive a response
#define NUM_REQUEST_TYPES (9)
#define NUM_SUBSYSTEMS (3)
#define NUM_FIELDS (7) //source, destination, request type, and 4 payload fields. 

enum request_type {PING, GREEN, GREEN_PULSE, RED, OVERRIDE_START, OVERRIDE_STOP, DETECTION, ACK, NONE, RESERVED}; //Putting in an additional request type stopped the compiler from "optimizing" some control structures.
enum subsystem {HHD, POL, VDD};

typedef struct packet_fields{
  subsystem sender_device;
  request_type request;
  subsystem target_device;
  uint32_t payload;
} packet_fields;

class APOL_Comms_Lib
{
	public:
		APOL_Comms_Lib(subsystem device_type, TaskHandle_t * rx_task_handle_ptr);
		void begin();
		void send_packet(request_type request, subsystem target_device, uint32_t payload);
		bool check_for_packet();
		bool check_for_any_packet();
		packet_fields packet_contents;
		static const constexpr char* const request_strings[] = {"PING", "GREEN", "GREEN_PULSE", "RED", "OVERRIDE_START", "OVERRIDE_STOP", "DETECTION", "ACK", "NONE"};
		static const constexpr char* const subsystem_strings[] = {"HHD", "POL", "VDD"};
		RH_RF95 * rf95;
		enum subsystem _device_type;	
};

#endif