#include "IngamePauseMenu.h"

#include <Arduino.h>

#include "BufferManager.h"
#include "SdCardSupport.h"
#include "Z80Bus.h"
#include "draw.h"
#include "menu.h"
#include "pin.h"
#include "utils.h"
#include "z80bus.h"
#include "PacketTypes.h"

constexpr uint16_t PM_INITIAL_DELAY = 380;
constexpr uint16_t PM_REPEAT_RATE = 120;
constexpr uint8_t PAUSE_XPOS = 80;
constexpr uint8_t PAUSE_YPOS_START = 48;
constexpr uint8_t HIGHLIGHT_OFFSET = PAUSE_YPOS_START / 8; 
constexpr uint8_t getY(int8_t line) { return PAUSE_YPOS_START + (line * 8); }

// ----------------------------------------------------------------------------------------------
// HARDWARE SYNC: /NMI TRIGGER LOGIC TO AVOID CORRUPTION
// ----------------------------------------------------------------------------------------------
// PCB UPDATE:   (double duty for ShiftRegClockPin)
// Nano A2 monitors /RD and /IORQ via a passive resistor logic gate (4.7K per
// line). This acts as a hardware AND gate for active-low signals: A2 only hits
// a Logic LOW when both Z80 lines are active, pinpointing the I/O Read cycle.
// WHY: Most games poll input (IN) when the stack is stable, so using NMI
// directly after seeing a I/O read means the stack is unlikely to be hijacked.
//
// ----------------------------------------------------------------------------------------------
// waitForUserExit: Processes joystick / PCB button
//    Each frame the 'PORTD' Port register is updated with fresh joystick data.
//    (Kempston joy data hits data bus on: IORQ,RD,A7; all LOW)
// ----------------------------------------------------------------------------------------------
void InGamePauseMenu::waitForUserExit(uint8_t borderColour) {

  Utils::readJoystick();  // flush junk (Z80 rd/wr port 0x1f shared)
  while (true) {
    uint8_t buttonData = Utils::readJoystick();

    // remap 2nd fire to jump 
// todo - needs config!
    if (buttonData & INPUT_FIRE2) {
      buttonData |= 0b00001000;  // up  (BITS: XsfFUDLR)
    }

    PORTD = buttonData & INPUT_MASK; 

    if (buttonData & INPUT_SELECT) {
      if (process(borderColour)) {
        break;  // back to game loader menu
      }
    }
  }
}

uint8_t InGamePauseMenu::getSelectedMenuOption_Blocking(uint8_t& selectedIndex) {
  Draw::text_P(PAUSE_XPOS, getY(-2), F("PAUSE MENU"));
  Draw::text_P(PAUSE_XPOS, getY(RESUME), F("Resume"));
  Draw::text_P(PAUSE_XPOS, getY(SAVE_SNA), F("Save"));
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
    Utils::delay16(1);
  }
}

