#ifndef GPIO_H
#define GPIO_H

#include "WInterrupts.h"
#include <Seeed_Arduino_FreeRTOS.h>
#include "ArduinoLowPower.h"


/*
****************************
****** BUTTON MAPPING ******
***    A0 = 14 = PA02    ***
***    A1 = 15 = PB08    ***
***    A2 = 16 = PB09    ***
***    A3 = 17 = PA04    ***
***    A4 = 18 = PA05    ***
***    A5 = 19 = PB02    ***
****************************

WARNING: A2 is interfering with the RH95 interrupts.

*/

#define GREEN_BUTTON_PIN (17)
// #define GREEN_BUTTON_PIN (17)
#define GREEN_PULSE_BUTTON_PIN (18) //was 15 just testing
#define RED_BUTTON_PIN (14)
#define OVERRIDE_BUTTON_PIN (15)
// #define OVERRIDE_BUTTON_PIN (15)
#define DEBOUNCE_DELAY (50) //milliseconds
#define TESTING_PIN (19)
#define RFM95_IRQ_PIN (7) //Need to attach a wake from interrupt to this pin in case we get an override start request

//FreeRTOS button task handle
extern TaskHandle_t button_task_handle;

//ISR Flags
volatile bool green_flag;
volatile bool green_pulse_flag;
volatile bool red_flag;
volatile bool override_flag;

//Function declarations
void green_button_ISR();
void green_pulse_button_ISR();
void red_button_ISR();
void override_button_ISR();

//Name: GPIO_init
//Purpose: Sets pin modes and attaches interrupts
//Inputs: None
//Outputs: None
void GPIO_init(){ 

  pinMode(A7, INPUT); //A7 (Analog pin 7) maps to the D9 pin which reads the battery voltage
  pinMode(uint32_t(GREEN_BUTTON_PIN), INPUT_PULLUP);
  pinMode(uint32_t(RED_BUTTON_PIN), INPUT_PULLUP);
  pinMode(uint32_t(GREEN_PULSE_BUTTON_PIN), INPUT_PULLUP);
  pinMode(uint32_t(OVERRIDE_BUTTON_PIN), INPUT_PULLUP);
  pinMode(uint32_t(TESTING_PIN), OUTPUT);

  green_flag = 0;
  green_pulse_flag = 0;
  red_flag = 0;
  override_flag = 0;


  attachInterrupt(uint32_t(GREEN_BUTTON_PIN), green_button_ISR, FALLING);
  // attachInterrupt(uint32_t(GREEN_PULSE_BUTTON_PIN), green_pulse_button_ISR, FALLING);
  attachInterrupt(uint32_t(RED_BUTTON_PIN), red_button_ISR, FALLING);
  attachInterrupt(uint32_t(OVERRIDE_BUTTON_PIN), override_button_ISR, FALLING);

  //Attaching interrupt wakeups allows these pins to wake up the uC from deepsleep
  LowPower.attachInterruptWakeup(GREEN_BUTTON_PIN, NULL, CHANGE);
  LowPower.attachInterruptWakeup(GREEN_PULSE_BUTTON_PIN, NULL, CHANGE);
  LowPower.attachInterruptWakeup(RED_BUTTON_PIN, NULL, CHANGE);
  LowPower.attachInterruptWakeup(OVERRIDE_BUTTON_PIN, NULL, CHANGE);
  LowPower.attachInterruptWakeup(RFM95_IRQ_PIN, NULL, CHANGE); //Lo-Ra radio IRQ pin
}

//Name: green_button_ISR
//Purpose: Sets state flags
//Inputs: None
//Outputs: None
void green_button_ISR() {
  green_flag = 1;
  vTaskResume(button_task_handle);
}

//Name: green_pulse_button_ISR
//Purpose: Sets state flags
//Inputs: None
//Outputs: None
void green_pulse_button_ISR() {
  green_pulse_flag = 1;
  vTaskResume(button_task_handle);
}

//Name: red_button_ISR
//Purpose: Sets state flags
//Inputs: None
//Outputs: None
void red_button_ISR() {
  red_flag = 1;
  vTaskResume(button_task_handle);
}

//Name: override_button_ISR
//Purpose: Sets state flags
//Inputs: None
//Outputs: None
void override_button_ISR() {
  override_flag = 1;
  vTaskResume(button_task_handle);
}

//Name: validate_input
//Purpose: Performs debounce. If the input is still valid after the debounce time, the input is considered valid.
//Inputs: pin (an unsigned integer containing the pin # of the input being evaluated)
//Return: input_valid (a boolean that is true if the button input was evaluated as valid)
bool inline validate_input(uint32_t pin){
  delay(DEBOUNCE_DELAY);
  bool input_valid = !digitalRead(pin);
  return input_valid;
}

#endif