#include <Arduino.h>

#include "IngamePauseMenu.h"
//#include "Menu.h"
#include "utils.h"
#include "Z80Bus.h"

#include "draw.h"
#include "SdCardSupport.h"
#include "BufferManager.h"
//#include "CommandRegistry.h"
#include "z80bus.h"
#include "PacketBuilder.h"
#include "menu.h"
#include "utils.h"
//#include "PacketTypes.h"

#include "pin.h"


// ----------------------------------------------------------------------------------------------
// HARDWARE SYNC: /NMI TRIGGER LOGIC TO AVOID CORRUPTION
// ----------------------------------------------------------------------------------------------
// PCB UPDATE:   (double duty for ShiftRegClockPin)
// Nano A2 monitors /RD and /IORQ via a passive resistor logic gate (4.7K per line).
// This acts as a hardware AND gate for active-low signals: A2 only hits a Logic LOW when both
// Z80 lines are active, pinpointing the I/O Read cycle.
// WHY: Most games poll input (IN) when the stack is stable, so using NMI directly after seeing
// a I/O read means the stack is unlikely to be hijacked.
//
// Using Inline ASM for cycle-accurate timing.
// Equivalent to the following logic:
//   while (digitalRead(A2) == HIGH); // Wait for I/O Read cycle
//   NMI_LOW(); NMI_HIGH();           // Pulse NMI immediately
//
// ----------------------------------------------------------------------------------------------

__attribute__((optimize("-Os"))) 
void InGamePauseMenu::waitForUserExit() {

  Utils::readJoystick();  // flush junk (Z80 rd/wr port 0x1f shared)

  uint8_t buttonData;
  while (true) {
    buttonData = Utils::readJoystick();
    PORTD = buttonData & INPUT_MASK;  // Kempston joystick interface + pcb select button

    if (buttonData & INPUT_SELECT) {  // Pause Menu

      // Double Duty : Temporary Grab 'ShiftRegClockPin' as INPUT to monitor Z80's /RD & /IORQ lines
      pinModeFast(Pin::ShiftRegClockPin, INPUT);  // A2

      asm volatile(                // continue when both /RD & /IORQ are low
        "1: sbic %[pin],2   \n\t"  // skip if LOW
        "rjmp 1b            \n\t"  // loop while HIGH
        "cbi  %[port],0     \n\t"  // NMI low
        "sbi  %[port],0     \n\t"  // NMI high (Pulsed /NMI)
        :
        : [pin] "I"(_SFR_IO_ADDR(PINC)),
          [port] "I"(_SFR_IO_ADDR(PORTC)));

      pinModeFast(Pin::ShiftRegClockPin, OUTPUT);  // back to normal clock use
      
      //------------------------------------------------------------------------
      // At this point we are running of the stock ROM
      // Allow NMI routine time to reach it's idle loop (way more than needed)
      delay(10);            
      //------------------------------------------------------------------------
      // Swap out to use SNA ROM, it takes over with NOPs
      Z80Bus::setSnaRom();  
      //------------------------------------------------------------------------
       // Wait for Z80 to hit SNA ROM's '.IngameHook'
      delay(10);            
      //------------------------------------------------------------------------

      Utils::storeZ80States();

    //  pinModeFast(Pin::ShiftRegClockPin, OUTPUT);  // back to normal clock use
      Utils::saveScreen(SCRATCH_FILE);
      //while ((Utils::readJoystick() & INPUT_SELECT)) {}
      Menu::waitForRelease();

      uint8_t result = pauseMenu();

      if (result == 5) {
        Utils::clearScreen(COL::BLACK_WHITE);
        Utils::viewSpeccyMemory();
      }


      if (result == 4) {  // Screenshot
        Utils::clearScreen(COL::BLACK_WHITE);
        Draw::text_P((ZX_SCREEN_WIDTH_PIXELS / 2) - ((6 * 17) / 2), (ZX_SCREEN_HEIGHT_PIXELS / 2) - 4, F("Saving Screenshot"));
        while (Utils::exportScreenshot() == false) {
          // SD card removed
          Utils::clearScreen(COL::FLASH_BLACK_RED);
          Draw::text_P((ZX_SCREEN_WIDTH_PIXELS / 2) - ((6 * 10) / 2), (ZX_SCREEN_HEIGHT_PIXELS / 2) - 4, F("SD Missing"));
          do {  
              delay(20);
          } while (!SdCardSupport::init());  // keep looking
          Utils::clearScreen(COL::BLACK_WHITE);
        }
        //while ((Menu::getButton() == Menu::BUTTON_NONE)) {}
       // while ((Menu::getButton() != Menu::BUTTON_NONE)) {}
        Menu::waitForAnyKey();
      }

      Z80Bus::sendFillCommand(ZX_SCREEN_ATTR_ADDRESS_START, ZX_SCREEN_ATTR_SIZE, COL::BLACK_BLACK);

      if (result == 6) { break; }                          // back to main menu
      //while ((Menu::getButton() != Menu::BUTTON_NONE)) {}  // wait for button let go
      Menu::waitForRelease();

      Utils::restoreScreen(SCRATCH_FILE);

      Utils::restoreZ80States();
      Utils::readJoystick();  // flush junk
    }
  }
}

