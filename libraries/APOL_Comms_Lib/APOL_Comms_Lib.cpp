#include <APOL_Comms_lib.h>

constexpr const char* const APOL_Comms_Lib::request_strings[];
constexpr const char* const APOL_Comms_Lib::subsystem_strings[];

APOL_Comms_Lib::APOL_Comms_Lib(subsystem device_type, TaskHandle_t * rx_task_handle_ptr)
{
	_device_type = device_type;
	rf95 = new RH_RF95(RFM95_CS, RFM95_INT, rx_task_handle_ptr);
}

void APOL_Comms_Lib::begin()
{
		
	//packetnum = 0;  // packet counter, we increment per transmission
	
	// Singleton instance of the radio driver
	
	
	pinMode(RFM95_RST, OUTPUT);
	
	// Manual Reset
	digitalWrite(RFM95_RST, HIGH);
	delay(10);
	digitalWrite(RFM95_RST, LOW);
	delay(10);
	digitalWrite(RFM95_RST, HIGH);
	delay(10);

	//Initialize RFM95
	while (!rf95 -> init()) {
		#ifdef DEBUG
			Serial.println("LoRa radio init failed");
		#endif
	while (1);
	}
		#ifdef DEBUG
			Serial.println("LoRa radio init OK!");
		#endif
	// Defaults after init are 434.0MHz, modulation GFSK_Rb250Fd250, +13dbM
	if (!rf95 -> setFrequency(RF95_FREQ)) {
		#ifdef DEBUG
			Serial.println("setFrequency failed");
		#endif
	while (1);
	}
	
}

void APOL_Comms_Lib::send_packet(request_type request, subsystem target_device, uint32_t payload)
{
  //Send ping and wait for response
  uint8_t radiopacket[NUM_FIELDS] = {_device_type, request, target_device, uint8_t(payload), uint8_t(payload >> 8), uint8_t(payload >> 16), uint8_t(payload >> 24)}; //sender device, request type, target device
  rf95 -> send(radiopacket, NUM_FIELDS);
  rf95 -> waitPacketSent();

}

_Bool APOL_Comms_Lib::check_for_packet()
{
	if (rf95 -> available()){
		uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
		uint8_t len = sizeof(buf);
		if (rf95 -> recv(buf, &len)) {
		  
			//Separate out data
			packet_contents.sender_device = (subsystem) (buf[0]);
			packet_contents.request = (request_type) (buf[1]);
			packet_contents.target_device = (subsystem) (buf[2]);
			packet_contents.payload = buf[3] | (buf[4] << 8) | (buf[5] << 16) | (buf[6] << 24); 
		
		if (packet_contents.target_device == _device_type){ 
			return 1;
		 }

		}
	}
	return 0;
}

_Bool APOL_Comms_Lib::check_for_any_packet()
{
	if (rf95 -> available()){
		uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
		uint8_t len = sizeof(buf);
		if (rf95 -> recv(buf, &len)) {
		  
			//Separate out data
			packet_contents.sender_device = (subsystem) (buf[0]);
			packet_contents.request = (request_type) (buf[1]);
			packet_contents.target_device = (subsystem) (buf[2]);
			packet_contents.payload = buf[3] | (buf[4] << 8) | (buf[5] << 16) | (buf[6] << 24); 
		
			return 1;

		}
	}
	return 0;
}