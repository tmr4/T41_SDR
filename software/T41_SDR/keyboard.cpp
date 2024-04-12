// modified from: Arduino IDE -> File -> Examples -> USBHost_t36 -> keyboard_viewer.ino
// library: https://github.com/PaulStoffregen/USBHost_t36
//

//#include "SDT.h"
#include "MyConfigurationFile.h"

#ifdef KEYBOARD_SUPPORT

//#ifdef BUFFER_SIZE
//#undef BUFFER_SIZE
//#endif

//#include "t41_keyboard.h"
#include <USBHost_t36.h>
//#include "Display.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

USBHost myusb;
//T41KeyboardController keyboard1(myusb);
KeyboardController keyboard1(myusb);
USBHIDParser hid1(myusb);


uint8_t kbIndexIn, kbIndexOut;
DMAMEM uint8_t kbBuffer[256];
//char kbBuffer[] = { "abcdefghijklmnopqrstuvwxyz/0"};

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

uint8_t getc() {
  if(kbIndexIn == kbIndexOut) {
    return 0;
  }

  return kbBuffer[kbIndexOut++];
}

void putc(uint8_t input) {
  kbBuffer[kbIndexIn++] = input;
}

void OnRelease(int unicode) {
  if (unicode == 0) return;

   putc(unicode & 0xff);
}

FLASHMEM void usbSetup() {
  myusb.begin();
  keyboard1.attachRelease(OnRelease);

  kbBuffer[0] = 0;

  delay(1000);
}

void usbLoop() {
  myusb.Task();
}

#endif
