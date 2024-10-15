// GEO Cookstove Sensor Code
// Respira Bien 2024-2025
// Arduino code for the cookstove sensor
// This file contains setup() and loop(), as well as functions for initializing the sensors
// There are 7 files that must accompany this one: CO.cpp, CO.h, CO2.h, Display.ino, 
// PM.cpp, PM.h, and SD_Card.ino
// CO.cpp, CO.h, CO2.h, PM.cpp, and PM.h are files that work with the various sensors
// Display.ino contains functions that control how to display to the screen
// SD_Card.ino contains functions that set up and store to the SD Card
// The GitHub's Readme file contains a block diagram of the code in this file.


#include <Arduino.h>
#include <U8g2lib.h>
#include <SD.h>       // Used to read/write files to the SD card
#include <SPI.h>      // Used to communicate with the SD card reader
#include <RTClib.h>   // Library for the real time clock
#include <String.h>   // Required to use string data type
#include "PM.h"       // Header file with PM functions/data
#include "CO2.h"      // Header file with CO2 functions/data
#include "CO.h"       // Header file with CO functions/data
#include <LowPower.h> // Library to enable sleep mode
#include <String.h>

/*
   Pin configuration:
   GND -> GND
   VCC -> 5V
   SCK -> D13
   SDA -> D11
   RES -> D8
   DC  -> D9
   CS  -> D10
*/

U8G2_SSD1309_128X64_NONAME0_F_4W_SW_SPI u8g2(U8G2_R0, /* clock=*/ 13, /* data=*/ 11, /* cs=*/ 10, /* dc=*/ 9, /* reset=*/ 8);

// PIN NUMBERS
#define FAN 6         // Pin number for MOSFET (controls power to the fan)
#define CS 53         // Pin number for SD card reader
#define INTERRUPT 18  // Pin number for the RTC interrupt
#define SELECT 2      // Pin number for Select Button
#define TOGGLE 3      // Pin number for Toggle Button

// OBJECTS
RTC_PCF8523 rtc;      // Real time clock object
PM pm;                // PM Sensor object
CO2 co2;              // CO2 Sensor object
CO co;                // CO Sensor object
File myFile;          // SD Card Object


// GLOBAL VARIABLES
String fileName = "datos.csv";
String pm2_5;
String pm10;
double ppmCO = 0.0;

double ppmCO2 = 0.0;
double ppmPrelimCO2 = 0.0;
double coRaw = 0.0;
double co2Raw = 0.0;
double ppmCOSpec = 0.0;
double ppmCOcal = 0.0;


// Volatile variables are involved in state machines and are meant to change often
volatile bool togglePushed = false;
volatile bool selectPushed = false;
volatile bool menuArrowState = false;
volatile int menuArrowPos = 25;
volatile bool measureArrowState = false;
volatile int measureArrowPos = 65;
volatile int optionsArrowPos = 20;
volatile int waitDebounce = 0;
volatile int measureTime = 150; //Every 15 is one second


// Calibrated parameters for mapping the CO and CO2 values
double calibratedInterceptCO = 0;  
double calibratedSlopeCO = 11.673;
double coZero = 497.7;    // zero value, subtract this from coRaw to shift the graph over before multiplying the slope and adding the intercept
double calibratedInterceptCO2 = 35.02;
double calibratedSlopeCO2 = 0.8504;


// Global Variables for the second counter (to possibly replace the time clock)
// Second counter last update time
static unsigned long lastSecondUpdate = 0;

// Seconds since entering the measure state (so far this only resets to zero when the arduino is powered off. If you end a 
// measurement and start a new one it will just start counting from where it left off)
static unsigned long secondsCounter = 0;   


// Enumeration of state variable for the state machine
enum state {menu, wait, measure, record, error, options};
enum state currentState;


