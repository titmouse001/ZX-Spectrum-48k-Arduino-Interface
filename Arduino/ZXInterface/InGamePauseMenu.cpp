#include "IngamePauseMenu.h"

#include <Arduino.h>

#include "BufferManager.h"
#include "PacketBuilder.h"
#include "SdCardSupport.h"
#include "Z80Bus.h"
#include "draw.h"
#include "menu.h"
#include "pin.h"
#include "utils.h"
#include "z80bus.h"
#include "PacketTypes.h"

// ----------------------------------------------------------------------------------------------
// HARDWARE SYNC: /NMI TRIGGER LOGIC TO AVOID CORRUPTION
// ----------------------------------------------------------------------------------------------
// PCB UPDATE:   (double duty for ShiftRegClockPin)
// Nano A2 monitors /RD and /IORQ via a passive resistor logic gate (4.7K per
// line). This acts as a hardware AND gate for active-low signals: A2 only hits
// a Logic LOW when both Z80 lines are active, pinpointing the I/O Read cycle.
// WHY: Most games poll input (IN) when the stack is stable, so using NMI
// directly after seeing a I/O read means the stack is unlikely to be hijacked.
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


    // Reminder ... also see 'restoreZ80States' for extra code needed to make this work
    // ... this slowmo will probably need to use stack in screen memory , it's going to be firing atlot
    // and if any game's using all their stack space - this will find it and things will go bad!
    // bool slowMo_todo = true;
    // if (slowMo_todo) {
    //   pinModeFast(Pin::ShiftRegClockPin, INPUT);  // A2
    //   asm volatile(                  // continue when both /RD & /IORQ are low
    //       "1: sbic %[pin],2   \n\t"  // skip if LOW
    //       "rjmp 1b            \n\t"  // loop while HIGH
    //       "cbi  %[port],0     \n\t"  // NMI low
    //       "sbi  %[port],0     \n\t"  // NMI high (Pulsed /NMI)
    //       :
    //       : [pin] "I"(_SFR_IO_ADDR(PINC)), [port] "I"(_SFR_IO_ADDR(PORTC)));
    //   pinModeFast(Pin::ShiftRegClockPin, OUTPUT);  // back to normal clock use

    //   delay(20);  
    //   Z80Bus::setSnaRom();
    //   delay(1);
    //   Utils::storeZ80States();
    //   Utils::restoreZ80States();
    // }

  }
}

// Local constants
constexpr uint16_t PM_INITIAL_DELAY = 380;
constexpr uint16_t PM_REPEAT_RATE = 120;
constexpr uint16_t PAUSE_XPOS = 80;
constexpr uint8_t PAUSE_YPOS_START = 48;
constexpr uint8_t HIGHLIGHT_OFFSET = PAUSE_YPOS_START / 8; 

constexpr uint8_t getY(int8_t line) { return PAUSE_YPOS_START + (line * 8); }

