#include "delay.h"
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

//I2C Display
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels

//Screen Imports
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

//Strings & enums for  center text options
char no_string[] = "";
char welcome_string[] = "Welcome";
char override_string[] = "Override";
char green_string[] = "Green";
char green_pulse_string[] = "Pulse";
char red_string[] = "Red";
char transmit_failed_string[] = "Tx Failed";

char * display_strings[] = {no_string, welcome_string, override_string, green_string, green_pulse_string, red_string, transmit_failed_string}; //"", "Welcome", "Override", "Green", "Pulse", "Red"};
int display_string_sizes[] = {0, 7, 8, 5, 5, 3, 9};
enum display_string_options {none, welcome, override, green, green_pulse, red, transmit_failed};

//Display Strings
char buffer[150];
char line1[21];
char line2[21];
char padding[21];
char connection_status;

TaskHandle_t display_wdt_task_handle;

typedef struct { 
  uint32_t display_update_start_time;
  volatile bool active;
 } display_wdt_params_t;

 display_wdt_params_t display_wdt_params;

 void display_wdt_task(void * pvParams);

void display_init(){
  xTaskCreate(display_wdt_task, // Task function
            "Display WDT Task", // Task name
            42, // Stack size 
            &display_wdt_params, 
            4, // Priority
            &display_wdt_task_handle); // Task handler
  
  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }
  
  // Show initial display buffer contents on the screen --
  // the library initializes this with an Adafruit splash screen.
  display.display();
  delay(500); // Pause for .5 seconds

  // Clear the buffer
  display.clearDisplay();

  //Set text font & color
  display.cp437(true);         // Use full 256 char 'Code Page 437' font
  display.setTextColor(SSD1306_WHITE); // Draw white text
}

void print2screen(char * buffer, int buffer_length) {
  display.setTextSize(1);      // Normal 1:1 pixel scale
  display.setTextColor(SSD1306_WHITE); // Draw white text
  display.setCursor(0, 0);     // Start at top-left corner
  display.cp437(true);         // Use full 256 char 'Code Page 437' font

  // Not all the characters will fit on the display. This is normal.
  // Library will draw what it can and the rest will be clipped.
  for(int16_t i=0; i<buffer_length; i++) {
    if (i < buffer_length) display.write(buffer[i]);
  }

}

//Name: print2Center
//Purpose: Prints text pointed to by the buffer pointer to the center of the screen.
//Inputs: buffer (pointer that points to the string being printed to the screen) & buffer_length (the number of characters in the buffer)
//Outputs: None
void print2Center(char * buffer, int buffer_length){
  
  // Overlay center text during override -> Hopefully can turn this into an interrupt flag
  display.setTextSize(2);
  int x_offset = (10-buffer_length) * 12; //Characters are approximately 12 pixels wide at this font size.
  x_offset += (x_offset % 2); //if the offset is odd, add one before dividing by 2 (justifying to the right)
  x_offset /= 2;

  display.setCursor(x_offset, 8);  // Start at vertical center, and attempt to center text horizontally. Experimentally, text is centered vertically with an offset of 8.
  //NOTES: here are approximately 12 pixels per size=2 character horizontally, and ~10 size=2 characters per horizontal line (~16 vertical)

  for(int16_t i=0; i<buffer_length; i++) {
    if (i < buffer_length) display.write(buffer[i]);
  }
  
}

//Name: draw_battery
//Purpose:
//Inputs
//Outputs
void draw_battery(uint8_t soc){ //soc is a number between 0 and 100
  
  int x_offset = 100;
  int y_offset = 4;
  int final_x_value = 10;
  int final_y_value = 5;

  for (int x_val = 0; x_val < final_x_value; x_val++){ 
    for (int y_val = 0; y_val < final_y_value; y_val++){
      //Rules for drawing a pixel
      //1.) Draw a pixel if the x/y value is the first or last value.
      //2.) Draw a pixel if the number of charge block columns isn't 0.
      if ( (soc >= (x_val * 10)) || (x_val == final_x_value) || (y_val == 0) || (y_val == final_y_value - 1)){ 
        display.drawPixel(x_offset+x_val, y_offset+y_val, WHITE );
      } 
    }
    //decrement number of charge columns if a column was finished that wasn't the first or last (also don't decement if the number of columns is already zero)
  }

  //Draw the battery contact
  for (int x_val = 1; x_val <= 2; x_val++){ 
    display.drawPixel(x_offset+final_x_value+x_val, y_offset+1, WHITE );
    display.drawPixel(x_offset+final_x_value+x_val, y_offset+2, WHITE );
    display.drawPixel(x_offset+final_x_value+x_val, y_offset+3, WHITE );
  }
}

//Name: draw_status
//Purpose: draws current POL status
//Inputs
//Outputs
void draw_status(char * buffer, int buffer_length){
  display.setTextSize(1);

  //int x_offset = (10-buffer_length) * 12; //Characters are approximately 12 pixels wide at this font size.
  //x_offset += (x_offset % 2); //if the offset is odd, add one before dividing by 2 (justifying to the right)
  //x_offset /= 2;

  int x_offset = 125-(buffer_length * 6) ;

  display.setCursor(x_offset, 24);  


  for(int16_t i=0; i<buffer_length; i++) {
    if (i < buffer_length) display.write(buffer[i]);
  }

}


//Name: display_update_wdt_task
//Purpose: Quick fix to display driver code inadequacy.
//Inputs: pvParams (display_wdt_params)
//Outputs: None
#define DISPLAY_UPDATE_TIMEOUT (500)
void display_wdt_task(void * pvParams){
  display_wdt_params_t * display_wdt_params = (display_wdt_params_t * ) pvParams;
  display_wdt_params -> active = false;

  volatile bool x = 0; //debug to see when loop entered;
  
  while (1){
    vTaskSuspend(NULL);
    while (display_wdt_params -> active){
      vTaskDelay(100);
      if (millis() > (display_wdt_params -> display_update_start_time + DISPLAY_UPDATE_TIMEOUT)){
        x ^= 1;
        display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS);
        display.clearDisplay();
        display.display();
      }
    }

  }
}



//Name: update_GUI
//Purpose: Updates the GUI based upon parameters passed.
//Inputs: is_connected (boolean stating whether the HHD is able to ping the POL), center_text (pointer to text being printed to the center of the screen), and center_text_length (the number of characters beign printed to the center of the screen).
//Outputs: None
void update_GUI(bool is_connected, char * center_text, int center_text_length, uint8_t soc, char * status_text, int status_text_length){
  
  //Update Display
  display.clearDisplay();
  
  //Update Connection Status
  if (is_connected) connection_status = 'o';                                               //Connection status is good
  else if (connection_status == 'o' || connection_status == ' ') connection_status = 'x';  //Connection status is bad -- flash an X
  else connection_status = ' ';

  // New text
  sprintf(line2, "[%c]", connection_status);
  sprintf(buffer, "%-21s%-21s%-21s%-21s", padding, padding, padding, line2);
  print2screen(buffer, sizeof(buffer));
  // delay(100);

  draw_battery(soc);
  print2Center(center_text, center_text_length);
  draw_status(status_text, status_text_length);
  
  //Update display
  display_wdt_params.display_update_start_time = millis();
  display_wdt_params.active = true;
  vTaskResume(display_wdt_task_handle);
  display.display();
  display_wdt_params.active = false;
  return;

}