// All initialization in the setup phase
void setup(void) {
  // Setup Terminal
  Serial.begin(9600);
  Serial3.begin(9600);  // Serial3 will be used to communicate with the PM sensor
  Serial.println("Respira Bien Cookstove Sensor");
  Serial.println();

  // Setup text
  u8g2.begin();
  u8g2.setFont(u8g2_font_tinytim_tr);  // choose a suitable font
  // Find fonts from: https://github.com/olikraus/u8g2/wiki/fntlistallplain#u8g2-font-list

  // Start Screen
  u8g2.setDrawColor(1);
  u8g2.setFont(u8g2_font_8x13_mr);  // choose a suitable font
  u8g2.drawStr(20, 25, "Bienvenidos"); // write something to the internal memory
  u8g2.sendBuffer();          // transfer internal memory to the display
  delay(2000);
  u8g2.clearBuffer();
  u8g2.sendBuffer();

  // Setup buttons
  pinMode(SELECT, INPUT);           // Configures the SELECT button as INPUT for Interrupts
  pinMode(TOGGLE, INPUT);           // Configures the TOGGLE button as INPUT for Interrupts
  pinMode(INTERRUPT, INPUT_PULLUP); // Initialize the interrupt pin with the internal pull up resistor in the Arduino
  pinMode(FAN, OUTPUT);             // Configures MOSFET pin as OUTPUT
//  pinMode(RED, OUTPUT);             // Configures RED pin as OUTPUT
//  pinMode(BLUE, OUTPUT);            // Configures BLUE pin as OUTPUT
//  pinMode(GREEN, OUTPUT);           // Configures GREEN pin as OUTPUT
  digitalWrite(FAN, LOW);           // Sets MOSFET pin to LOW

  // Setup Interrupts for buttons
  attachInterrupt(digitalPinToInterrupt(SELECT), isrSelect, FALLING); 
  attachInterrupt(digitalPinToInterrupt(TOGGLE), isrToggle, FALLING);

  // Initialize each sensor
  //           PM     CO    CO2   RTC   SD
  initSensors(false, true, true, true, true);
  Serial.println("Sensors initialized successfully");

  // Start the code in the MENU State
  currentState = menu;

  // Turn on the fan
  Serial.println("Turn on fan");
  digitalWrite(FAN, HIGH);

  // Set up the SD Card file where measurements are stored
  bool makeHeader = printHeader();
  if (makeHeader) {
    Serial.println("made header for file");
  }
  else {
    Serial.println("failed to make header for file");
  }
    
}