__attribute__((optimize("-Os"))) 
uint8_t InGamePauseMenu::getSelectedMenuOption(uint8_t& selectedIndex) {
  Draw::text_P(PAUSE_XPOS, getY(-2), F("PAUSE MENU"));
  Draw::text_P(PAUSE_XPOS, getY(RESUME), F("Resume"));
  Draw::text_P(PAUSE_XPOS, getY(SAVE_SNA), F("*Save SNA"));
  Draw::text_P(PAUSE_XPOS, getY(SLOWMO), F("*SlowMo"));
  Draw::text_P(PAUSE_XPOS, getY(POKE), F("Poke"));
  Draw::text_P(PAUSE_XPOS, getY(SCREENSHOT), F("Screenshot"));
  Draw::text_P(PAUSE_XPOS, getY(MEM_VIEW), F("Mem View"));
  Draw::text_P(PAUSE_XPOS, getY(EXIT), F("Exit"));

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
      const uint16_t delayThreshold =
          isRepeating ? PM_REPEAT_RATE : PM_INITIAL_DELAY;
      if (currentButton != lastButton ||
          (now - lastActionTime) >= delayThreshold) {
        isRepeating =
            (currentButton ==
             lastButton);  // Becomes true only on subsequent held triggers
        lastActionTime = now;
        lastButton = currentButton;
        if (currentButton == Menu::BUTTON_ADVANCE && selectedIndex < EXIT) {
          Z80Bus::sendFillCommand(ZX_SCREEN_ATTR_ADDRESS_START +
                                      ((selectedIndex + HIGHLIGHT_OFFSET) * 32),
                                  32, COL::BRIGHT_BLACK_WHITE);
          selectedIndex++;
        } else if (currentButton == Menu::BUTTON_BACK &&
                   selectedIndex > RESUME) {
          Z80Bus::sendFillCommand(ZX_SCREEN_ATTR_ADDRESS_START +
                                      ((selectedIndex + HIGHLIGHT_OFFSET) * 32),
                                  32, COL::BRIGHT_BLACK_WHITE);
          selectedIndex--;
        } else if (currentButton == Menu::BUTTON_MENU) {
          return selectedIndex;
        }
      }
    }
    Z80Bus::sendFillCommand(ZX_SCREEN_ATTR_ADDRESS_START +
                                ((selectedIndex + HIGHLIGHT_OFFSET) * 32),
                            32, COL::CYAN_BLACK);
    delay(1);
  }
}

