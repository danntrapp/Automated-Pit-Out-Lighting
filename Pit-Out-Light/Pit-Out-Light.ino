#include "Pit-Out-Light.h"

//Taskhandles
TaskHandle_t rx_task_handle;
TaskHandle_t button_task_handle;
TaskHandle_t display_task_handle;
TaskHandle_t light_control_task_handle;
TaskHandle_t terminal_task_handle;
TaskHandle_t power_management_task_handle;

//Mutexes
SemaphoreHandle_t uart_mutex;
SemaphoreHandle_t display_mutex;

//Global state variables
bool override_flag;
bool new_override;
bool display_update_flag = true; //Here because its safer to do this than suspend/resume tasks
int time_multiplier;

//Task parameters
light_control_t light_parameters;

typedef struct{
  uint32_t last_activity;
  bool idle;
} power_management_parameters_t;

power_management_parameters_t power_management_parameters;

//Comms stack
APOL_Comms_Lib comms(POL, &rx_task_handle);

#ifdef UART
  Uart & serial = Serial1;
#else
  Serial_ & serial = Serial;
#endif

//Name: setup
//Purpose: Initializes peripherals, creates tasks, and starts the FreeRTOS scheduler.
//Inputs: None
//Outputs: None
void setup() {

  noInterrupts();

  //Initialize peripherals
  #ifdef DEBUG
    uart_mutex = xSemaphoreCreateMutex(); //mutex for uart
    display_mutex = xSemaphoreCreateMutex();
    serial.begin(BAUD_RATE);
    xTaskCreate(terminal_task, // Task function
    "UART TERMINAL", // Task name
    256, // Stack size 
    NULL, 
    1, // Priority
    &terminal_task_handle); // Task handler
  #endif
  
  #ifndef NO_SCREEN
    display_init();
    xTaskCreate(display_task, // Task function
            "UPDATE DISPLAY", // Task name
            256, // Stack size 
            NULL, 
            3, // Priority
            &display_task_handle); // Task handler

  #endif

  #ifdef BUTTONS_CONNECTED
    GPIO_init(); //Stops peripherals from working depending on M0 controller used...  
    
    xTaskCreate(button_task, // Task function
            "BUTTON HANDLER", // Task name
            256, // Stack size 
            NULL, 
            4, // Priority
            &button_task_handle); // Task handler
  #endif

  #if defined(LIGHTS_CONNECTED) && defined(BUTTONS_CONNECTED)
      light_parameters.active_light = 0;
      light_parameters.handle_secondary_request = 0;
      xTaskCreate(light_control_task, // Task function
              "LIGHT TASK", // Task name
              256, // Stack size 
              NULL, 
              4, // Priority
              &light_control_task_handle); // Task handler
  #endif
                
  override_flag = 0;
  new_override = 0;
  time_multiplier = 3;

  #ifdef RF_ENABLED
    comms.begin();
    comms.rf95 -> setModeRx(); //Start in Rx Mode
    comms.rf95 -> setTxPower(20);

    xTaskCreate(rx_task, // Task function
              "RX HANDLER", // Task name
              256, // Stack size 
              NULL, 
              5, // Priority
              &rx_task_handle); // Task handler

  #endif

  interrupts();
  #ifdef IDLE_ENABLED
  xTaskCreate(power_management_task, // Task function
            "POWER MANAGEMENT", // Task name
            256, // Stack size 
            &power_management_parameters, 
            3, // Priority
            &power_management_task_handle); // Task handler
  #endif

  //Start tasks
  vTaskStartScheduler();

}

//Name: loop
//Purpose: Does nothing (required by Arduino)
//Inputs: None
//Outputs: None
void loop() { 
  // Do nothing.
}


