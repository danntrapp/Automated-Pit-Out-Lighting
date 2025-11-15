#include <APOL_Comms_Lib.h>
#include <Seeed_Arduino_FreeRTOS.h>
#include "GPIO.h"
#include "terminal.h"
#include "ArduinoLowPower.h"

#define DEBUG //define to enable serial print statements
#define RF_ENABLED
// #define TASK_LOGGING //define to enable task entry and exit logging

#define DURATION_MAX 10
#define DURATION_MIN 0
#define BAUD_RATE 115200
#define MAX_QUEUED_REQUESTS 5 //halved to try to try and fix loss of comms issue due to too many items in queue
#define MAX_TRANSMIT_ATTEMPTS 5
#define IDLE_START_MILLISECONDS 10000 //Stay active for 10 seconds

extern RH_RF95 rf95;
extern volatile _Bool up_button_flag;
extern volatile _Bool down_button_flag;

typedef struct {
  request_type current_request;
  request_type ack_context;
  uint32_t current_payload;
  bool ack_flag;
} request_handler_t; 