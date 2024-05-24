// library: https://github.com/PaulStoffregen/USBHost_t36
//

#include "MyConfigurationFile.h"

#ifdef KEYBOARD_SUPPORT

#include <USBHost_t36.h>

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

// *** it would be nice to save this memory until a keyboard is plugged in
// *** but both USBHost and USBHIDParser are needed to automatically detect
// *** a new devise so we don't really save that much.  Doing this manually
// is a possibility if we need to save memory when not using a keyboard.
USBHost usbHost;
USBHub usbHub(usbHost);
USBHIDParser hkbParser(usbHost); // each device needs a parser
KeyboardController kbController(usbHost);

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
}

void putc(uint8_t input) {
  kbBuffer[kbIndexIn++] = input;
}

void OnRelease(int unicode) {
  if (unicode == 0) return;

   putc(unicode & 0xff);
}

FLASHMEM void UsbSetup() {
  usbHost.begin();
  kbController.attachRelease(OnRelease);

  kbBuffer[0] = 0;

  delay(1000);
}

void UsbLoop() {
  usbHost.Task();
}

#endif
