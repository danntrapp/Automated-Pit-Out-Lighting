#include <APOL_Comms_Lib.h>

#define MAX_BUFFER_SIZE (100)
#define MAX_ARGS (4)
#define NUM_PERSISTENT_LINES 5

//global vars
extern APOL_Comms_Lib comms;
extern TaskHandle_t rx_task_handle;
extern TaskHandle_t power_management_task_handle;

#ifdef UART
  extern Uart & serial;
#else
  extern Serial_ & serial;
#endif

char input_buffer [MAX_BUFFER_SIZE] = {0};
int buffer_pos;

subsystem term_destination_device = POL; //default is POL
request_type term_request_type = NONE;
uint8_t term_request_payload = 0; 
uint8_t current_tx_power = 20; //default

//Name: lessThan
//Purpose: Alternative less than function (used to stop the compiler from "optimizing" certain lines)
//Inputs: 
//Outputs: 
inline int lessThan(int a, int b){
  //USE WITH CAUTION 
  //THE COMPILER IS RANDOMLY OPTIMIZING A < B OPERATIONS INCORRECTLY
  //THIS IS A WORKAROUND
  //FYI: Using pragma to turn down optimization level isn't working
  //     Options are to turn off optimization (bad) or implement a workaround (good).
   return max(ceil(b-a), 0); // if a > b, b - a < 0
} 

//Name: format_terminal_for_new_entry
//Purpose: 
//Inputs: 
//Outputs: 
inline void format_terminal_for_new_entry(){
  serial.printf("\033[%dF\033[2K", NUM_PERSISTENT_LINES); //this escape moves the cursor up N lines and to the beginning of that line and clear that line
}

//Name: format_terminal_for_new_entry
//Purpose: 
//Inputs: 
//Outputs: 
inline void format_new_terminal_entry(){
  for (int i = 0; i < NUM_PERSISTENT_LINES; i++) serial.println("\033[2K"); 
}

//Name: clear_buffer
//Purpose: Replace all values in an array with null terminators. 
//Inputs: buffer (a pointer to the character buffer) and buffer_entries (an integer value containing the number of characters in the buffer)  
//Outputs: None
inline void clear_buffer(char * buffer, int buffer_entries){
    for (int idx = 0; idx < buffer_entries; idx++){
        buffer[idx] = '\0'; //Fill w/ null terminators
    }
}

