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

#define UP_BUTTON_PIN (15)
#define DOWN_BUTTON_PIN (14)
#define GREEN_LIGHT_PIN (17)
#define GREEN_LIGHT_PORT (PORTB)
#define GREEN_LIGHT_BIT (9)
#define RED_LIGHT_PIN (18)
#define RED_LIGHT_PORT (PORTB)
#define RED_LIGHT_BIT (2)
#define DEBOUNCE_DELAY (5) //milliseconds
#define RFM95_IRQ_PIN (7) //Need to attach a wake from interrupt to this pin in case we get an override start request

//ISR Flags
volatile bool up_button_flag;
volatile bool down_button_flag;

extern TaskHandle_t button_task_handle;

//Function declarations
void up_button_ISR();
void down_button_ISR();

//Name: GPIO_init
//Purpose: Sets pin modes and attaches interrupts
//Inputs: None
//Outputs: None
void GPIO_init(){ 

  pinMode(DOWN_BUTTON_PIN, INPUT_PULLUP);
  pinMode(UP_BUTTON_PIN, INPUT_PULLUP);
  pinMode(GREEN_LIGHT_PIN, OUTPUT);
  pinMode(RED_LIGHT_PIN, OUTPUT);
  // pinMode(LED, OUTPUT);

  up_button_flag = 0;
  down_button_flag = 0;

  attachInterrupt(digitalPinToInterrupt(UP_BUTTON_PIN), up_button_ISR, FALLING);
  attachInterrupt(digitalPinToInterrupt(DOWN_BUTTON_PIN), down_button_ISR, FALLING);

  //Attaching interrupt wakeups allows these pins to wake up the uC from deepsleep
  LowPower.attachInterruptWakeup(UP_BUTTON_PIN, NULL, FALLING);
  LowPower.attachInterruptWakeup(DOWN_BUTTON_PIN, NULL, FALLING);
  LowPower.attachInterruptWakeup(RFM95_IRQ_PIN, NULL, CHANGE); //Lo-Ra radio IRQ pin
}

void toggle_LED(){

}

void up_button_ISR() {
  up_button_flag = 1;
  vTaskResume(button_task_handle);
}

void down_button_ISR() {
  down_button_flag = 1;
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