bool InGamePauseMenu::process(uint8_t borderColour) {
  uint8_t selectedIndex = 0;

  // Double Duty : Temporary Grab 'ShiftRegClockPin' as INPUT to monitor Z80's combined /RD AND /IORQ lines
  pinModeFast(Pin::ShiftRegClockPin, INPUT);  // A2

  // Using Inline ASM for cycle-accurate timing.
  // Equivalent to:
  //   while (digitalRead(A2) == HIGH); // Wait for external signal on A2
  //   NMI_LOW(); NMI_HIGH();           // Pulse NMI on A0 immediately
  asm volatile(
      "1: sbic %[pin],2   \n\t"  // Check A2 (PC2); skip next instruction if LOW
      "rjmp 1b            \n\t"  // Jump back to '1' (Loop while A2 is HIGH)
      "cbi  %[port],0     \n\t"  // Drive A0 (PC0) LOW (Start NMI pulse)
      "sbi  %[port],0     \n\t"  // Drive A0 (PC0) HIGH (End NMI pulse, 2 cycles wide)
      :
      : [pin] "I"(_SFR_IO_ADDR(PINC)), [port] "I"(_SFR_IO_ADDR(PORTC)));

  pinModeFast(Pin::ShiftRegClockPin, OUTPUT);  // back to normal clock use

  //------------------------------------------------------------------------
  // At this point we are running of the stock ROM
  // Allow NMI routine time to reach it's idle loop
  Utils::delay16(10);  // (way more than needed)
  //------------------------------------------------------------------------
  // Swap out to use SNA ROM, it takes over with NOPs
  Z80Bus::setSnaRom();
  //------------------------------------------------------------------------
  // Wait for Z80 to hit SNA ROM's '.IngameHook'
  Utils::delay16(10);  // (way more than needed)
  //------------------------------------------------------------------------

  // storeZ80States allocates memory, and restoreZ80States frees it.
  // If exiting without calling restoreZ80States, free structs marker manually.
  Z80Registers* z80Registers = Utils::storeZ80States();
  z80Registers->borderCol = borderColour; // used the original snapshot value, we can't extract this at game time!

  //Utils::saveScreen(SCRATCH_FILE);
//  Utils::saveMemory(SCRATCH_FILE, ZX_SCREEN_ADDRESS_START, 1024U*48);// ZX_SCREEN_BITMAP_SIZE + ZX_SCREEN_ATTR_SIZE); 
//  Utils::saveHeader(z80Registers);

  Utils::saveMemory(SCRATCH_FILE, ZX_SCREEN_ADDRESS_START, ZX_SCREEN_BITMAP_SIZE + ZX_SCREEN_ATTR_SIZE); 
  
  Menu::waitForRelease();

  do {
    Utils::clearScreen(COL::BRIGHT_BLACK_WHITE);
    uint8_t result = getSelectedMenuOption_Blocking(selectedIndex);

    Utils::clearScreen(COL::BRIGHT_BLACK_WHITE);
    Menu::waitForRelease();   

    if (result == SAVE_SNA ) { 
      Utils::saveSnapshot(z80Registers);
      // clear a thin window for text
      Z80Bus::sendFillCommand( ZX_SCREEN_ATTR_ADDRESS_START + (((ZX_SCREEN_HEIGHT_PIXELS)/2/8) * (ZX_SCREEN_WIDTH_BYTES)), ZX_SCREEN_WIDTH_BYTES, COL::BRIGHT_BLACK_WHITE);
      for (uint8_t i=0; i<8; i++) {
        Z80Bus::sendFillCommand(Utils::zx_spectrum_screen_address((ZX_SCREEN_HEIGHT_PIXELS/2) + i), ZX_SCREEN_WIDTH_BYTES, 0);
      }

      Draw::text_P((ZX_SCREEN_WIDTH_PIXELS / 2) - ((6 * 5) / 2), (ZX_SCREEN_HEIGHT_PIXELS / 2), F("SAVED"));
      Utils::delay16(1500);
    }
    if (result == POKE) {
      handlePokeMenu();
    }
    if (result == MEM_VIEW) {
      Utils::viewSpeccyMemory();
    }
    if (result == SCREENSHOT) {  // Screenshot
      handleScreenshotMenu();
    }
    if (result == Resume) {
      Z80Bus::sendFillCommand(ZX_SCREEN_ATTR_ADDRESS_START, ZX_SCREEN_ATTR_SIZE, COL::BLACK_BLACK);
      break;
    }
    if (result == EXIT) {
      Z80Bus::sendFillCommand(ZX_SCREEN_ATTR_ADDRESS_START, ZX_SCREEN_ATTR_SIZE, COL::BLACK_BLACK);
      BufferManager::freeToMark(z80Registers->AllocMark);
      return true;  // go back to main game loader menu (exit game)
    }
  } while (true);


// // -------------------- DEBUG
// Utils::clearScreen(COL::BRIGHT_MAGENTA_BLACK);
// if (z80Registers->i == 0xfe) {  // Zynaps "I" value
//    Draw::text_P(80, 90, F("I=fe, IM 2"));
// }else {
//    Draw::text_P(80, 90, F("I=3f, IM 1"));
// }
// delay(1000);
// // ------------------------------


  Menu::waitForRelease();

  Utils::loadMemory(SCRATCH_FILE, ZX_SCREEN_ADDRESS_START, ZX_SCREEN_BITMAP_SIZE + ZX_SCREEN_ATTR_SIZE);
  //Utils::loadMemory(SCRATCH_FILE, ZX_SCREEN_ADDRESS_START, 1024U*48) ; //ZX_SCREEN_BITMAP_SIZE + ZX_SCREEN_ATTR_SIZE);
  //Utils::restoreScreen(SCRATCH_FILE);

  Utils::restoreZ80States(z80Registers);

  // Give Z80 time to reach the next idle loop so the stock ROM can take control.
  Utils::delay16(1);               
  
  // About to restore the stock ROM and continue game
  // We need to put something useful on the output pins for joystick port 0x1F.
  PORTD = Utils::readJoystick() & INPUT_MASK; 
  
  // Stock rom escapes the idle loop via its NOPs
  Z80Bus::setStockRom();  
  // let the stock rom catch up
  Utils::delay16(1);       

  Utils::readJoystick();  // flush junk

  return false;
}

