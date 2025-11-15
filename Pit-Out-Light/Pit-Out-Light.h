//Macros for per-processor directives
// #define NO_SCREEN //define if prototyping a device without a screen
#define DEBUG //define to enable serial print statements
#define BUTTONS_CONNECTED //define to enable GPIO interrupts
#define RF_ENABLED
// #define IDLE_ENABLED
// #define UART //if defined, serial communications are through UART pins rather than USB emulation
//#define TASK_LOGGING //define to enable task entry and exit logging
#define LIGHTS_CONNECTED

#define NO_PAYLOAD 0

//Macros for system parameters
#define DURATION_MAX 24
#define DURATION_MIN 1
#define DURATION_INC 5
#define BAUD_RATE 115200
#define PULSE_DELAY 1000
#define IDLE_START_MILLISECONDS 5000 //30 Seconds

#include <APOL_Comms_Lib.h>
#include <Seeed_Arduino_FreeRTOS.h>
#include "display.h"
#include "GPIO.h"
#include "terminal.h"
#include "ArduinoLowPower.h"

extern RH_RF95 rf95;
extern volatile bool up_button_flag;
extern volatile bool down_button_flag;
extern bool trigger_flag;

typedef struct {
  uint32_t active_light;
  uint32_t requested_light;
  char requested_mode;
  uint8_t current_state;
  uint8_t requested_state;
  bool pulse_active;
  bool handle_secondary_request; //in the event that another request is interrupting the current light control request, do not suspend the light control task.
} light_control_t;