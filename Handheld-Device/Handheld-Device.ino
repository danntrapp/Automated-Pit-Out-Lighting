#include "Handheld-Device.h"

//Taskhandles
TaskHandle_t ping_task_handle;
TaskHandle_t rx_task_handle;
TaskHandle_t button_task_handle;
TaskHandle_t display_task_handle;
TaskHandle_t override_task_handle;
TaskHandle_t request_task_handle;
TaskHandle_t terminal_task_handle;
TaskHandle_t soc_monitoring_task_handle;
TaskHandle_t power_management_task_handle;

QueueHandle_t request_queue;
QueueHandle_t payload_queue;

//Mutexes
SemaphoreHandle_t uart_mutex;
SemaphoreHandle_t display_mutex;

//Settings and state variables
bool testState;
bool is_override = false;
bool new_override = 0;
bool display_update_flag = true;

typedef struct{
  uint32_t last_activity;
  bool idle;
} power_management_parameters_t;

typedef struct{
  bool successful_ping;
  bool is_connected;
  uint8_t unsusccesful_attempts;
} ping_parameters_t;

override_t override_paramaters;
request_handler_t request_handler_parameters;
light_state_t light_parameters;
power_management_parameters_t power_management_parameters;
ping_parameters_t ping_parameters;
request_type request;
display_string_options center_string_select;
display_string_options status_string_select;
uint8_t battery_soc;
int override_delay;
int testVar;
double battery_voltage;
char display_string[10];
uint32_t start_time;
APOL_Comms_Lib comms(HHD, &rx_task_handle);

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
    serial.begin(BAUD_RATE);
    xTaskCreate(terminal_task, // Task function
    "UART TERMINAL", // Task name
    256, // Stack size 
    NULL, 
    1, // Priority
    &terminal_task_handle); // Task handler
    
  #endif

  #ifdef TEST_PLAN_5
  testState = true;
  testVar = 0;
  #endif

  #ifndef NO_SCREEN
    center_string_select = welcome;
    display_init();
    xTaskCreate(display_task, // Task function
          "UPDATE DISPLAY", // Task name
          256, // Stack size 
          NULL, 
          4, // Priority
          &display_task_handle); // Task handler
    display_mutex = xSemaphoreCreateMutex();
  #endif

  #ifdef BUTTONS_CONNECTED
    GPIO_init(); 
    xTaskCreate(button_task, // Task function
          "BUTTON HANDLER", // Task name
          256, // Stack size 
          &light_parameters, 
          4, // Priority
          &button_task_handle); // Task handler

  #endif


  #ifdef RF_ENABLED
    comms.begin();
    comms.rf95 -> setModeRx(); //Start in Rx Mode
    comms.rf95 -> setTxPower(13);
    
    xTaskCreate(ping_task, // Task function
              "PING", // Task name
              64, // Stack size 
              &ping_parameters, 
              4, // Priority
              &ping_task_handle); // Task handler

    xTaskCreate(rx_task, // Task function
              "RX HANDLER", // Task name
              256, // Stack size 
              &light_parameters,
              5, // Priority
              &rx_task_handle); // Task handler

    xTaskCreate(override_task, // Task function
              "OVERRIDE TASK", // Task name
              256, // Stack size 
              &override_paramaters, 
              5, // Priority
              &override_task_handle); // Task handler

    xTaskCreate(request_handler_task, // Task function
              "REQUEST HANDLER TASK", // Task name
              256, // Stack size 
              &request_handler_parameters, 
              4, // Priority
              &request_task_handle); // Task handler

    request_queue = xQueueCreate(MAX_QUEUED_REQUESTS, sizeof(request_type));
    payload_queue = xQueueCreate(MAX_QUEUED_REQUESTS, sizeof(uint32_t));

  #endif


  xTaskCreate(soc_monitoring_task, // Task function
            "BATTERY MONITORING", // Task name
            64, // Stack size 
            NULL, 
            2, // Priority
            &soc_monitoring_task_handle); // Task handler
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
  //Do nothing.
}


