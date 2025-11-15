#include "ArduinoLowPower.h"
#include "Seeed_Arduino_FreeRTOS.h"

/*
**********************
*** BUTTON MAPPING ***
***    A0 = 14     ***
***    A1 = 15     ***
***    A2 = 16     ***
***    A3 = 17     ***
***    A4 = 18     ***
***    A5 = 19     ***
**********************
*/

#ifndef GPIO_H
#define GPIO_H

#define TRIGGER_PIN (14)
#define DEBOUNCE_DELAY (50) //milliseconds
#define RFM95_IRQ_PIN (7) //Need to attach a wake from interrupt to this pin in case we get an override start request

extern TaskHandle_t override_task_handle;

//Function declarations
void trigger_ISR();
void wakeup_callback();

void GPIO_init(){    

  pinMode(TRIGGER_PIN, INPUT_PULLDOWN);
  
  attachInterrupt(TRIGGER_PIN, trigger_ISR, HIGH);

  // LowPower.attachInterruptWakeup(TRIGGER_PIN, NULL, CHANGE); 
  // LowPower.attachInterruptWakeup(RFM95_IRQ_PIN, NULL, CHANGE); //Lo-Ra radio IRQ pin

}

void trigger_ISR() {
  vTaskResume(override_task_handle);
}

//Name: validate_input
//Purpose: Performs debounce. If the input is still valid after the debounce time, the input is considered valid.
//Inputs: pin (an unsigned integer containing the pin # of the input being evaluated)
//Return: input_valid (a boolean that is true if the button input was evaluated as valid)
bool inline validate_input(uint32_t pin){
  delay(DEBOUNCE_DELAY);
  bool input_valid = digitalRead(pin);
  return input_valid;
}

//Name: wakeup_callback
//Purpose: Resume the RTOS scheduler
//Inputs: None
//Outputs: None
void wakeup_callback(){
  // xTaskResumeAll();
}

#endif