// -------------------------------------------------------------------------------------
//  "Arduino Hardware Interface for ZX Spectrum" - 2023/24 P.Overy
// -------------------------------------------------------------------------------------
// This software uses the Arduino's ATmega328P range (Nano, Pro mini ... )
// for example here's the Nano specs:-
// - 32K bytes of in-system self-programmable flash program memory
// - 1K bytes EEPROM
// - 2K bytes internal SRAM
// IMPORTANT: Do not modify PORTB directly without preserving the clock/crystal bits.
// -------------------------------------------------------------------------------------
// SPEC OF SPECCY H/W PORTS HERE: - https://mdfs.net/Docs/Comp/Spectrum/SpecIO
// good links for Arduino info  :   https://devboards.info/boards/arduino-nano
//                                  https://arduino.stackexchange.com/questions/30968/how-do-interrupts-work-on-the-arduino-uno-and-similar-boards
// SNAPSHOT FILE HEADER (.sna files)
//  REG_I	 =00, REG_HL'=01, REG_DE'	=03, REG_BC' =05	(.sna file header)
//  REG_AF'=07, REG_HL =09, REG_DE	=11, REG_BC	 =13  (        27 bytes)
//  REG_IY =15, REG_IX =17, REG_IFF =19, REG_R	 =20
//  REG_AF =21, REG_SP =23, REG_IM	=25, REG_BDR =26
// -------------------------------------------------------------------------------------

// Note about slow roms giving unextpected behaviour.

#include <Arduino.h>
#include <Wire.h>  //  I2C devices
#include "SSD1306AsciiAvrI2c.h"  //  I2C displays, oled

#include "utils.h"
#include "smallfont.h"
#include "pin.h"
#include "ScrSupport.h"
#include "Z80Bus.h"
#include "Buffers.h"
#include "sdSupport.h"
#include "Draw.h"

// --------------------------------
#define SERIAL_DEBUG 0

#if SERIAL_DEBUG
  #warning "*** SERIAL_DEBUG is enabled"
  // Pins D0/D1 are shared between Serial RX/TX and the Z80 data bus.
  // This configuration will break Z80 communication. Enable SERIAL_DEBUG only for 
  // testing purposes and disable it in production builds.
  #include <SPI.h>  // Serial
#endif
// --------------------------------

#define VERSION ("0.14")

// SD card support
SdFat32 sd;
FatFile root;
FatFile file;
char fileName[65];
// ----------------------------------------------------------------------------------
// OLED support
#define I2C_ADDRESS 0x3C  // 0x3C or 0x3D
SSD1306AsciiAvrI2c oled;
bool haveOled = false;
// ----------------------------------------------------------------------------------

enum { BUTTON_NONE,
       BUTTON_DOWN,
       BUTTON_SELECT,
       BUTTON_UP,
       BUTTON_DOWN_REFRESH_LIST,
       BUTTON_UP_REFRESH_LIST };

unsigned long lastButtonPress = 0;     // Store the last time a button press was processed
unsigned long buttonDelay = 300;       // Initial delay between button actions in milliseconds
unsigned long lastButtonHoldTime = 0;  // Track how long the button has been held
bool buttonHeld = false;               // Track if the button is being held
uint16_t oldHighlightAddress = 0;      // Last highlighted screen position for clearing away
uint16_t currentFileIndex = 0;         // The currently selected file index in the list
uint16_t startFileIndex = 0;           // The start file index of the current viewing window

void setup() {

#if (SERIAL_DEBUG==1)
    Serial.begin(9600);
    while (! Serial) {};
    Serial.println("DEBUG MODE - BREAKS Z80 TRANSFERS - ARDUINO SIDE DEBUGGING ONLY");
#endif

  Z80Bus::setupNMI();
  Z80Bus::setupPins();
  Utils::setupJoystick();

  // ---------------------------------------------------------------------------
  // Revert to stock ROM at start-up (Select button held)
  if (getAnalogButton(analogRead(Pin::BUTTON_PIN)) == BUTTON_SELECT) {
    Z80Bus::bankSwitchStockRom();
    Z80Bus::resetZ80();
    // TO DO -  allow user to go back to the sna loading mode  ???
    while(true) { delay(50); }
  }
  // -----------------------------------------------------------------------------
}