//Name: ping_task
//Purpose: FreeRTOS task that periodically transmits a ping to the POL.
//Inputs: None
//Outputs: None
void ping_task(void *pvParameters) {
  
  ping_parameters_t * ping_parameters = (ping_parameters_t *) pvParameters;
  ping_parameters -> unsusccesful_attempts = 3; //start not connected
  ping_parameters -> successful_ping = false;
  ping_parameters -> is_connected = 0;

  while(1){
    
    vTaskDelay(1000);
    
    #ifdef DEBUG
      xSemaphoreTake(uart_mutex, portMAX_DELAY);
    #endif

    #if defined(DEBUG) && defined(TASK_LOGGING)
      format_terminal_for_new_entry();
      serial.print("Ping Task Entered\n");
      format_new_terminal_entry();
    #endif

    if (ping_parameters -> successful_ping == true) {
      ping_parameters -> unsusccesful_attempts = 0;
    }
    else{
      ping_parameters -> unsusccesful_attempts++;
    }
    
    //We are not connected if there are more than 3 unsuccessful attempts
    ping_parameters -> is_connected = ping_parameters -> unsusccesful_attempts < 3;

    //
    ping_parameters -> successful_ping = false;
    comms.send_packet(PING, POL, NO_PAYLOAD);
    comms.rf95 -> setModeRx();
    
    #if defined(DEBUG) && defined(TASK_LOGGING)
      format_terminal_for_new_entry();
      serial.print("Ping Task Exited\n");
      format_new_terminal_entry();
    #endif  

    #ifdef DEBUG
      xSemaphoreGive(uart_mutex);
    #endif
  }
}

//Name: rx_task
//Purpose: FreeRTOS task that checks for Lo-Ra packets, and handles them (triggered by RFM95 interrupt).
//Inputs: None
//Outputs: None
void rx_task(void *pvParameters) {
  light_state_t * params = (light_state_t *) pvParameters;
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
    
    if(comms.check_for_packet() && comms.packet_contents.target_device == HHD){
      switch(comms.packet_contents.request){
        case OVERRIDE_START:{
        #if defined(DEBUG) 
        format_terminal_for_new_entry();
        serial.print("Override Start Received\n");
        format_new_terminal_entry();
        #endif
          override_delay = comms.packet_contents.payload;
          is_override = true;  
          new_override = 1;
          vTaskResume(override_task_handle);
        } break;
        case OVERRIDE_STOP:{
          #if defined(DEBUG) && defined(TASK_LOGGING)
          serial.println("Override stop received\n");
          #endif
          override_delay = 0;
          is_override = false;
          vTaskResume(override_task_handle);
        } break;
        case PING:{
          comms.send_packet(ACK, comms.packet_contents.sender_device, NO_PAYLOAD);
        } break;
        case GREEN_PULSE:{
          status_string_select = none;
        } break;
        case ACK:{
          #ifdef DEBUG
            format_terminal_for_new_entry();
            serial.printf("Ack received = %d & Ack context = %d. (for reference GREEN is %d).\n", comms.packet_contents.payload, request_handler_parameters.ack_context, GREEN); //green is 1, red is 3
            format_new_terminal_entry();
          #endif
          switch (comms.packet_contents.payload){
            case GREEN:{
              params-> green_state ? status_string_select = green : status_string_select = none;
            } break;
            case RED: {
              params-> red_state ? status_string_select = red : status_string_select = none;
            }break;
            case GREEN_PULSE: {
              status_string_select = green_pulse;
            }break;
          }
          if (comms.packet_contents.payload == PING) ping_parameters.is_connected = (comms.packet_contents.sender_device == POL);
          else if (comms.packet_contents.payload == request_handler_parameters.ack_context) request_handler_parameters.ack_flag = 0;

        } break;
      }
    } 

    //After responding, put back into RX mode
    comms.rf95 -> setModeRx();

    #if defined(DEBUG) && defined(TASK_LOGGING)
      format_terminal_for_new_entry();
      serial.print("RX Task Exited\n");
      format_new_terminal_entry();
    #endif
    #ifdef DEBUG
      xSemaphoreGive(uart_mutex);
    #endif
  }
}