void InGamePauseMenu::handlePokeMenu() {
  Z80Bus::sendFillCommand(ZX_SCREEN_ATTR_ADDRESS_START + (32*(64/8)), 32*(64/8), COL::BLACK_WHITE);
  Draw::text_P(128 - ((21*6)/2), 40, F("Enter address & value"));
  Draw::text_P(0, 48, F("e.g. JetPac: Infinite Lives, Poke: 25014,0"));
  
  uint8_t x = 144;
  uint8_t y = 64 + 64;
  Z80Bus::sendFillCommand(ZX_SCREEN_ATTR_ADDRESS_START + (((y+0) / 8) * 32) + (x / 8), 14, COL::BRIGHT_CYAN_BLACK);
  Z80Bus::sendFillCommand(ZX_SCREEN_ATTR_ADDRESS_START + (((y+8) / 8) * 32) + (x / 8), 14, COL::MAGENTA_BLACK);
  Z80Bus::sendFillCommand(ZX_SCREEN_ATTR_ADDRESS_START + (((y+16)/ 8) * 32) + (x / 8), 14, COL::GREEN_BLACK);
  
  Draw::text_P(x, y + 0,  F("[Space] Abort"));
  Draw::text_P(x, y + 8,  F("[Sym shft] Delete"));
  Draw::text_P(x, y + 16, F("[Enter] Apply"));
  Draw::text_P(128, 192 / 2, F(","));
  Draw::text_P(128 + 8, 192 / 2, F("___"));

  int32_t addr = readNumericInput(5, 64, 192 / 2, "Poke:", 0x4000, 0xffff);
  int32_t value = readNumericInput(3, 128, 192 / 2, ",", 0, 255);

  if (addr != -1 && value != -1) {
    Poke_Packet pkt(addr, value);
    Z80Bus::sendBytes(reinterpret_cast<uint8_t*>(&pkt), sizeof(Poke_Packet));
  }
}

 
int32_t InGamePauseMenu::readNumericInput(uint8_t maxDigits, int xPos, int yPos, const char* name, uint16_t min, uint16_t max) {
  Draw::text(xPos, yPos, name);

  xPos += strlen(name) * (SmallFont::FNT_WIDTH + SmallFont::FNT_GAP);
  xPos = (xPos + 7) & ~7; // Size Optimization: Replaces ((xPos + 7) / 8) * 8

  uint8_t mark = BufferManager::getMark();
  char* buf = (char*)BufferManager::allocate(maxDigits + 1);

  while (true) {
    memset(buf, ' ', maxDigits);
    buf[maxDigits] = '\0';

    uint8_t len = 0;
    uint8_t flash = 0;
    uint8_t prevKey = 0;

    // Inner loop handles text entry
    while (true) {
      uint8_t key = Z80Bus::getKeyboard();
      if (key != prevKey) {
        prevKey = key;
        if (key != 0) {
          if (key == 0x20) { // CANCEL key (Space) - Exit immediately
            BufferManager::freeToMark(mark);
            return -1;
          }
          if (key == 0x0D && len > 0 && buf[0] != ' ') { // ENTER key
            break; 
          }
          if (key == 0x02 && len > 0) { // DEL key
            buf[--len] = ' ';
            flash = 0;
          } 
          else if (key >= '0' && key <= '9' && len < maxDigits) { // Digit entry
            buf[len++] = key;
            flash = 0;
          }
        }
      }

      // Size Optimization: Flattened ternary logic to save flash cycles
      uint8_t cursorIdx = len;
      if (cursorIdx >= maxDigits) {
        cursorIdx = maxDigits - 1;
      }
      
      char origChar = buf[cursorIdx];
      if (flash < 60) {  
        buf[cursorIdx] = 127; // Blink pattern 
      }

      Draw::text(xPos, yPos, buf);
      buf[cursorIdx] = origChar; // Restore after draw
      
      if (++flash >= 120) flash = 0;
      Utils::delay16(1);
    }
   
    Draw::text(xPos, yPos, buf); // Clear cursor

    // Convert text to value
    uint16_t value = 0;   
    for (uint8_t i = 0; i < len; i++) {
      value = (value * 10) + (buf[i] - '0');
    }
    
    while (Z80Bus::getKeyboard() != 0); 

    if (value >= min && value <= max) {
      BufferManager::freeToMark(mark);
      return value; // Success path: cleanup memory and exit function
    }

    uint16_t targetAddr = ZX_SCREEN_ATTR_ADDRESS_START + ((yPos & ~7) << 2) + (xPos >> 3);
    uint8_t widthCells = ((maxDigits * 6) + 7) >> 3;

    Z80Bus::sendFillCommand(targetAddr, widthCells, COL::FLASH_RED_WHITE);
    Utils::delay16(1100);
    Z80Bus::sendFillCommand(targetAddr, widthCells, COL::BRIGHT_BLACK_WHITE);
  }
}

