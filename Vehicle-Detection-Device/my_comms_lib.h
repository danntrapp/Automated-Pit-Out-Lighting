#include <SPI.h>
#include <RH_RF95.h>

//M0 RF95 Pins
#define RFM95_CS (8)
#define RFM95_RST (4)
#define RFM95_INT (3)

#define RF95_FREQ (915.0) //MHz

#define PING_TIMEOUT (500) //How long the transmitter will wait to receive a response

#define NUM_FIELDS (3)

enum request_type {PING, GREEN, GREEN_PULSE, RED, OVERRIDE_START, OVERRIDE_STOP, DETECTION, ACK};
enum subsystem {HHD, POL, VDD};

const enum subsystem device_type = VDD; 

typedef struct packet_fields{
  subsystem sender_device;
  request_type request;
  subsystem target_device;
} packet_fields;

packet_fields packet_contents;

int16_t packetnum = 0;  // packet counter, we increment per transmission
  
// Singleton instance of the radio driver
rf95(RFM95_CS, RFM95_INT);

void comms_init(){

  //RF Setup
  pinMode(RFM95_RST, OUTPUT);
  digitalWrite(RFM95_RST, HIGH);

  // manual reset
  digitalWrite(RFM95_RST, LOW);
  delay(10);
  digitalWrite(RFM95_RST, HIGH);
  delay(10);

  while (!rf95.init()) {
    Serial.println("LoRa radio init failed");
    Serial.println("Uncomment '#define SERIAL_DEBUG' in RH_RF95.cpp for detailed debug info");
    while (1);
  }
  Serial.println("LoRa radio init OK!");

  // Defaults after init are 434.0MHz, modulation GFSK_Rb250Fd250, +13dbM
  if (!rf95.setFrequency(RF95_FREQ)) {
    Serial.println("setFrequency failed");
    while (1);
  }
  Serial.print("Set Freq to: "); Serial.println(RF95_FREQ);
  
  // Defaults after init are 434.0MHz, 13dBm, Bw = 125 kHz, Cr = 4/5, Sf = 128chips/symbol, CRC on

  // The default transmitter power is 13dBm, using PA_BOOST.
  // If you are using RFM95/96/97/98 modules which uses the PA_BOOST transmitter pin, then 
  // you can set transmitter powers from 5 to 23 dBm:
  rf95.setTxPower(23, false);
}

void send_packet(request_type request, subsystem target_device){
  //Send ping and wait for response
  uint8_t radiopacket[NUM_FIELDS] = {device_type, request, target_device}; //sender device, request type, target device
  rf95.send((uint8_t *) radiopacket, NUM_FIELDS);
  rf95.waitPacketSent();

  if (request == PING) rf95.waitAvailableTimeout(PING_TIMEOUT);

}

_Bool check_for_packet(){
  if (rf95.available()){
    uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
    uint8_t len = sizeof(buf);

    if (rf95.recv(buf, &len)) {
      
      //Separate out data
      packet_contents.sender_device = (subsystem) (buf[0]);
      packet_contents.request = (request_type) (buf[1]);
      packet_contents.target_device = (subsystem) (buf[2]);

      // Serial.printf("Expecting: %d %d %d Got: %d %d %d\n", HHD, PING, POL, packet_contents.sender_device, packet_contents.request, packet_contents.target_device);

      if (packet_contents.target_device == device_type){ 
        // Serial.printf("Received %s from %s\n", request_strings[packet_contents.request], subsystem_strings[packet_contents.sender_device]);
        return 1;
      }

    }
  }
  return 0;
}