// Continuous loop, checks the program state each time to determine what to do
void loop(void) {

  static int fanTimer = 0;
  static bool writeSuccess = false;
  static bool SDCardSuccess = false;
  
  // Perform state update (Mealy).
  switch(currentState) { 
    ////////////////////////////////////////////////////////////////////////////////////////
    // MENU State: Stays in menu unless deciding to start measuring or to change options  //
    ////////////////////////////////////////////////////////////////////////////////////////
    case menu:
      if (selectPushed) {
        // Go to Options screen
        if (menuArrowState) { 
          currentState = options; 
          clearScreen();
        }
        // Go to Measure screen
        else { 
          currentState = measure;
          waitDebounce = 0; 
          clearScreen();
        }
        selectPushed = false;
      }
      break;
    ////////////////////////////////////////////////////////////////////////////////////
    // WAIT State: Waits for a specified amount of time then goes to MEASURE          //
    // Does not put Arduino to sleep because the power source will turn off if        //
    // Arduino is not consuming at least 50mah                                        //
    ////////////////////////////////////////////////////////////////////////////////////
    case wait:
      // Select button controls
      if (selectPushed) {
        // If "Salir: Si" is selected, go back to MENU
        if (measureArrowState) { 
          currentState = menu;
          measureArrowPos = 65;
          measureArrowState = false;
          clearScreen();
        } else {}
        selectPushed = false;
      }
      // If waiting enough time, go to Measure
      else if (waitDebounce > measureTime) {
        waitDebounce = 0;
        currentState = measure;
      } else {
        currentState = wait;
        // I created a countSeconds function so that while the code is in this state it will be incrementing the second counter
        countSeconds();
      }
      break;
    ////////////////////////////////////////////////////////////////////////////////////
    // MEASURE State: Contains select button controls, goes directly to RECORD        //
    ////////////////////////////////////////////////////////////////////////////////////
    case measure:
      if (selectPushed) {
        if (measureArrowState) {
          currentState = menu;
          measureArrowPos = 65;
          measureArrowState = false;
          clearScreen();
        } else {}
        selectPushed = false;
      } else {
        // I created a countSeconds function so that while the code is in this state it will be incrementing the second counter
        countSeconds();
        currentState = record;
      }
      break;
    ////////////////////////////////////////////////////////////////////////////////////
    // RECORD State: Contains select button controls, goes to either WAIT or ERROR    //
    ////////////////////////////////////////////////////////////////////////////////////
    case record:
      if (selectPushed) {
        if (measureArrowState) {
          currentState = menu;
          measureArrowPos = 65;
          measureArrowState = false;
          clearScreen();
        } else {}
        selectPushed = false;
      }
      // I created a countSeconds function so that while the code is in this state it will be incrementing the second counter
      countSeconds();
      // If the SD Card had an error, go to the ERROR state
      if (writeSuccess) {
        currentState = wait;
      } else {
        clearScreen();
        currentState = error;
      }
      break;
    ////////////////////////////////////////////////////////////////////////////////////
    // ERROR State: Stay in error state until error is fixed, then return to MEASURE  //
    ////////////////////////////////////////////////////////////////////////////////////
    case error:
      if (SDCardSuccess && printHeader()) {
        clearScreen();
        currentState = measure;
      }
      break;
    case options:
    default:
      break;
  }

  // Perform state action (Moore).
  switch(currentState) { 
    ////////////////////////////////////////////////////////////////////////////////////
    // MENU State: Draw Menu screen, change arrows with toggle button                 //
    ////////////////////////////////////////////////////////////////////////////////////
    case menu:
      // Draw menu screen
      printMenuScreen(menuArrowPos);
      if (togglePushed) {
        if (menuArrowState) {
          menuArrowState = false;
          menuArrowPos = 25;
        }
        else {
          menuArrowState = true;
          menuArrowPos = 45;
        }
        togglePushed = false;
      }
      break;
    ////////////////////////////////////////////////////////////////////////////////////
    // WAIT State: Draw Wait/Measure/Record screen, update debouncer                  //
    ////////////////////////////////////////////////////////////////////////////////////
    case wait:
      // Draw wait screen
      printMeasureScreen(measureArrowPos, ppmCOcal, coRaw, pm2_5);

      // I created a countSeconds function so that while the code is in this state it will be incrementing the second counter
      countSeconds();
      
      // Control toggle buttons
      measureWaitButtons();
      waitDebounce++;
      break;
    ////////////////////////////////////////////////////////////////////////////////////
    // MEASURE State: Record data and draw Wait/Measure/Record screen                 //
    ////////////////////////////////////////////////////////////////////////////////////
    case measure:
      measureWaitButtons();
      // I created a countSeconds function so that while the code is in this state it will be incrementing the second counter
      countSeconds();

      //          *************************************** PM measuring ***************************************
      
      pm.measure();         // PM sensor reads measurements, variables in PM library will contain the results
      pm2_5 = pm.pm2_5;     // Concentration of PM that is 2.5micrometers
      pm10 = pm.pm10;       // Concentration of PM that is 10micrometers
      
      //          *************************************** CO measuring ***************************************
      
      coRaw = co.measure();
      //This next line subtracts coZero which is a measured rawdata reading in a "zero" ppm CO environment
      ppmCOcal = (coRaw - coZero) * calibratedSlopeCO + calibratedInterceptCO;

      //Before saving data to SD card and printing to screen, check if the number is negative
      //Because there are no negative concentrations of CO, if the value is negative we can set it to zero
      if (ppmCOcal < 0) { ppmCOcal = 0; }

      //Serial.print statements for realtime readings in the serial monitor
      Serial.print("CO raw reading: ");
      Serial.println(coRaw); 

      Serial.print("CO PPM calibrated reading: ");
      Serial.println(ppmCOcal);

      //          *************************************** CO2 measuring ***************************************
      co2Raw = co2.measure();
      ppmCO2 = (co2Raw * calibratedSlopeCO2) + calibratedInterceptCO2;   // Concentration of CO2 in parts per million

      //Before saving data to SD card and printing to screen, check if the number is negative
      //Because there are no negative concentrations of CO2, if the value is negative we can set it to zero
      if (ppmCO2 < 0) { ppmCO2 = 0; }

      //Serial.print statements for realtime readings in the serial monitor
//      Serial.print("CO2 raw reading: ");
//      Serial.println(co2Raw); 
//
//      Serial.print("CO2 PPM calibrated reading: ");
//      Serial.println(ppmCO2);

      // we changed the ppmCO2 to coRaw just for testing purposes *******************************************
      printMeasureScreen(measureArrowPos, ppmCOcal, coRaw, pm2_5);
      break;
    ////////////////////////////////////////////////////////////////////////////////////
    // RECORD State: Write data to SD Card file and draw Wait/Measure/Record screen   //
    ////////////////////////////////////////////////////////////////////////////////////
    case record:
      measureWaitButtons();
      // I created a countSeconds function so that while the code is in this state it will be incrementing the second counter
      countSeconds();

      // we changed the ppmCO2 to coCal just for testing purposes *******************************************
      writeSuccess = writeToFile(rtc.now(), ppmCOcal, coRaw, ppmCO2, co2Raw, pm2_5, pm10);
            
      break;
    ////////////////////////////////////////////////////////////////////////////////////
    // ERROR State: Display error message                                             //
    ////////////////////////////////////////////////////////////////////////////////////
    case error:
      printError();
      SDCardSuccess = SD.begin();
      break;
    ////////////////////////////////////////////////////////////////////////////////////
    // OPTIONS State: Draw Options screen, keep track of options buttons/arrows       //
    ////////////////////////////////////////////////////////////////////////////////////
    case options:
      printOptions(optionsArrowPos);
      optionsButtons();
      break;
    default:
      break;
  }
}