//Name: argument_mapping
//Purpose: map arguments to corresponding actions.
//Inputs: char * arguments (array of pointers to argument strings), num_args (the number of actual arguments received)  
//Outputs: None
void argument_mapping(char * arguments[MAX_ARGS], uint8_t num_args){

    if (0 == strcmp(arguments[0], "help")){ 
      format_terminal_for_new_entry();
      serial.print("Valid options are: configure, clear, disable, enable, pingtest, send, and trigger.\n");
      format_new_terminal_entry();
    }
    else if (0 == strcmp(arguments[0], "configure")){

      if (num_args < 2){
        format_terminal_for_new_entry();
        serial.print("Incorrect number of arguments entered for configure command.\n");
        format_new_terminal_entry();
      }
      else if (0 == strcmp(arguments[1], "help")){
        format_terminal_for_new_entry();
        serial.print("Valid options are: destination, status, and terminal\n");
        format_new_terminal_entry();
      }

      else if (0 == strcmp(arguments[1], "destination")){
      format_terminal_for_new_entry();
        if (num_args < 3){
          format_terminal_for_new_entry();
          serial.print("Incorrect number of arguments entered for configure destination command.\n");
          format_new_terminal_entry();
        }
        
        else if (0 == strcmp(arguments[2], "HHD")){
          format_terminal_for_new_entry();
          serial.print("Destination set to Handheld Device.\n");
          format_new_terminal_entry();
          term_destination_device = HHD;
        } 
        
        else if (0 == strcmp(arguments[2], "POL")){ 
          format_terminal_for_new_entry();
          serial.print("Destination set to Pit Out Light.\n");
          format_new_terminal_entry();
          term_destination_device = POL;
        } 
        
        else if (0 == strcmp(arguments[2], "VDD")){
          format_terminal_for_new_entry();
          serial.print("Destination set to Vehicle Deteciton Device.\n");
          format_new_terminal_entry();
          term_destination_device = VDD;
        }
        
        else{
          format_terminal_for_new_entry();
          serial.print("Incorrect subsystem entered. Try again.\n");
          format_new_terminal_entry();
        }
      
      }

      else if (0 == strcmp(arguments[1], "payload")){
        if (num_args < 3){
          format_terminal_for_new_entry();
          serial.print("Incorrect number of arguments entered for configure payload command.\n");  
          format_new_terminal_entry();
        }
        term_request_payload = atoi(arguments[2]);
        format_terminal_for_new_entry();
        serial.printf("Payload set to %d.\n", term_request_payload);
        format_new_terminal_entry();
      }

      else if (0 == strcmp(arguments[1], "power")){
        if (num_args < 3){
          format_terminal_for_new_entry();
          serial.print("Incorrect number of arguments entered for configure power command.\n");  
          format_new_terminal_entry();
        }
        else {
          int requested_power = atoi(arguments[2]);

          if (requested_power > 20 || requested_power < 2){ //if power is above 20 dBm or below 2 dBm, complain. 
            
            format_terminal_for_new_entry();
            serial.print("Invalid power entered. Please enter a value between 2 dBm and 20 dBm\n");  
            format_new_terminal_entry();
          
          }
          
          else {

            comms.rf95 -> setTxPower(requested_power);
            format_terminal_for_new_entry();
            serial.printf("Power set to %d dBm.\n", requested_power);
            current_tx_power = requested_power;
            format_new_terminal_entry();
          
          }
        }
      }

      else if (0 == strcmp(arguments[1], "request")){
        
        if (num_args < 3){
          format_terminal_for_new_entry();
          serial.print("Incorrect number of arguments entered");
          serial.print("for configure request command.\n");
          format_new_terminal_entry();
        } 
        else {
          
          bool arg_found = 0;

          for(int request = 0; lessThan(request , NUM_REQUEST_TYPES) ; request++){ // Code to execute if request is less than NUM_REQUEST_TYPES (compiler is doing stupid shit again)

            if (0 == strcmp(arguments[2], comms.request_strings[request])){
              term_request_type = (request_type) request;
              format_terminal_for_new_entry();
              serial.printf("Request type set to %s.\n", comms.request_strings[term_request_type]);
              format_new_terminal_entry();
              arg_found = 1;
              break;
            }

          }
          
          if (arg_found == 0){
            format_terminal_for_new_entry();
            serial.print("Incorrect request type entered. Try again.\n");
            format_new_terminal_entry();
          }
        
        }
      
      } 
      
      else if (0 == strcmp(arguments[1], "status")){
        format_terminal_for_new_entry();
        serial.printf("Destination = %s\n\r", comms.subsystem_strings[term_destination_device]);
        serial.printf("\033[2KRequest Type = %s\n\r", comms.request_strings[term_request_type]);
        serial.printf("\033[2KPayload = %u\n\r", term_request_payload);
        serial.printf("\033[2KPayload = %u\n\r", term_request_payload);
        serial.printf("\033[2KTransmit power = %u dBm\n\r", current_tx_power);
        format_new_terminal_entry();
      } 

      else if (0 == strcmp(arguments[1], "terminal")){
        serial.print("\033[?25l"); //hide cursor
        serial.printf("\33[2J");
        serial.print("\033[H");
      }

      else {
        format_terminal_for_new_entry();
        serial.print("Invalid entry for configure command.\n");
        format_new_terminal_entry();
      }

    }
    
    else if (0 == strcmp(arguments[0], "clear")){
      serial.print("\033[H");
      serial.printf("\33[2J");

    }

     else if (0 == strcmp(arguments[0], "disable")){
        
      if (num_args < 2){
        format_terminal_for_new_entry();
        serial.print("Incorrect number of arguments entered.\n");
        format_new_terminal_entry();
      }
      else if (0 == strcmp(arguments[1], "help")){

        format_terminal_for_new_entry();
        serial.print("Valid options are: idle.\n");
        format_new_terminal_entry();
      }
      else if (0 == strcmp(arguments[1], "idle")) {
        vTaskSuspend(power_management_task_handle);
      }
      
      else {
        format_terminal_for_new_entry();
        serial.print("Invalid entry for disable command.\n");
        format_new_terminal_entry();
      }
    }

    else if (0 == strcmp(arguments[0], "enable")){
        
      if (num_args < 2){
        format_terminal_for_new_entry();
        serial.print("Incorrect number of arguments entered.\n");
        format_new_terminal_entry();
      }
      else if (0 == strcmp(arguments[1], "help")){

        format_terminal_for_new_entry();
        serial.print("Valid options are: idle.\n");
        format_new_terminal_entry();
      }
      else if (0 == strcmp(arguments[1], "idle")) {
        vTaskResume(power_management_task_handle);
      }
      
      else {
        format_terminal_for_new_entry();
        serial.print("Invalid entry for enable command.\n");
        format_new_terminal_entry();
      }
    }

    else if (0 == strcmp(arguments[0], "pingtest")){
        
      if (num_args < 2){
        format_terminal_for_new_entry();
        serial.print("Incorrect number of arguments entered.\n");
        format_new_terminal_entry();
      }
      else if (0 == strcmp(arguments[1], "help")){

        format_terminal_for_new_entry();
        serial.print("Valid options are: tx, rx.\n");
        format_new_terminal_entry();
      }
      else if (0 == strcmp(arguments[1], "tx")) {
      
        // suspend_all_tasks();
        for (int packet_idx = 1; packet_idx <= 1000; packet_idx++){
          
          //Send initial packet
          comms.send_packet(NONE, term_destination_device, packet_idx);
          comms.rf95 -> setModeRx();

          format_terminal_for_new_entry();
          serial.printf("Packet #%d sent.\n\r", packet_idx, NUM_PERSISTENT_LINES + 1);
          format_new_terminal_entry();

          delay(50);

          int attempts = 0;
          do {
            
            if (comms.check_for_packet() && (comms.packet_contents.request == ACK) && (comms.packet_contents.payload == packet_idx )){
              format_terminal_for_new_entry();
              serial.print("Transmit successful\n\r");
              format_new_terminal_entry();
              break;
            } 
            
            format_terminal_for_new_entry();
            serial.print("Trying to ressend a packet...\n\r");
            format_new_terminal_entry();

            comms.send_packet(NONE, term_destination_device, packet_idx);
            comms.rf95 -> setModeRx();

            delay(50);

            attempts++;

          } while (attempts < 5);
        }
      }
      
      else if (0 == strcmp(arguments[1], "rx")) {
      
        // suspend_all_tasks();
        comms.rf95 -> setModeRx();
        int end_time = millis() + 2.5*60*1000; // (1 minute = 60 * 1000 milliseconds) 
        
        // for (int packet_idx = 0; packet_idx <= 1000; packet_idx++){
        //   format_terminal_for_new_entry();
        //   serial.printf("New packet received (Packet Index = %d)!\n", packet_idx);
        //   format_new_terminal_entry();
        //   delay(100);
        // }
        while(millis() < end_time){ //reserve around a minute of time
          if (comms.check_for_packet()){

            format_terminal_for_new_entry();
            serial.printf("New packet received (Packet Index = %d)!\n", comms.packet_contents.payload);
            format_new_terminal_entry();

            comms.send_packet(ACK, comms.packet_contents.sender_device, comms.packet_contents.payload);
            comms.rf95 -> setModeRx();

          }
        }
        // resume_all_tasks();
      
      }
      else {
        format_terminal_for_new_entry();
        serial.print("Invalid entry for pingtest command.\n");
        format_new_terminal_entry();
      }
        
    }
    
    else if (0 == strcmp(arguments[0], "send")){
      
      if (0 == strcmp(arguments[1], "packet")){

        comms.send_packet(term_request_type, term_destination_device, term_request_payload);
        format_terminal_for_new_entry();
        serial.printf("Packet sent.\n");
        format_new_terminal_entry();
      }
      
      else { //If text is too long, the command only prints out the first 10-ish characters.
        format_terminal_for_new_entry();
        serial.print("IMPORTANT: first use the configure command to customizer your packet then type send packet.\n");
        format_new_terminal_entry();
      }

    }

    else {
      format_terminal_for_new_entry();
      serial.print("Invalid entry. Type help to find out what options are available.\n");
      format_new_terminal_entry();
    }
    return;
}