//Name: rx_task
//Purpose: Handles LoRa APOL Comms packets as they're received (unsuspended from RFM95 RX interrupt).
//Inputs: None
//Outputs: None
void rx_task(void *pvParameters) {
  while(1){
    
    vTaskSuspend(rx_task_handle);
    
    #ifdef DEBUG
      xSemaphoreTake(uart_mutex, portMAX_DELAY);
    #endif
    
    #if defined(DEBUG) && defined(TASK_LOGGING)
      format_terminal_for_new_entry();
      serial.print("Rx Task Entered\n");
      format_new_terminal_entry();
    #endif
    
    
    if(comms.check_for_packet() || (trigger_flag == 1)){
      
      #ifdef DEBUG
        if (trigger_flag == 1) trigger_flag = 0;
      #endif

      switch(comms.packet_contents.request){
        case GREEN:{
          
          #ifdef DEBUG
            format_terminal_for_new_entry();
            serial.print("Green Request Received\n");
            format_new_terminal_entry();
          #endif
          if (light_parameters.pulse_active == 1){
            light_parameters.pulse_active = 0;
          }
          light_parameters.requested_light = GREEN_LIGHT_PIN;
          light_parameters.requested_state = comms.packet_contents.payload;
          light_parameters.requested_mode = GREEN; //continuous
          vTaskResume(light_control_task_handle);
        } break;
        case GREEN_PULSE: {
          #ifdef DEBUG
            format_terminal_for_new_entry();
            serial.print("Green Pulse Request Received\n");
            format_new_terminal_entry();
          #endif
          if (light_parameters.pulse_active == 1){
            light_parameters.pulse_active = 0;
          }
          light_parameters.requested_light = GREEN_LIGHT_PIN;
          light_parameters.requested_mode = GREEN_PULSE; //pulsed
          vTaskResume(light_control_task_handle);
        } break;
        case OVERRIDE_START: {
          #ifdef DEBUG
            format_terminal_for_new_entry();
            serial.print("Override Start Request received\n");
            format_new_terminal_entry();
          #endif
          comms.send_packet(ACK, VDD, OVERRIDE_START); 
          comms.send_packet(OVERRIDE_START, HHD, time_multiplier * DURATION_INC);
          if (light_parameters.pulse_active == 1){
            light_parameters.pulse_active = 0;
          }
          light_parameters.requested_mode = OVERRIDE_START; //override
          new_override = 1;
          override_flag = 1;
          comms.rf95 -> setModeRx();
          vTaskResume(light_control_task_handle);
        } break;
        case OVERRIDE_STOP: {
          #ifdef DEBUG
            format_terminal_for_new_entry();
            serial.print("Override Stop Request received\n");
            format_new_terminal_entry();
          #endif
          //comms.send_packet(OVERRIDE_STOP, HHD, NO_PAYLOAD);
          light_parameters.requested_light = light_parameters.active_light;
          light_parameters.requested_mode = GREEN; //pulsed
          override_flag = 0;
          vTaskResume(light_control_task_handle);
        } break;
        case PING: {
          #ifdef DEBUG
            format_terminal_for_new_entry();
            serial.print("Ping received\n");
            format_new_terminal_entry();
          #endif
        } break;
        case RED: {
          #ifdef DEBUG
            format_terminal_for_new_entry();
            serial.print("Red request received\n");
            format_new_terminal_entry();
          #endif
          if (light_parameters.pulse_active == 1){
            light_parameters.pulse_active = 0;
          }
          light_parameters.requested_light = RED_LIGHT_PIN;
          light_parameters.requested_state = comms.packet_contents.payload;
          light_parameters.requested_mode = RED; 
          vTaskResume(light_control_task_handle);
        } break;
      }
      comms.send_packet(ACK, comms.packet_contents.sender_device, comms.packet_contents.request); //send ACK back
    } 

    comms.rf95 -> setModeRx(); //Put back into Rx mode after responding
    
    #ifdef IDLE_ENABLED
      power_management_parameters.last_activity = millis();
    #endif

    #if defined(DEBUG) && defined(TASK_LOGGING)
      format_terminal_for_new_entry();
      serial.print("Rx Task Exited\n");
      format_new_terminal_entry();
    #endif

    #ifdef DEBUG
      xSemaphoreGive(uart_mutex);
    #endif

  }

}

