#include <Arduino.h>

#include "IngamePauseMenu.h"
#include "utils.h"
#include "Z80Bus.h"
#include "draw.h"
#include "SdCardSupport.h"
#include "BufferManager.h"
#include "z80bus.h"
#include "PacketBuilder.h"
#include "menu.h"
#include "utils.h"
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
// ----------------------------------------------------------------------------------------------


// waitForUserExit: Processes joystick
__attribute__((optimize("-Os"))) 
void InGamePauseMenu::waitForUserExit() {

  Utils::readJoystick();  // flush junk (Z80 rd/wr port 0x1f shared)
  while (true) {
    const uint8_t buttonData = Utils::readJoystick();

    // Each frame the 'PORTD' Port register is updated with fresh joystick data.
    // (Kempston joy data hits data bus on: IORQ,RD,A7; all LOW)
    PORTD = buttonData & INPUT_MASK;  // Kempston joystick interface + pcb select button
    if (buttonData & INPUT_SELECT) {  
      if (process()) {
        break;  // back to game loader menu
      }
    }

  }
}

// Local constants
constexpr uint16_t PM_INITIAL_DELAY = 380;
constexpr uint16_t PM_REPEAT_RATE = 120;
constexpr uint16_t PAUSE_XPOS = 80;
constexpr uint8_t PAUSE_YPOS_START = 48;
constexpr uint8_t HIGHLIGHT_OFFSET = PAUSE_YPOS_START / 8;  // align highlighting with the "Resume" line

constexpr uint8_t getY(int8_t line) { return PAUSE_YPOS_START + (line * 8); }

__attribute__((optimize("-Os")))
uint8_t InGamePauseMenu::getSelectedMenuOption(uint8_t& selectedIndex) {

  // Z80Bus::sendFillCommand(ZX_SCREEN_ATTR_ADDRESS_START + ((getY(-2)/8)*32), 32, COL::BLUE_WHITE);
  Draw::text_P(PAUSE_XPOS, getY(-2), F("PAUSE MENU"));
  Draw::text_P(PAUSE_XPOS, getY(RESUME), F("Resume"));          
  Draw::text_P(PAUSE_XPOS, getY(SAVE_SNA), F("*Save SNA"));      
  Draw::text_P(PAUSE_XPOS, getY(SLOWMO), F("*SlowMo")); 
  Draw::text_P(PAUSE_XPOS, getY(CHEATS), F("*Cheats"));          
  Draw::text_P(PAUSE_XPOS, getY(SCREENSHOT), F("Screenshot"));  
  Draw::text_P(PAUSE_XPOS, getY(MEM_VIEW), F("Mem View"));      
  Draw::text_P(PAUSE_XPOS, getY(EXIT), F("Exit"));              

  //uint8_t index = 0;
  //uint16_t oldHighlightAddress = 0;
  unsigned long lastActionTime = 0;
  bool isRepeating = false;
  Menu::Button_t lastButton = Menu::BUTTON_NONE;
  while (true) {
    Utils::show5VoltRailStatus();

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
        if (currentButton == Menu::BUTTON_ADVANCE && selectedIndex < EXIT) {
          Z80Bus::sendFillCommand(ZX_SCREEN_ATTR_ADDRESS_START + ((selectedIndex + HIGHLIGHT_OFFSET)*32), 32, COL::BRIGHT_BLACK_WHITE);
          selectedIndex++;
        } else if (currentButton == Menu::BUTTON_BACK && selectedIndex > RESUME) {
          Z80Bus::sendFillCommand(ZX_SCREEN_ATTR_ADDRESS_START + ((selectedIndex + HIGHLIGHT_OFFSET)*32), 32, COL::BRIGHT_BLACK_WHITE);
          selectedIndex--;
        } else if (currentButton == Menu::BUTTON_MENU) {
          return selectedIndex;
        }
      }
    }
    Z80Bus::sendFillCommand(ZX_SCREEN_ATTR_ADDRESS_START + ((selectedIndex + HIGHLIGHT_OFFSET)*32), 32, COL::CYAN_BLACK);
    delay(1);
  }
}