/*Function controlling button functionality when inside measure/wait/record state
* The toggle button changes the arrow to either:
*     "Salir: no" stays in the states
*     "Salir: si" exits to the menu state
*/
void measureWaitButtons() {
  if (togglePushed) {
    if (measureArrowState) { 
      // Move arrow to "Salir: no"
      measureArrowState = false; 
      measureArrowPos = 65;
    } else { 
      // Move arrow to "Salir: si"
      measureArrowState = true;
      measureArrowPos = 85;
    }
    togglePushed = false;
  }
}

/*Function controlling button functionality when inside options state
* The toggle button moves the arrow on the screen
* The select button selects options wherever arrow is located
*/
void optionsButtons() {
  // Toggle through options when toggle button is pushed
  if (togglePushed) {
    togglePushed = false;
    if (optionsArrowPos == 20) { optionsArrowPos = 55; }
    else if (optionsArrowPos == 55) { optionsArrowPos = 92; }
    else if (optionsArrowPos == 92) { optionsArrowPos = 65; }
    else if (optionsArrowPos == 65) { optionsArrowPos = 85; }
    else if (optionsArrowPos == 85) { optionsArrowPos = 20; }
  }
  // Select an option if select buttons is pushed
  if (selectPushed) {
    selectPushed = false;
    if (optionsArrowPos == 20) { measureTime = 150; Serial.println("10 sec");}
    else if (optionsArrowPos == 55) { measureTime = (150*3) + (15*5); Serial.println("30 sec");}
    else if (optionsArrowPos == 92) { measureTime = (150*6) + (15*12); Serial.println("1 min");}
    else if (optionsArrowPos == 85) {  
      // Setup menu state for reentry
      Serial.println("Enter Menu State");
      optionsArrowPos = 20;
      menuArrowPos = 25;
      menuArrowState = false;
      clearScreen();
      currentState = menu; 
    }
  }
}

int countSeconds() {
  // Update the second counter if at least one second has passed
  unsigned long currentMillis = millis();
  if(currentMillis - lastSecondUpdate >= 1000) {
    lastSecondUpdate = currentMillis;
    secondsCounter++;
    Serial.print("Seconds Counting: ");
    Serial.println(secondsCounter);
  }
  return secondsCounter;
}

// Initializes all of the sensors
void initSensors(bool pmInit, bool coInit, bool co2Init, bool rtcInit, bool sdInit) {
  Serial.println("sdInit");
  bool sd = 0;
  if (sdInit) {
    sd = SD.begin();
  }
  Serial.println(sd);
  Serial.println("pmInit");
  if (pmInit) {
    pm.reset_measurement();               // Opens communication with the PM sensor
  }
  Serial.println("co2Init");
  if (co2Init) {
    bool test = co2.scd30.begin();                    // Intializes CO2 sensor
    co2.scd30.setMeasurementInterval(1);  // Fastest communication time with CO2 Sensor
  }
  Serial.println("coInit");
  if (coInit) {

    // There was never anything here.

  }
  Serial.println("rtcInit");
  if (rtcInit) {

    /*********** we have been having issues with the RTC keeping the correct date and time across days. We reset it and it is correct, then a couple weeks *************/
    /*********** later is incorrect again. This is why we decided to implement the countSeconds() function to act as a simple seconds counter that increments *************/
    /*********** whenever the system is in wait, measure and record states. *************/
    
    rtc.begin();  // Initializes real time clock, uses RTC library
    if (!rtc.isrunning()) {
      Serial.println("RTC is not running! Setting __DATE__ and __TIME__ to the date and time of last compile.");
      rtc.adjust(DateTime(F(__DATE__), F(__TIME__))); // Will set the real time clock to the time from the computer system
    }

    // If you wish to reset the date and time to the time of
    // last compile, uncomment this line of code. Otherwise,
    // It will only get reset if the RTC stops running e.g.
    // the battery dies, etc.

//         rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
}

/*
   Interrupt service routine:

   When the attachInterrupt() function is called in the sleep state,
   this function is specified as the interrupt service routine (isr).
   This means that when the RTC sends the interrupt signal, it calls
   this function right when the MCU wakes up and then continues
   execution where it left off (in the sleep state action).

*/
void isr() {
  Serial.println("MCU is now awake");
}

void isrToggle() {
  static unsigned long last_interrupt_time = 0;
  unsigned long interrupt_time = millis();
  // If interrupts come faster than 200ms, assume it's a bounce and ignore
  if (interrupt_time - last_interrupt_time > 200) {
    // Write new code here
    Serial.println("Pressed Toggle Button");
    togglePushed = true;
  }
  last_interrupt_time = interrupt_time;
}

void isrSelect() {
  static unsigned long last_interrupt_time = 0;
  unsigned long interrupt_time = millis();
  // If interrupts come faster than 200ms, assume it's a bounce and ignore
  if (interrupt_time - last_interrupt_time > 200) {
    // Write new code here
    Serial.println("Pressed Select Button");
    selectPushed = true;
  }
  last_interrupt_time = interrupt_time;
}