//Name: button_task
//Purpose: FreeRTOS task that handles button inputs (unsuspended by GPIO ISRs).
//Inputs: None
//Outputs: None
void button_task(void *pvParameters) {
  
  while(1){
    //If all tasks have been handled, suspend
    if ((up_button_flag == 0) && (down_button_flag == 0)) vTaskSuspend(button_task_handle);
    
    #if defined(DEBUG) && defined(TASK_LOGGING)
      xSemaphoreTake(uart_mutex, portMAX_DELAY);
      format_terminal_for_new_entry();
      serial.print("Button Task Entered\n");
      format_new_terminal_entry();
      xSemaphoreGive(uart_mutex);
    #endif

    if (up_button_flag && validate_input(UP_BUTTON_PIN)) {
      time_multiplier += (time_multiplier < DURATION_MAX); //add 1 if the multiplier is not at its maximum 
      #ifdef IDLE_ENABLED
        power_management_parameters.last_activity = millis();
      #endif
      up_button_flag = 0;
    }

    else if (down_button_flag && validate_input(DOWN_BUTTON_PIN)) {
      time_multiplier -= (time_multiplier > DURATION_MIN); //subtract 1 if the multiplier is not at its minimum 
      #ifdef IDLE_ENABLED
        power_management_parameters.last_activity = millis();
      #endif
      down_button_flag = 0;
    }

    else {
      up_button_flag = 0, down_button_flag = 0;
    }

    #if defined(DEBUG) && defined(TASK_LOGGING)
      xSemaphoreTake(uart_mutex, portMAX_DELAY);
      format_terminal_for_new_entry();
      serial.print("Button Task Exited\n");
      format_new_terminal_entry();
      xSemaphoreGive(uart_mutex);
    #endif

  }

}

//Name: display_taks
//Purpose: Periodically updates the display.
//Inputs: None
//Outputs: None
void display_task(void *pvParameters) {

  while(1){
  
    vTaskDelay(100); //~60 Hz refresh rate

    #if defined(DEBUG) && defined(TASK_LOGGING)
      xSemaphoreTake(uart_mutex, portMAX_DELAY);
      format_terminal_for_new_entry();
      serial.print("Display Task Entered\n");
      format_new_terminal_entry();
      xSemaphoreGive(uart_mutex);
    #endif
    
    if (display_update_flag){
      xSemaphoreTake(display_mutex, portMAX_DELAY);
      update_GUI(time_multiplier * DURATION_INC);
      xSemaphoreGive(display_mutex);
    }
    
    #if defined(DEBUG) && defined(TASK_LOGGING)
      xSemaphoreTake(uart_mutex, portMAX_DELAY);
      format_terminal_for_new_entry();
      serial.print("Display Task Exited\n");
      format_new_terminal_entry();
      xSemaphoreGive(uart_mutex);
    #endif

  }

}