//Name: request_handler_task
//Purpose: FreeRTOS task that sends requests from the queue to the POL (started whenever another task needs to send a request).
//Inputs: params (state variables for acknowledgement handling)
//Outputs: None
void request_handler_task(void *pvParameters) {
  request_handler_t * params = (request_handler_t *) pvParameters;
  
  while(1){
    vTaskSuspend(request_task_handle); 

    #ifdef DEBUG
      xSemaphoreTake(uart_mutex, portMAX_DELAY);
    #endif

    #if defined(DEBUG) && defined(TASK_LOGGING)
      format_terminal_for_new_entry();
      serial.print("Request Handler Entered\n");
      format_new_terminal_entry();
    #endif

    while (uxQueueMessagesWaiting(request_queue) > 0){
      xQueueReceive(request_queue, &(params -> current_request), 0);
      xQueueReceive(payload_queue, &(params -> current_payload), 0);

      //Set the context to match the current request
      //Set the ack flag and keep sending the request until an acknowledgement is received that matches the current context 
      params -> ack_context = params -> current_request;
      params -> ack_flag = 1;
      #ifdef DEBUG
            format_terminal_for_new_entry();
            serial.printf("Ack context was set to %d.\n\r", params -> current_request);
            serial.printf("Ack context is actually %d.\n", params -> ack_context);
            format_new_terminal_entry();
      #endif
      
      int attempts = 0;
      
      do {
          #ifdef DEBUG
            format_terminal_for_new_entry();
            serial.print("Trying to send a new packet.\n");
            format_new_terminal_entry();
            xSemaphoreGive(uart_mutex);
          #endif
          comms.send_packet(params -> current_request, POL, params -> current_payload);
          comms.rf95 -> setModeRx();
          vTaskDelay(50);
          attempts++;
          xSemaphoreTake(uart_mutex, portMAX_DELAY);
      } while (params -> ack_flag && attempts < MAX_TRANSMIT_ATTEMPTS);

      if (attempts == MAX_TRANSMIT_ATTEMPTS) center_string_select = transmit_failed;

    }
    
    params -> ack_context = NONE;
    comms.rf95 -> setModeRx();
    #if defined(DEBUG) && defined(TASK_LOGGING)
      format_terminal_for_new_entry();
      serial.print("Request Handler Exited\n");
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
  
  light_state_t * params = (light_state_t *) pvParameters;

  request_type new_request;
  uint32_t new_request_payload;

  while(1){

    //If all button presses have been handled, suspend.
    if ((green_flag == 0) && (green_pulse_flag == 0) && (red_flag == 0) && (override_flag == 0)) vTaskSuspend(button_task_handle);

    #if defined(DEBUG) && defined(TASK_LOGGING)
      xSemaphoreTake(uart_mutex, portMAX_DELAY);
      format_terminal_for_new_entry();
      serial.print("Button Task Entered\n");
      format_new_terminal_entry();
      xSemaphoreGive(uart_mutex);
    #endif

    if (green_flag && validate_input(GREEN_BUTTON_PIN)) {

      center_string_select = green;
      start_time = millis();
      green_flag = 0;
      params -> red_state = 0;
      params -> green_state ^= 1; //flip green state from whatever it is
      new_request = GREEN;
      new_request_payload = params -> green_state;

      #if defined(DEBUG)
        xSemaphoreTake(uart_mutex, portMAX_DELAY);
        format_terminal_for_new_entry();
        serial.println("Green pressed\n");
        format_new_terminal_entry();
        xSemaphoreGive(uart_mutex);
      #endif

      power_management_parameters.last_activity = millis(); //update the last activity time


    }

    else if (green_pulse_flag && validate_input(GREEN_PULSE_BUTTON_PIN)){
      center_string_select = green_pulse;
      start_time = millis();
      green_pulse_flag = 0;
      new_request = GREEN_PULSE;
      params -> red_state = 0;
      params -> green_state = 0;
      
      #if defined(DEBUG)
        xSemaphoreTake(uart_mutex, portMAX_DELAY);
        format_terminal_for_new_entry();
        serial.println("Green pulse pressed\n");
        format_new_terminal_entry();
        xSemaphoreGive(uart_mutex);
      #endif

      power_management_parameters.last_activity = millis();
    }

    else if (red_flag && validate_input(RED_BUTTON_PIN)) {
      center_string_select = red; 
      red_flag = 0;
      start_time = millis();
      params -> red_state ^= 1; //flip red state from whatever it is
      params -> green_state = 0;
      new_request = RED;
      new_request_payload = params -> red_state;
      
      #if defined(DEBUG)
        xSemaphoreTake(uart_mutex, portMAX_DELAY);
        format_terminal_for_new_entry();
        serial.print("Red pressed\n");
        format_new_terminal_entry();
        xSemaphoreGive(uart_mutex);
      #endif

      #ifdef TEST_PLAN_5
      digitalWrite(TESTING_PIN, testState ? HIGH : LOW);
      testVar++;

      format_terminal_for_new_entry();
      serial.printf("Button was pressed %d number of times\n", testVar);
      format_new_terminal_entry();
 
      format_terminal_for_new_entry();
      if (testState){
      serial.println("LED turns on\n" );
      } else {
        serial.println("LED turns off\n");
      }
      format_new_terminal_entry();
      testState = !testState;
      #endif

      power_management_parameters.last_activity = millis();

    }

    else if (override_flag && validate_input(OVERRIDE_BUTTON_PIN)) {
      center_string_select = override; 
      override_flag = 0;
      new_request = OVERRIDE_STOP;
      is_override = false;
      status_string_select = none;
      start_time = millis();
      #if defined(DEBUG)
        xSemaphoreTake(uart_mutex, portMAX_DELAY);
        format_terminal_for_new_entry();
        serial.println("Override pressed\n");
        format_new_terminal_entry();
        xSemaphoreGive(uart_mutex);
      #endif

      power_management_parameters.last_activity = millis();

    }

    else {
      green_flag = 0, green_pulse_flag = 0, red_flag = 0, override_flag = 0;
    }

    #ifdef RF_ENABLED
      xQueueSend(request_queue, &new_request, 0);
      xQueueSend(payload_queue, &new_request_payload, 0);
      vTaskResume(request_task_handle);
    #endif

    new_request = NONE; //Reset new request
    new_request_payload = 0;
    
    #if defined(DEBUG) && defined(TASK_LOGGING)
      xSemaphoreTake(uart_mutex, portMAX_DELAY);
      format_terminal_for_new_entry();
      serial.print("Button Task Exited\n");
      format_new_terminal_entry();
      xSemaphoreGive(uart_mutex);
    #endif
  }
}

//Name: display_task
//Purpose: FreeRTOS task that periodically updates the GUI on the display.
//Inputs: None
//Outputs: None
void display_task(void *pvParameters) {
  while(1){
    
    vTaskDelay(REFRESH_DELAY); 
    
    #if defined(DEBUG) && defined(TASK_LOGGING)
      xSemaphoreTake(uart_mutex, portMAX_DELAY);
      format_terminal_for_new_entry();
      serial.print("Display Task Entered\n");
      format_new_terminal_entry();
      xSemaphoreGive(uart_mutex);
    #endif
    xSemaphoreTake(display_mutex, portMAX_DELAY);
    if (display_update_flag) {
      update_GUI(ping_parameters.is_connected , display_strings[center_string_select], display_string_sizes[center_string_select], battery_soc, display_strings[status_string_select], display_string_sizes[status_string_select]);
      if ((millis() - start_time)/1000 > 5){
        center_string_select = none;
      }
    }
    xSemaphoreGive(display_mutex);
    #if defined(DEBUG) && defined(TASK_LOGGING)
      xSemaphoreTake(uart_mutex, portMAX_DELAY);
      format_terminal_for_new_entry();
      serial.print("Display Task Exited\n");
      format_new_terminal_entry();
      xSemapohoreGive(uart_mutex);
    #endif
  }
}

//Name: override_task
//Purpose: FreeRTOS task that processes override.
//Inputs: None
//Outputs: None
void override_task(void *pvParameters) {
  override_t * parameters = (override_t *) pvParameters;
  while(1){

    vTaskSuspend(override_task_handle);
    
    vTaskSuspend(power_management_task_handle);
    new_override = 0;
    // #ifdef DEBUG
    //   xSemaphoreTake(uart_mutex, portMAX_DELAY);
    // #endif

    #if defined(DEBUG) && defined(TASK_LOGGING)
      format_terminal_for_new_entry();
      serial.print("Override Task Entered\n");
      format_new_terminal_entry();
    #endif

    //Need to keep in mind you might get an override in middle of an override
    detachInterrupt(GREEN_BUTTON_PIN);
    detachInterrupt(GREEN_PULSE_BUTTON_PIN);
    detachInterrupt(RED_BUTTON_PIN);
    #ifndef NO_SCREEN
      display_update_flag = false;
    #endif
    status_string_select = override;
    parameters -> end_time = millis() + (override_delay * 1000); //converting seconds to milliseconds
    
    xSemaphoreTake(display_mutex ,portMAX_DELAY);
    while (millis() < parameters -> end_time && is_override) {
      if (new_override == 1){
        new_override = 0;
        parameters -> end_time = millis() + (override_delay * 1000);
      }
      parameters -> time_left = (parameters -> end_time - millis()) / 1000;
      parameters -> len = sprintf(display_string, "%02d:%02d", parameters -> time_left / 60, parameters -> time_left % 60);
      #if defined(DEBUG) 
      format_terminal_for_new_entry();
      serial.printf("Override Timer running: %02d:%02d\n", parameters -> time_left / 60, parameters -> time_left % 60);
      format_new_terminal_entry();
      #endif
      #ifndef NO_SCREEN
        update_GUI(ping_parameters.is_connected, display_string, parameters -> len, battery_soc, display_strings[status_string_select], display_string_sizes[status_string_select]);
      #endif
      //delay(REFRESH_DELAY);
      vTaskDelay(REFRESH_DELAY);
    }
    xSemaphoreGive(display_mutex);
    status_string_select = none;
    light_parameters.green_state = 0;
    light_parameters.red_state = 0;

    attachInterrupt(uint32_t(GREEN_BUTTON_PIN), green_button_ISR, FALLING);
    attachInterrupt(uint32_t(GREEN_PULSE_BUTTON_PIN), green_pulse_button_ISR, FALLING);
    attachInterrupt(uint32_t(RED_BUTTON_PIN), red_button_ISR, FALLING);
    
    #ifndef NO_SCREEN
    vTaskDelay(500); //I think this is the cause of the display woes.
    display_update_flag = true;
    #endif
    
    power_management_parameters.last_activity = millis();
    vTaskResume(power_management_task_handle);
    // interrupts();

    #if defined(DEBUG) && defined(TASK_LOGGING)
      format_terminal_for_new_entry();
      serial.print("Override Task Exited\n");
      format_new_terminal_entry();
    #endif
    // #ifdef DEBUG
    //   xSemaphoreGive(uart_mutex);
    // #endif
  }
}

//Name: soc_monitoring_task
//Purpose: FreeRTOS task that periodically checks the battery voltage and maps it to an SoC
//Inputs: None
//Outputs: None
void soc_monitoring_task(void *pvParameters) {
  static double hysteresis_voltage = SOC_CONSTANT * ( ( ( (double) analogRead(A7) ) / 1024 ) * 3.3 ); //for hysteresis;
  while(1){

    vTaskDelay(SOC_MONITORING_DELAY); 
    
    #ifdef DEBUG
      xSemaphoreTake(uart_mutex, portMAX_DELAY);
    #endif

    #if defined(DEBUG) && defined(TASK_LOGGING)
      format_terminal_for_new_entry();
      serial.print("SoC Monitoring Task Entered\n");
      format_new_terminal_entry();
    #endif

    battery_voltage = SOC_CONSTANT * ( ( ( (double) analogRead(A7) ) / 1024 ) * 3.3 );
    
    //Implementing hysteresis to not confuse the user
    //If the voltage fluctuation is larger than 200 mV, a battery was likely plugged in (so update).
    hysteresis_voltage = ((battery_voltage - hysteresis_voltage) < 0.2) ? min(hysteresis_voltage, battery_voltage) : hysteresis_voltage = battery_voltage; 
    battery_soc = soc_mapping(hysteresis_voltage);
    #if defined(DEBUG) && defined(TASK_LOGGING)
      format_terminal_for_new_entry();
      serial.print("SoC Monitoring Task Exited\n");
      format_new_terminal_entry();
    #endif

    #ifdef DEBUG
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
      xSemaphoreTake(uart_mutex, portMAX_DELAY);
    #endif

    //Check to see if there has been activity within the last N seconds. 
    //If there has been, either do nothing or take out of idle state.
    //If there hasn't been, go into idle mode. 
    
    volatile uint32_t current_time = millis();

    if((power_management_parameters -> idle == false) && (millis() > (power_management_parameters -> last_activity + IDLE_START_MILLISECONDS) )) {
      
      //Take mutex so it doesn't get stuck in the display task.
      xSemaphoreTake(uart_mutex, portMAX_DELAY);
      
      #ifndef NO_SCREEN
        display_update_flag = false;
        
        //Turn off display
        display.clearDisplay();
        display.display();
      #endif 

      power_management_parameters -> idle = true;

      LowPower.deepSleep();

      //Now that the risk of the mutex being stuck is gone, give it back.
      xSemaphoreGive(uart_mutex); 
    }

    else if ( (power_management_parameters -> idle == true) && (current_time < (power_management_parameters -> last_activity + IDLE_START_MILLISECONDS))) {
      
      display_update_flag = true;

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

//Name: wakeup_callback
//Purpose: Resume the RTOS scheduler
//Inputs: None
//Outputs: None
void wakeup_callback(){
  xTaskResumeAll();
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

    xSemaphoreTake(uart_mutex, portMAX_DELAY);

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
    serial.printf("* Current Ack Context = %d  *\33[0K\033[E", request_handler_parameters.ack_context);
    serial.printf("*   Connection Status = %d  *\33[0K\033[E", ping_parameters.is_connected);
    serial.printf("*   Messages in Queue = %d  *\33[0K\033[E", uxQueueMessagesWaiting(request_queue));
    serial.printf("*  Battery Voltage = %.2lfV *\33[0K\033[E", battery_voltage);
    serial.printf("*    Battery SoC = %3d%%    *\33[0K\033[E", battery_soc);
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

    xSemaphoreGive(uart_mutex);
  }
}