void loop() {

  Z80Bus::resetToSnaRom();
  Z80Bus::fillScreenAttributes(Utils::Ink7Paper0);   // setup the whole screen

  uint16_t totalFiles = getFileCount();

  if (getAnalogButton(analogRead(Pin::BUTTON_PIN)) == BUTTON_DOWN) {
    ScrSupport::DemoScrFiles(root, file, packetBuffer);
  }

  Draw::fileList(startFileIndex);
  Z80Bus::highlightSelection(currentFileIndex, startFileIndex, oldHighlightAddress);

   //Wait here until SELECT is released, so we don't retrigger on the same press.
  while ( getAnalogButton( analogRead(Pin::BUTTON_PIN) ) == BUTTON_SELECT ) {
    delay(1); // do nothing
  }
    
  // ***************************************
  // *** Main loop - file selection menu ***
  // ***************************************
  while (true) {
    unsigned long start = millis();
    byte action = doMenu(totalFiles);
    if (action == BUTTON_SELECT) {
      break;
    }
    Utils::frameDelay(start);
  }

  const uint16_t address = 0x4004;
  packetBuffer[0] = 'S';
  packetBuffer[1] = (uint8_t)(address>>8);
  packetBuffer[2] = (uint8_t)(address&0xFF);
  Z80Bus::sendBytes(packetBuffer, 3);
  Z80Bus::waitRelease_NMI(); 

  //-----------------------------------------
  // Read the Snapshots 27-byte header ahead of time
  if (file.available()) {
    head27_Execute[0] = 'E';
    byte bytesReadHeader = (byte)file.read(&head27_Execute[0 + 1], 27);
    if (bytesReadHeader != 27) { debugFlash(3000); }

    // TODO: show error message
  } 
  //-----------------------------------------
  // Copy snapshot data into Speccy RAM (0x4000-0xFFFF)
  uint16_t currentAddress = 0x4000;  //starts at beginning of screen
  while (file.available()) {
    byte bytesRead = (byte)file.read(&packetBuffer[SIZE_OF_HEADER], PAYLOAD_BUFFER_SIZE);
    START_UPLOAD_COMMAND(packetBuffer, 'G', bytesRead);
    END_UPLOAD_COMMAND(packetBuffer, currentAddress);
    Z80Bus::sendBytes(packetBuffer, SIZE_OF_HEADER + (uint16_t)bytesRead);
    currentAddress += packetBuffer[HEADER_PAYLOADSIZE];
  }
  file.close();
  //-----------------------------------------
  // Command Speccy to wait for the next screen frefresh
  packetBuffer[0] = 'W';
  Z80Bus::sendBytes(packetBuffer, 1);
  //-----------------------------------------
  // The ZX Spectrum's maskable interrupt (triggered by the 50Hz screen refresh) will self release
  // the CPU from halt mode during the vertical blanking period.
  Z80Bus::waitHalt();  // wait for speccy to triggered the 50Hz maskable interrupt
  //-----------------------------------------
  Z80Bus::sendSnaHeader(&head27_Execute[0]);  // sends 27 commands allowing Z80 to patch together the Sna files saved register states.
  //-----------------------------------------
  // At this point the speccy is running code in screen memory to safely swap ROM banks.
  // A final HALT is given just before jumping back to the real snapshot code/game.
  Z80Bus::waitRelease_NMI();  // signal NMI line for the speccy to resume
  //-----------------------------------------
  // T-state duration:  1/3.5MHz = 0.2857μs
  // NMI Z80 timings -  Push PC onto Stack: ~10 or 11 T-states
  //                    RETN Instruction:    14 T-states.
  //                    Total:              ~24 T-states
  delayMicroseconds(7);  // (24×0.2857=6.857) wait for z80 to return to the main code.

  Z80Bus::bankSwitchStockRom();

  // At ths point rhe Spectrum's external ROM has switched to using the 16K stock ROM.
  // Wait for HALT line to return HIGH again (shows Z80 has resumed)
  while ( (PINB & (1 << PINB0)) == 0 ) {};

  do {
    unsigned long startTime = millis();
    PORTD = Utils::readJoystick();  // send to the Z80 data lines (Kempston standard)
    Utils::frameDelay(startTime);
  } while (getAnalogButton(analogRead(Pin::BUTTON_PIN)) != BUTTON_SELECT);

}

//-------------------------------------------------
// Section: Menu / Input Support
//-------------------------------------------------