//Name: light_control_task
//Purpose: In charge of controlling which lights are on and how long they're on for (unsuspended from RX task)
//Inputs: None
//Outputs: None
void light_control_task(void *pvParameters) {
  
  while(1){
    
    if (light_parameters.handle_secondary_request == 0) vTaskSuspend(light_control_task_handle);
    else light_parameters.handle_secondary_request = 0;

    #ifdef DEBUG
      xSemaphoreTake(uart_mutex, portMAX_DELAY);
    #endif

    #if defined(DEBUG) && defined(TASK_LOGGING)
      format_terminal_for_new_entry();
      serial.print("Light Task Entered\n");
      format_new_terminal_entry();
    #endif
    
    #ifdef DEBUG
      format_terminal_for_new_entry();
      serial.print("Attempting to turn on light...\n");
      format_new_terminal_entry();
      xSemaphoreGive(uart_mutex);
    #endif

    //Turn off currently active light (if applicable)
    if (light_parameters.active_light != 0) digitalWrite(light_parameters.active_light, LOW);
    

    switch(light_parameters.requested_mode){

      case GREEN:
      case RED: {
        digitalWrite(light_parameters.requested_light,  light_parameters.requested_state);
        light_parameters.active_light = light_parameters.requested_light;
        break;
      }

      case OVERRIDE_START: {
        new_override = 0;
        #ifdef IDLE_ENABLED
          vTaskSuspend(power_management_task_handle);
        #endif
        display_update_flag = true;
        uint override_end_time = millis() + (time_multiplier * DURATION_INC) * 1000;
        digitalWrite(RED_LIGHT_PIN, HIGH);
        do {

          vTaskDelay(10);
          if (new_override == 1) {
            override_end_time = millis() + (time_multiplier * DURATION_INC) * 1000;
            new_override = 0;
          }
        } while ((override_flag == 1) && (millis() < override_end_time)); 
        digitalWrite(RED_LIGHT_PIN, LOW);
        override_flag = 0;
        #ifdef IDLE_ENABLED
        power_management_parameters.last_activity = millis();
        vTaskResume(power_management_task_handle);
        #endif
        break;
      }

      case GREEN_PULSE: {
        uint pulse_end_time = millis() + PULSE_DELAY; 
        light_parameters.pulse_active = 1; 
        digitalWrite(GREEN_LIGHT_PIN, HIGH); 
        do {

          vTaskDelay(10);
        
        } while ( (millis() < pulse_end_time) && (light_parameters.pulse_active == 1));
        if (light_parameters.pulse_active == 1 || light_parameters.requested_mode == RED)
        digitalWrite(GREEN_LIGHT_PIN,  LOW);
        if (light_parameters.pulse_active == 1){ 
          comms.send_packet(GREEN_PULSE, HHD, NO_PAYLOAD);
          comms.rf95 -> setModeRx();
        }
        //If the pulse was externally interrupted, do not suspend the task and handle the new request.
        if (light_parameters.pulse_active == 1){
          light_parameters.pulse_active = 0;
        } 
        else {
          light_parameters.handle_secondary_request = 1;
        }
        break;
      }
    }

    #if defined(DEBUG) && defined(TASK_LOGGING)
      xSemaphoreTake(uart_mutex, portMAX_DELAY);
      format_terminal_for_new_entry();
      serial.print("Light Task Exited\n");
      format_new_terminal_entry();
      xSemaphoreGive(uart_mutex);
    #endif
  }

}


//Name: power_management_task
//Purpose: Monitors when device is being used and when it isn't. When device not in use, turn off display and put system into deepsleep.
//Inputs: None
//Outputs: None
void power_management_task(void *pvParameters) {
  
  power_management_parameters_t * power_management_parameters = (power_management_parameters_t *) pvParameters;
  power_management_parameters -> idle = 0;
  power_management_parameters -> last_activity = millis();
  
  while(1){

    vTaskDelay(100); 

    #if defined(DEBUG) && defined(TASK_LOGGING)
      xSemaphoreTake(uart_mutex, portMAX_DELAY);
      format_terminal_for_new_entry();
      serial.print("Power Monitoring Task Entered\n");
      format_new_terminal_entry();
      xSemaphoreGive(uart_mutex);
    #endif

    //Check to see if there has been activity within the last N seconds. 
    //If there has been, either do nothing or take out of idle state.
    //If there hasn't been, go into idle mode. 
    volatile uint32_t current_time = millis(); 
    if((power_management_parameters -> idle == false) && (current_time > (power_management_parameters -> last_activity + IDLE_START_MILLISECONDS)) ){
      
      xSemaphoreTake(uart_mutex, portMAX_DELAY);

      //Stop display from updating
      #ifndef NO_SCREEN
        display_update_flag = false;
        
        //Turn off display
        display.clearDisplay();
        xSemaphoreTake(display_mutex, portMAX_DELAY);
        display.display();
        xSemaphoreGive(display_mutex);
      #endif
      
      //State machine -> system goes into idle
      power_management_parameters -> idle = true;

      //Put uC into deep sleep
      LowPower.deepSleep();

      xSemaphoreGive(uart_mutex);
    }

    else if ( (power_management_parameters -> idle == true) && (current_time < (power_management_parameters -> last_activity + IDLE_START_MILLISECONDS))){

      //Start updating display again
      #ifndef NO_SCREEN
        display_update_flag = true;
      #endif

      //State machine -> system exits idle mode
      power_management_parameters -> idle = 0;
    }
    
    #if defined(DEBUG) && defined(TASK_LOGGING)
      
      xSemaphoreTake(uart_mutex, portMAX_DELAY);
      format_terminal_for_new_entry();
      serial.print("Power Monitoring Task Exited\n");
      format_new_terminal_entry();
      xSemaphoreGive(uart_mutex);
    #endif
  }
}