//Name: handle_input
//Purpose: handle the user's input.
//Inputs: None
//Outputs: None
void handle_input(char * user_input){
    
    const char delimiter[2] = " ";
    char * arguments[MAX_ARGS];
    size_t arg_num = 0; 

    //It seems as though I ran into a legitimate compiler bug as the conditional in the block below legitimately doesn't get evaluated correctly. 
    //Print statements tell me that arg_num is 8, but it thinks arg_num is also less than 4. Either program memory is being overwritten, 
    //or I've run into some edgecase. Compiling this snippet using repl tokenizes correctly, and the conditional is evaluated
    //correctly.

    // while(arg_num < 4){ //parse arguments
    //     if (arguments[arg_num] == NULL){
    //         serial.printf("No additional arguments found...");
    //         break;
    //     }
    //     arguments[arg_num++] = strtok(NULL, delimiter);
    //     serial.printf("Argument %d is %s [Sanity Check = %d]", arg_num, arguments[arg_num], arg_num<4);
    // }
    
    //I spent over an hour banging my head against a wall, so instead
    //I'm just gonna call strtok four times, and check for null pointers
    //that way we don't access memory we're not supposed to. 
    arguments[arg_num++] = strtok(user_input, delimiter);    
    arguments[arg_num++] = strtok(NULL, delimiter);
    arguments[arg_num++] = strtok(NULL, delimiter);
    arguments[arg_num] = strtok(NULL, delimiter); 
    
    uint8_t num_args = 0;
    if (arguments[arg_num--] != NULL) num_args++;
    if (arguments[arg_num--] != NULL) num_args++;
    if (arguments[arg_num--] != NULL) num_args++;
    if (arguments[arg_num] != NULL) num_args++;

    argument_mapping(arguments, num_args);

}