#include "Debug.h"
#include "Constants.h"

#ifdef SERIAL_DEBUG
#warning "*** SERIAL_DEBUG enabled: Z80 Bus Comms will be BROKEN! ***"
// NOTE: D0/D1 are shared with the Z80 data bus.
// USE ONLY when debugging the Nano standalone via USB.
// !!! Connecting USB while the Z80 board is powered will mix two 5V power sources !!!
#include <SPI.h>

void Debug::setupSerial() {
    Serial.begin(9600);
    while (!Serial)  { };
    Serial.println("SERIAL DEBUG BREAKS COMMS TO Z80");
}
#endif