//Name: uart_terminal_task
//Purpose: Provide a debug terminal.
//Inputs: None
//Outputs: None
void terminal_task(void *pvParameters) {

  clear_buffer(input_buffer, sizeof(input_buffer));
  buffer_pos = 0;
  
  while(1){

    vTaskDelay(100);

    #ifdef DEBUG
      xSemaphoreTake(uart_mutex, portMAX_DELAY);
    #endif

    //Update Terminal Window
    //There are a bunch of ANSI escape character sequences I've been using to help with keeping a
    //persistent monitoring window at the bottom of the screen. Here are a few examples: 
    //\033[36m" is SGR for set terminal text to cyan (helps differentiate terminal text from other text)
    //The redundant return line characters seem to be necessary. I belive sometimes the character is dropped...
    //\033[E goes to the start of next line. 
    
    serial.print("\033[36m"); //Turn text cyan (PERSISTENT STATE VARIABLE MONITORING)
    serial.printf("\033[%dF", NUM_PERSISTENT_LINES ); //Escape sequence to put cursor up N lines and at the start of the line
    serial.printf("****************************\33[0K\033[E");
    serial.printf("*  Last Packet Paylod = %d  *\33[0K\033[E", comms.packet_contents.payload);
    // serial.printf("* Free RAM = %d bytes *\n", freeMemory());
    serial.printf("****************************\33[0K\033[E");
    serial.print("\033[35m"); //turn terminal text magenta (USER INPUT)
    serial.printf("%s\33[0K", input_buffer);

    while (serial.available() && (buffer_pos < MAX_BUFFER_SIZE)){ //get number of bits on buffer
      
      input_buffer[buffer_pos] = serial.read();
      
      //handle special chracters
      if (input_buffer[buffer_pos] == '\r') { //If an enter character is received
        input_buffer[buffer_pos] = '\0'; //Get rid of the carriage return
        serial.print("\033[33m"); //turn terminal text yellow (TERMINAL STATMENTS)
        handle_input(input_buffer); //handle the input
        clear_buffer(input_buffer, sizeof(input_buffer));
        buffer_pos = -1; //return the buffer back to zero (incrimented after this statement)
      }
      else if (input_buffer[buffer_pos] == 127){ //handle a backspace character
        serial.printf("%c", input_buffer[buffer_pos]); //print out backspace
        input_buffer[buffer_pos] = '\0'; //clear the backspace 
        if (buffer_pos > 0) input_buffer[--buffer_pos] = '\0'; //clear the previous buffer pos if there was another character in the buffer that wasn't a backspace
        buffer_pos--;
      }
      else serial.printf("%c", input_buffer[buffer_pos]);

      buffer_pos++;
      
    }

    serial.print("\033[0m"); //resets text color and other graphical options to default

    #ifdef DEBUG
      xSemaphoreGive(uart_mutex);
    #endif
  }
}