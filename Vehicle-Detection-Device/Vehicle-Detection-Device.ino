#include "Vehicle-Detection-Device.h"

//Taskhandles
TaskHandle_t override_task_handle;
TaskHandle_t rx_task_handle;
TaskHandle_t terminal_task_handle;
TaskHandle_t request_task_handle;
TaskHandle_t power_management_task_handle;

//Comms stack
APOL_Comms_Lib comms(VDD, &rx_task_handle);

//Queues
QueueHandle_t request_queue;
QueueHandle_t payload_queue;

request_handler_t request_handler_params;

//Mutexes
SemaphoreHandle_t uart_mutex;

typedef struct{
  uint32_t last_activity;
  bool idle;
} power_management_parameters_t;

power_management_parameters_t power_management_parameters;

//Global vars
uint32_t vehicle_detections = 0;

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

  #ifdef DEBUG
    serial.begin(BAUD_RATE);
    uart_mutex = xSemaphoreCreateMutex(); //mutex for uart
    xTaskCreate(terminal_task, // Task function
      "UART TERMINAL", // Task name
      256, // Stack size 
      NULL, 
      1, // Priority
      &terminal_task_handle); // Task handler
  #endif

  GPIO_init();

  xTaskCreate(override_task, // Task function
          "OVERRIDE TASK", // Task name
          256, // Stack size 
          NULL, 
          4, // Priority
          &override_task_handle); // Task handler

  #ifdef RF_ENABLED
    comms.begin();
    comms.rf95 -> setModeRx(); //Start in Rx Mode
    comms.rf95 -> setTxPower(20); //Set to max power (VDD far away from everything else)

    xTaskCreate(rx_task, // Task function
              "RX HANDLER", // Task name
              256, // Stack size 
              NULL, 
              9, // Priority
              &rx_task_handle); // Task handler]

  #endif
  
  xTaskCreate(request_handler_task, // Task function
                "REQUEST HANDLER TASK", // Task name
                256, // Stack size 
                &request_handler_params, 
                4, // Priority
                &request_task_handle); // Task handler


  // xTaskCreate(power_management_task, // Task function
  //           "POWER MANAGEMENT", // Task name
  //           256, // Stack size 
  //           &power_management_parameters, 
  //           3, // Priority
  //           &power_management_task_handle); // Task handler

  request_queue = xQueueCreate(MAX_QUEUED_REQUESTS, sizeof(request_type));
  payload_queue = xQueueCreate(MAX_QUEUED_REQUESTS, sizeof(uint32_t));

  //Start tasks
  vTaskStartScheduler();

}

void loop() {  
  // Do nothing.

}

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

    if(comms.check_for_packet() && comms.packet_contents.target_device == VDD){
      switch(comms.packet_contents.request){
        case ACK:
          #ifdef DEBUG
            format_terminal_for_new_entry();
            serial.printf("Ack received = %d & Ack context = %d. (for reference GREEN is %d).\n", comms.packet_contents.payload, request_handler_params.ack_context, GREEN);
            format_new_terminal_entry();
          #endif
          if (comms.packet_contents.payload == request_handler_params.ack_context) request_handler_params.ack_flag = 0;
          break;
        
        default:
          break;
      }
    } 

    comms.rf95 -> setModeRx(); //Put back into Rx mode after responding
    
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

void override_task(void *pvParameters) {
  const request_type override = OVERRIDE_START;
  const uint32_t no_payload = NONE;

  while(1){

    vTaskSuspend(override_task_handle);

    #ifdef DEBUG
      xSemaphoreTake(uart_mutex, portMAX_DELAY);
    #endif
    
    #if defined(DEBUG) && defined(TASK_LOGGING)
      format_terminal_for_new_entry();  
      serial.print("Override Task Entered\n");
      format_new_terminal_entry();
    #endif

    uint32_t current_time = millis();

    //Actually debouncing an input could potentially impact 
    if (current_time > (power_management_parameters.last_activity + DEBOUNCE_DELAY)){ //don't register a new input for the duration of one debounce delay
      
      power_management_parameters.last_activity = millis();

      vehicle_detections++;
      
      xQueueSend(request_queue, &override, 0);
      xQueueSend(payload_queue, &no_payload, 0);

      vTaskResume(request_task_handle);
    }

    #if defined(DEBUG) && defined(TASK_LOGGING)
      format_terminal_for_new_entry();
      serial.print("Override Task Exited\n");
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
      params -> ack_flag = true;
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
      } while (params -> ack_flag); //continuously send

    }
    
    params -> ack_context = NONE;
    
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
    volatile uint32_t current_time = millis(); //seems to stop compiler from making some sort of optimization
    if((power_management_parameters -> idle == false) && (current_time > (power_management_parameters -> last_activity + IDLE_START_MILLISECONDS)) ){
      
      #ifdef DEBUG
        xSemaphoreTake(uart_mutex, portMAX_DELAY);
        format_terminal_for_new_entry();
        serial.print("Going into deep sleep...\n");
        format_new_terminal_entry();
        xSemaphoreGive(uart_mutex);
      #endif

      //State machine -> system goes into idle
      power_management_parameters -> idle = true;
      
      //Stop scheduler
      // vTaskSuspendAll();

      //Put uC into deep sleep
      LowPower.deepSleep();
      
    }

    else if ( (power_management_parameters -> idle == true) && (current_time < (power_management_parameters -> last_activity + IDLE_START_MILLISECONDS))) {
      
      //If we're in idle, come out of it.
      #ifdef DEBUG
        xSemaphoreTake(uart_mutex, portMAX_DELAY);
        format_terminal_for_new_entry();
        serial.print("Coming out of deep sleep...\n");
        format_new_terminal_entry();
        xSemaphoreGive(uart_mutex);
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
    serial.printf("* Vehicle Detections = %02d *\33[0K\033[E", vehicle_detections);
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


