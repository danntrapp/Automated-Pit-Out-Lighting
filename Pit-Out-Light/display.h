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

extern SemaphoreHandle_t uart_mutex;

static const unsigned char PROGMEM logo_bmp[] =
{ 0b00000000, 0b11000000,
  0b00000001, 0b11000000,
  0b00000001, 0b11000000,
  0b00000011, 0b11100000,
  0b11110011, 0b11100000,
  0b11111110, 0b11111000,
  0b01111110, 0b11111111,
  0b00110011, 0b10011111,
  0b00011111, 0b11111100,
  0b00001101, 0b01110000,
  0b00011011, 0b10100000,
  0b00111111, 0b11100000,
  0b00111111, 0b11110000,
  0b01111100, 0b11110000,
  0b01110000, 0b01110000,
  0b00000000, 0b00110000 };

  //Display functions -> It seems like header files in .ino files are intended to have functions in them?

void display_init(){
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

void print2Center(char * buffer, int buffer_length){
  
  // Overlay center text during override -> Hopefully can turn this into an interrupt flag
  display.setTextSize(2);
  int x_offset = (10-buffer_length) * 12;
  x_offset += (x_offset % 2); //if the offset is odd, add one before dividing by 2 (justifying to the right)
  x_offset /= 2;

  display.setCursor(x_offset, 8);  // Start at vertical center, and attempt to center text horizontally
  //NOTES: here are approximately 12 pixels per size=2 character horizontally, and ~10 size=2 characters per horizontal line (~16 vertical)

  for(int16_t i=0; i<buffer_length; i++) {
    if (i < buffer_length) display.write(buffer[i]);
  }
  
}

void update_GUI(int time){
  
  // time = millis() % 60*60;

  //Update Display
  display.clearDisplay();


  char time_string[6];
  sprintf(time_string, "%02d:%02d", time / 60, time % 60);
  time_string[5] = '\0'; 
  print2Center(time_string, 5);


  //Update display
  display.display();
  
  return;

}