// Local constants
constexpr uint16_t PM_INITIAL_DELAY = 380;
constexpr uint16_t PM_REPEAT_RATE = 120;
constexpr uint16_t PAUSE_XPOS = 80;
constexpr uint8_t PAUSE_YPOS_START = 48;
constexpr uint8_t HIGHLIGHT_OFFSET = PAUSE_YPOS_START / 8;  // align highlighting with the "Resume" line
inline uint8_t getY(int8_t line) {
  return PAUSE_YPOS_START + (line * 8);
}

__attribute__((optimize("-Os")))
uint8_t InGamePauseMenu::pauseMenu() {

  Utils::clearScreen(COL::BLACK_WHITE);
  Draw::text_P(PAUSE_XPOS, getY(-2), F("PAUSE MENU"));
  Draw::text_P(PAUSE_XPOS, getY(0), F("Resume"));                 // 0
  Draw::text_P(PAUSE_XPOS, getY(1), F("<todo>Save SNA"));         // 1
  Draw::text_P(PAUSE_XPOS, getY(2), F("<todo>SlowMo [normal]"));  // 2
  Draw::text_P(PAUSE_XPOS, getY(3), F("<todo>Cheats"));           // 3
  Draw::text_P(PAUSE_XPOS, getY(4), F("Screenshot"));             // 4
  Draw::text_P(PAUSE_XPOS, getY(5), F("Mem View"));               // 5
  Draw::text_P(PAUSE_XPOS, getY(6), F("Exit"));                   // 6

  uint8_t index = 0;
  uint16_t oldHighlightAddress = 0;
  unsigned long lastActionTime = 0;
  bool isRepeating = false;
  Menu::Button_t lastButton = Menu::BUTTON_NONE;
  while (true) {
    Utils::highlightSelection(index + HIGHLIGHT_OFFSET, 0, oldHighlightAddress);
    Menu::Button_t currentButton = Menu::getButton();
    if (currentButton == Menu::BUTTON_NONE) {
      lastButton = Menu::BUTTON_NONE;
      isRepeating = false;
    } else {
      const unsigned long now = millis();
      const uint16_t delayThreshold = isRepeating ? PM_REPEAT_RATE : PM_INITIAL_DELAY;
      if (currentButton != lastButton || (now - lastActionTime) >= delayThreshold) {
        isRepeating = (currentButton == lastButton);  // Becomes true only on subsequent held triggers
        lastActionTime = now;
        lastButton = currentButton;
        if (currentButton == Menu::BUTTON_ADVANCE && index < 6) {
          index++;
        } else if (currentButton == Menu::BUTTON_BACK && index > 0) {
          index--;
        } else if (currentButton == Menu::BUTTON_MENU) {
          return index;
        }
      }
    }
    delay(1);
  }
}