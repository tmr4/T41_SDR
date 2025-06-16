
#include "T41Config.h"

#ifdef HOST_KEYBOARD_MOUSE_SUPPORT

#include <USBHost_t36.h> // https://github.com/PaulStoffregen/USBHost_t36

extern KeyboardController kbController;

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

uint8_t kbIndexIn, kbIndexOut;
DMAMEM uint8_t kbBuffer[256];

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

uint8_t getc() {
  if(kbIndexIn == kbIndexOut) {
    return 0;
  }

  return kbBuffer[kbIndexOut++];
  if(kbIndexOut >= 256) kbIndexOut = 0;
}

void putc(uint8_t input) {
  kbBuffer[kbIndexIn++] = input;
  if(kbIndexIn >= 256) kbIndexIn = 0;
}

uint8_t rawPressed = 0;
void OnRawPress(uint8_t unicode) {
  //Serial.print("   Raw pressed: "); Serial.println(unicode);
  rawPressed = unicode;
}

void OnRelease(int unicode) {
  // a space and backspace with cap lock on is coming through as 0
  // use the raw key pressed to process these keys
  if(unicode == 0) {
    // use the last key that was pressed rather than released since the
    // raw released task occurs after the released task
        //Serial.print("rawPressed: "); Serial.println(rawPressed);
    switch(rawPressed) {
      case 42:
        // it's a space, enter it in the buffer
        putc(127);
        break;

      case 44:
        //Serial.print("rawPressed: "); Serial.println(rawPressed);
        // it's a space, enter it in the buffer
        putc(32);
        break;

      default:
        return;
    }
  }
  //Serial.print("   released: "); Serial.println(unicode & 0xff);
  putc(unicode & 0xff);
}

void KeyboardSetup() {
  // capture processed key on release and store in keyboard buffer
  kbController.attachRelease(OnRelease);

  // *** Teensy USB Host library doesn't process space and backspace keys properly
  //     when the caps lock key is on (error in library?) ***
  // Use a raw key press to identify these keys
  kbController.attachRawPress(OnRawPress);

  kbIndexIn = 0;
  kbIndexOut = 0;
  kbBuffer[0] = 0;
}

#endif
