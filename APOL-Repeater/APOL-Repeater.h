#ifndef HHD_H
#define HHD_H

//Macros for per-processor directives
// #define NO_SCREEN //define if prototyping a device without a screen
#define DEBUG //define to enable serial print statements
#define BUTTONS_CONNECTED //define to enable GPIO interrupts
#define RF_ENABLED
// #define UART //if defined, serial communications are through UART pins rather than USB emulation
// #define TASK_LOGGING
// #define TEST_PLAN_5

#include <APOL_Comms_Lib.h>
#include <Seeed_Arduino_FreeRTOS.h>
#include "ArduinoLowPower.h"
#include "terminal.h"
#include "soc.h"

//Macros for system parameters
#define NO_PAYLOAD 0
#define MAX_QUEUED_REQUESTS 10
#define REFRESH_DELAY 100
#define DURATION_MAX 10
#define DURATION_MIN 0
#define BAUD_RATE 115200 //For serial
#define LOOP_DELAY 100
#define MAX_TRANSMIT_ATTEMPTS 5
#define SOC_MONITORING_DELAY 1000
#define IDLE_START_MILLISECONDS 30000 //30 Seconds

extern RH_RF95 rf95;
extern volatile bool up_button_flag;
extern volatile bool down_button_flag;

typedef struct {
  unsigned long end_time; //converting seconds to milliseconds
  uint32_t time_left;
  int len;
} override_t;

typedef struct {
  bool green_state; //tracks the state of the green light
  bool red_state; //tracks the state of the red light
} light_state_t;

typedef struct {
  request_type current_request;
  request_type ack_context;
  uint32_t current_payload;
  bool ack_flag;
} request_handler_t; 

#endif