// Using Inline ASM for cycle-accurate timing.
// Equivalent to the following logic:
//   while (digitalRead(A2) == HIGH); // Wait for I/O Read cycle
//   NMI_LOW(); NMI_HIGH();           // Pulse NMI immediately
//
bool InGamePauseMenu::process() {
  uint8_t selectedIndex = 0;

  // Double Duty : Temporary Grab 'ShiftRegClockPin' as INPUT to monitor Z80's
  // /RD & /IORQ lines
  pinModeFast(Pin::ShiftRegClockPin, INPUT);  // A2

  asm volatile(                  // continue when both /RD & /IORQ are low
      "1: sbic %[pin],2   \n\t"  // skip if LOW
      "rjmp 1b            \n\t"  // loop while HIGH
      "cbi  %[port],0     \n\t"  // NMI low
      "sbi  %[port],0     \n\t"  // NMI high (Pulsed /NMI)
      :
      : [pin] "I"(_SFR_IO_ADDR(PINC)), [port] "I"(_SFR_IO_ADDR(PORTC)));

  pinModeFast(Pin::ShiftRegClockPin, OUTPUT);  // back to normal clock use

  //------------------------------------------------------------------------
  // At this point we are running of the stock ROM
  // Allow NMI routine time to reach it's idle loop
  delay(10);  // (way more than needed)
  //------------------------------------------------------------------------
  // Swap out to use SNA ROM, it takes over with NOPs
  Z80Bus::setSnaRom();
  //------------------------------------------------------------------------
  // Wait for Z80 to hit SNA ROM's '.IngameHook'
  delay(10);  // (way more than needed)
  //------------------------------------------------------------------------

  Utils::storeZ80States();
  // TODO ? screen data - good fit to compress to SD card?
  //  would it be quicker that way... probably for most screens
  Utils::saveScreen(SCRATCH_FILE);
  Menu::waitForRelease();

  do {
    Utils::clearScreen(COL::BRIGHT_BLACK_WHITE);
    uint8_t result = getSelectedMenuOption(selectedIndex);

    if (result == SAVE_SNA || result == SLOWMO || result == CHEATS) {
      Utils::clearScreen(COL::CYAN_BLACK);
      Draw::text_P((ZX_SCREEN_WIDTH_PIXELS / 2) - ((6 * 19) / 2),
                   (ZX_SCREEN_HEIGHT_PIXELS / 2), F("*NOT IMPLEMENTED YET"));
      delay(2000);
      Utils::clearScreen(COL::BRIGHT_BLACK_WHITE);
    }

    if (result == MEM_VIEW) {
      Utils::clearScreen(COL::BRIGHT_BLACK_WHITE);
      Utils::viewSpeccyMemory();
    }

    if (result == SCREENSHOT) {  // Screenshot
      bool savedSuccessfully = false;

      do {
        Utils::clearScreen(COL::BRIGHT_BLACK_WHITE);
        Draw::text_P((ZX_SCREEN_WIDTH_PIXELS / 2) - ((6 * 17) / 2),
                     (ZX_SCREEN_HEIGHT_PIXELS / 2) - 16, F("Saving Screenshot"));

        savedSuccessfully = Utils::exportScreenshot();

        if (!savedSuccessfully) {
          if (!SdCardSupport::isInserted()) {
            Utils::waitForSDCard_Blocking(
                true);  // shows message "INSERT SD CARD"
          } else {
            Draw::text_P((ZX_SCREEN_WIDTH_PIXELS / 2) - ((6 * 11) / 2),
                         (ZX_SCREEN_HEIGHT_PIXELS / 2) + 16, F("SAVE FAILED"));
            delay(2000);
          }
        }
      } while (!savedSuccessfully);

      Menu::waitForAnyKey();
    }

    if (result == Resume) {
      Z80Bus::sendFillCommand(ZX_SCREEN_ATTR_ADDRESS_START, ZX_SCREEN_ATTR_SIZE, COL::BLACK_BLACK);
      break;
    }
    if (result == EXIT) {
      Z80Bus::sendFillCommand(ZX_SCREEN_ATTR_ADDRESS_START, ZX_SCREEN_ATTR_SIZE, COL::BLACK_BLACK);
      return true;  // go back to main game loader menu (exit game)
    }
  } while (true);

  Menu::waitForRelease();
  Utils::restoreScreen(SCRATCH_FILE);
  Utils::restoreZ80States();
  Utils::readJoystick();  // flush junk

  return false;
}