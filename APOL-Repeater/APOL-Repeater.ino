#include "APOL-Repeater.h"

//Taskhandles
TaskHandle_t rx_task_handle;
TaskHandle_t terminal_task_handle;
TaskHandle_t soc_monitoring_task_handle;
TaskHandle_t power_management_task_handle;

QueueHandle_t request_queue;
QueueHandle_t payload_queue;

//Mutexes
SemaphoreHandle_t uart_mutex;

//Settings and state variables
bool testState;
bool is_override = false;

typedef struct{
  uint32_t last_activity;
  bool idle;
} power_management_parameters_t;

power_management_parameters_t power_management_parameters;

uint8_t battery_soc;
double battery_voltage;

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


  xTaskCreate(soc_monitoring_task, // Task function
            "BATTERY MONITORING", // Task name
            64, // Stack size 
            NULL, 
            2, // Priority
            &soc_monitoring_task_handle); // Task handler

  xTaskCreate(power_management_task, // Task function
            "POWER MANAGEMENT", // Task name
            256, // Stack size 
            &power_management_parameters, 
            3, // Priority
            &power_management_task_handle); // Task handler


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

//Name: rx_task
//Purpose: FreeRTOS task that checks for Lo-Ra packets, and handles them (triggered by RFM95 interrupt).
//Inputs: None
//Outputs: None
void rx_task(void *pvParameters) {
  while(1){

    vTaskSuspend(rx_task_handle);

    #ifdef DEBUG
      xSemaphoreTake(uart_mutex, portMAX_DELAY);
    #endif
    
    if(comms.check_for_any_packet()){
      format_terminal_for_new_entry();
      serial.printf("Forwarding New Packet (Sender: %s Target: %s Request: %s Payload: %d)\n", comms.subsystem_strings[comms.packet_contents.sender_device], comms.subsystem_strings[comms.packet_contents.target_device], comms.request_strings[comms.packet_contents.request], comms.packet_contents.payload);
      format_new_terminal_entry();

      comms._device_type = comms.packet_contents.sender_device; //Mock sender
      comms.send_packet(comms.packet_contents.request, comms.packet_contents.target_device, comms.packet_contents.payload); //repeat
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

//Name: soc_monitoring_task
//Purpose: FreeRTOS task that periodically checks the battery voltage and maps it to an SoC
//Inputs: None
//Outputs: None
void soc_monitoring_task(void *pvParameters) {
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
    battery_soc = soc_mapping(battery_voltage);
    
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

      power_management_parameters -> idle = true;

      LowPower.deepSleep();

      //Now that the risk of the mutex being stuck is gone, give it back.
      xSemaphoreGive(uart_mutex); 
    }

    else if ( (power_management_parameters -> idle == true) && (current_time < (power_management_parameters -> last_activity + IDLE_START_MILLISECONDS))) {
            
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
    serial.printf("*  Battery Voltage = %.2lfV *\33[0K\033[E", battery_voltage);
    serial.printf("*    Battery SoC = %3d%%    *\33[0K\033[E", battery_soc);
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