void InGamePauseMenu::handleScreenshotMenu() {
  bool savedSuccessfully = false;
  do {
    Draw::text_P((ZX_SCREEN_WIDTH_PIXELS / 2) - ((6 * 17) / 2), (ZX_SCREEN_HEIGHT_PIXELS / 2) - 16, F("Saving Screenshot"));

    if (Utils::exportScreenshot("SHOTS")) {
      Draw::text_P((ZX_SCREEN_WIDTH_PIXELS / 2) - ((6 * 15) / 2), (ZX_SCREEN_HEIGHT_PIXELS / 2) + 16, F("SAVED - ANY KEY"));
      Menu::waitForAnyKey();
      break;
    }else{
      if (!SdCardSupport::isInserted()) {
        Utils::waitForSDCard_Blocking(true); // shows "INSERT SD CARD"
      } else {
        Draw::text_P((ZX_SCREEN_WIDTH_PIXELS / 2) - ((6 * 11) / 2), (ZX_SCREEN_HEIGHT_PIXELS / 2) + 16, F("SAVE FAILED"));
        Utils::delay16(2000);
        Utils::clearScreen(COL::BRIGHT_BLACK_WHITE);
      }
    }
  } while (!savedSuccessfully);
}








    // This SloMo kind of works .. but sooner or later games using it crash.
    // I don't think it's the comms between the Arduino and Z80 (i.e. something like a bad jmp command)
    // Think it's just the misuse of the stack .. even though it's only 2 deep.
    // Guess using 4 bytes from the stack is bad - looks like 80s game's programmers new their stack and used all of it!!!


    // // Reminder ... also see 'restoreZ80States' for extra code needed to make this work
    // // ... this slowmo will probably need to use stack in screen memory , it's going to be firing atlot
    // // and if any game's using all their stack space - this will find it and things will go bad!
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

    //   delay(20);  // game slow down 

    //   delay(1);
    //   Z80Bus::setSnaRom();

    //   delay(1);
    //   Utils::storeZ80States();
    //   Utils::restoreZ80States();

    //   // Give Z80 time to reach the next idle loop so the stock ROM can take control.
    //   delay(1);     
    
    //   // About to restore the stock ROM and continue game         
    //   // We need to put something useful on the output pins for joystick port 0x1F.
    //   PORTD = Utils::readJoystick() & INPUT_MASK; 

    //   // Stock rom escapes the idle loop via its NOPs
    //   Z80Bus::setStockRom();  
      
    //   // let the stock rom catch up
    //   delay(1);       
    // }