__attribute__((optimize("-Os"))) 
int32_t InGamePauseMenu::readNumericInput(uint8_t maxDigits, int xPos, int yPos, const char* name, uint16_t min, uint16_t max) {
  Draw::text(xPos, yPos, name);

  xPos += strlen(name) * (SmallFont::FNT_WIDTH + SmallFont::FNT_GAP);
  xPos = ((xPos+7)/8)*8;  //xPos = (xPos + 7) & ~7;

  uint8_t mark = BufferManager::getMark();
  char* buf = (char*)BufferManager::allocate(maxDigits + 1);

again:
  memset(buf, '_', maxDigits);
  //for (uint8_t i = 0; i < maxDigits; i++) buf[i] = '_';
  buf[maxDigits] = '\0';

  uint8_t len = 0;
  uint8_t flash = 0;
  uint8_t prevKey = 0;

  while (true) {
    uint8_t key = Z80Bus::getKeyboard();
    if (key != prevKey) {
      prevKey = key;
      if (key != 0) {
        if (key == 0x02) {          // DEL key
          if (len > 0) {
            buf[--len] = '_';
            flash = 0;              // snap visible
          }
        } 
        else if (key >= '0' && key <= '9') { 
          if (len < maxDigits) {
            buf[len++] = key;
            flash = 0;              // snap visible 
          }
        } 
        else if (key == 0x0D && buf[0] != '_') {     // ENTER key
          break;
        } 
        else if (key == 0x20) {     // CANCEL key (Space)
          BufferManager::freeToMark(mark);
          return -1;
        }
      }
    }

    uint8_t cursorIdx = (len < maxDigits) ? len : maxDigits - 1;
    char savedChar = buf[cursorIdx];
    // blinking cursor
    if (flash < 60) {  
      buf[cursorIdx] = 127;  
    } 
    else if (len < maxDigits) {  
      buf[cursorIdx] = ' '; 
    }

    Draw::text(xPos, yPos, buf);
    buf[cursorIdx] = savedChar;     
    if (++flash >= 120) flash = 0;
    delay(1);
  }

  // clr leftover cursor pattern
  for (uint8_t i = 0; i < maxDigits; i++) {
    if (buf[i] == '_') { buf[i]=' '; }
  }

//  if (len < maxDigits) buf[len] = ' ';   // clr any cursor
  Draw::text(xPos, yPos, buf);

  // Convert raw text strings to final value
  buf[len] = '\0';
  uint16_t value = 0;   
  // NOTE: hand rolled as 'strtoul' is pig - used around 658 bytes of extra flash
  for (uint8_t i = 0; i < len; i++) {
    value = (value * 10) + (buf[i] - '0');
  }
  
  while (Z80Bus::getKeyboard() != 0); // before retry!
  if ((value<min) || (value>max)) {
    Z80Bus::sendFillCommand(ZX_SCREEN_ATTR_ADDRESS_START+((yPos/8)*32) + (xPos/8), ((maxDigits*6)+7)/8, COL::FLASH_RED_WHITE);
    delay(1100);
    Z80Bus::sendFillCommand(ZX_SCREEN_ATTR_ADDRESS_START+((yPos/8)*32) + (xPos/8), ((maxDigits*6)+7)/8, COL::BRIGHT_BLACK_WHITE);
    goto again;
  }
  BufferManager::freeToMark(mark);
  return value;
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

    if (result == SAVE_SNA || result == SLOWMO) {
      Utils::clearScreen(COL::CYAN_BLACK);
      Draw::text_P((ZX_SCREEN_WIDTH_PIXELS / 2) - ((6 * 19) / 2),
                   (ZX_SCREEN_HEIGHT_PIXELS / 2), F("*NOT IMPLEMENTED YET"));
      delay(2000);
      Utils::clearScreen(COL::BRIGHT_BLACK_WHITE);
    }

    if (result == POKE) {
      Utils::clearScreen(COL::BRIGHT_BLACK_WHITE);
      Menu::waitForRelease();   
      Z80Bus::sendFillCommand( ZX_SCREEN_ATTR_ADDRESS_START + (32*(64/8)), 32*(64/8), COL::BLACK_WHITE);
      Draw::text_P( 128 - ((21*6)/2), 40, F("Enter address & value"));
      Draw::text_P( 0, 48, F("e.g. JetPac: Infinite Lives, Poke: 25014,0"));
      uint8_t x=144;
      uint8_t y=64+64;
      Z80Bus::sendFillCommand( ZX_SCREEN_ATTR_ADDRESS_START  + (((y+0 ) / 8) * 32) + (x / 8), 14, COL::BRIGHT_CYAN_BLACK);
      Z80Bus::sendFillCommand( ZX_SCREEN_ATTR_ADDRESS_START  + (((y+8 ) / 8) * 32) + (x / 8), 14, COL::MAGENTA_BLACK);
      Z80Bus::sendFillCommand( ZX_SCREEN_ATTR_ADDRESS_START  + (((y+16) / 8) * 32) + (x / 8), 14, COL::GREEN_BLACK);
      Draw::text_P( x, y+0, F("[Space] Abort"));
      Draw::text_P( x, y+8,  F("[Sym shft] Delete"));
      Draw::text_P( x, y+16,  F("[Enter] Apply"));
      Draw::text_P( 128, 192/2, F(","));
      Draw::text_P( 128+8, 192/2, F("___"));
    
      int32_t addr = readNumericInput(5, 64, 192 / 2, "Poke:",  0x4000, 0xffff);
      int32_t value = readNumericInput(3, 128, 192 / 2, ",",  0, 255);

      if (addr!=-1 && value!=-1) {
        Poke_Packet pkt(addr, value);
        Z80Bus::sendBytes(reinterpret_cast<uint8_t*>(&pkt),sizeof(Poke_Packet));
        Utils::clearScreen(COL::BRIGHT_BLACK_WHITE);
      }
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
                     (ZX_SCREEN_HEIGHT_PIXELS / 2) - 16,
                     F("Saving Screenshot"));

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
      Z80Bus::sendFillCommand(ZX_SCREEN_ATTR_ADDRESS_START, ZX_SCREEN_ATTR_SIZE,
                              COL::BLACK_BLACK);
      break;
    }
    if (result == EXIT) {
      Z80Bus::sendFillCommand(ZX_SCREEN_ATTR_ADDRESS_START, ZX_SCREEN_ATTR_SIZE,
                              COL::BLACK_BLACK);
      return true;  // go back to main game loader menu (exit game)
    }
  } while (true);

  Menu::waitForRelease();
  Utils::restoreScreen(SCRATCH_FILE);
  Utils::restoreZ80States();
  Utils::readJoystick();  // flush junk

  return false;
}