byte doMenu(uint16_t totalFiles) {
  byte btn = readButtons(totalFiles);
  switch (btn) {
    case BUTTON_SELECT:
      Sd::openFileByIndex(currentFileIndex);
      break;
    case BUTTON_UP:
    case BUTTON_DOWN:
      Z80Bus::highlightSelection(currentFileIndex, startFileIndex, oldHighlightAddress);
      break;

    case BUTTON_UP_REFRESH_LIST:
    case BUTTON_DOWN_REFRESH_LIST:
      Draw::fileList(startFileIndex);
      Z80Bus::highlightSelection(currentFileIndex, startFileIndex, oldHighlightAddress);
      break;

    default:
      break;
  }
  return btn;
}

byte readButtons(uint16_t totalFiles) {
  // ANALOGUE VALUES ARE AROUND... 1024,510,324,22
  byte ret = BUTTON_NONE;
  int but = analogRead(Pin::BUTTON_PIN);
  uint8_t joy = Utils::readJoystick();  // Kempston standard, "000FUDLR" bit pattern

  if (getAnalogButton(but) || (joy & B00011100)) {
    unsigned long currentMillis = millis();     // Get the current time
    if (buttonHeld && (currentMillis - lastButtonHoldTime >= buttonDelay)) {
      buttonDelay = max(40, buttonDelay - 20);  // speed-up file selection over time
      lastButtonHoldTime = currentMillis;
    }

    if ((!buttonHeld) || (currentMillis - lastButtonPress >= buttonDelay)) {
      if (but < (100 + 22) || (joy & B00000100)) {  // 1st button
        if (currentFileIndex < startFileIndex + SCREEN_TEXT_ROWS - 1 && currentFileIndex < totalFiles - 1) {
          currentFileIndex++;
          ret = BUTTON_DOWN;
        } else if (startFileIndex + SCREEN_TEXT_ROWS < totalFiles) {
          startFileIndex += SCREEN_TEXT_ROWS;
          currentFileIndex = startFileIndex;
          ret = BUTTON_DOWN_REFRESH_LIST;
        }
      } else if (but < (100 + 324) || (joy & B00010000)) {  // 2nd button
        ret = BUTTON_SELECT;
      } else if (but < (100 + 510) || (joy & B00001000)) {  // 3rd button
          if (currentFileIndex > startFileIndex) {
            currentFileIndex--;
             ret = BUTTON_UP;
          } else if (startFileIndex >= SCREEN_TEXT_ROWS) {
            startFileIndex -= SCREEN_TEXT_ROWS;
            currentFileIndex = startFileIndex + SCREEN_TEXT_ROWS - 1;
            ret = BUTTON_UP_REFRESH_LIST;
          }
      }
      buttonHeld = true;
      lastButtonPress = currentMillis;
      lastButtonHoldTime = currentMillis;
    }
  } else {   /* no button is pressed - reset delay */
    buttonHeld = false;
    buttonDelay = 300;  // Reset to the initial delay
  }
  return ret;
}

uint8_t getAnalogButton(int but) {
  static constexpr int BUTTON_UP_THRESHOLD = 122;
  static constexpr int BUTTON_SELECT_THRESHOLD = 424;
  static constexpr int BUTTON_DOWN_THRESHOLD = 610;

  if (but < BUTTON_UP_THRESHOLD) {
    return BUTTON_UP;
  } else if (but < BUTTON_SELECT_THRESHOLD) {
    return BUTTON_SELECT;
  } else if (but < BUTTON_DOWN_THRESHOLD) {
    return BUTTON_DOWN;
  }
  return BUTTON_NONE;
}

//-------------------------------------------------
/*
 * getFileCount: Returns number of SNA files on sd card 
 * File Limit=65535
 */
uint16_t getFileCount() {
  Sd::Status status;
  uint16_t totalFiles = 0;

  do {  // Keep trying - maybe sd card is missing
    status = Sd::init(totalFiles);
    if (status != Sd::Status::OK) {
      switch (status) {
        case Sd::Status::OK: 
          break;  // do nothing
        case Sd::Status::NO_FILES:
          Draw::text(80, 90, "NO FILES FOUND");
          break;
        case Sd::Status::NO_SD_CARD:
          Draw::text(80, 90, "INSERT SD CARD");
          break;
      }
      delay(1000/50);
    }
  } while (totalFiles == 0);

  return totalFiles;
}

//-------------------------------------------------
// Section: Debug Supprt
//-------------------------------------------------

void debugFlash(int flashspeed) {
  // If we get here it's faital
  file.close();
  root.close();
  sd.end();
  while (1) {
    pinMode(Pin::ledPin, OUTPUT);
    digitalWrite(Pin::ledPin, HIGH);
    delay(flashspeed);
    digitalWrite(Pin::ledPin, LOW);
    delay(flashspeed);
  }
}
