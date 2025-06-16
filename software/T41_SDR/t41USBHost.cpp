
#include <USBHost_t36.h> // https://github.com/PaulStoffregen/USBHost_t36

#include "keyboard.h"
#include "mouse.h"
#include "t41Control.h"

#include "T41Config.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

/*  it would be nice to save this memory until a keyboard is plugged in
    but both USBHost and USBHIDParser are needed to automatically detect
    a new devise so we don't really save that much.  Doing this manually
    is a possibility if we need to save memory when not using a keyboard. */
#if defined(HOST_KEYBOARD_MOUSE_SUPPORT) || defined(HOST_SERIAL_SUPPORT)
USBHost usbHost;
USBHub usbHub(usbHost);
#endif

#ifdef HOST_KEYBOARD_MOUSE_SUPPORT
USBHIDParser hkbParser(usbHost); // each device needs a parser
KeyboardController kbController(usbHost);

USBHIDParser mouseParser(usbHost); // each device needs a parser
MouseController mouseController(usbHost);
#endif

#ifdef HOST_SERIAL_SUPPORT
//#undef BUFFER_SIZE

// the T41 does not register when plugged into the USB host with this object defined
// this is likely due to the Teensy defaulting to high speed serial vs full speed used by USBSerial
//USBSerial usbHostSerial(usbHost);

// support three host serial object corresponding to Serial, SerialUSB1 and SerialUSB2
// available when the device unit is compiled with USB Type: Serial, Dual Serial or Triple Serial
USBSerial_BigBuffer usbHostSerial(usbHost, 1);
USBSerial_BigBuffer usbHostSerial1(usbHost, 1); // USB device must compiled with Dual or Triple serial
USBSerial_BigBuffer usbHostSerial2(usbHost, 1); // USB device must be compiled with Triple serial

/*
USBDriver *drivers[] = {&usbHostSerial, &usbHub};
bool driver_active[2] = {false, false};
const char *driver_names[2] = {"usbHost", "Hub1"};
//USBHIDParser hostParser(usbHost); // each device needs a parser

uint32_t baud = 1000000;
void check_for_usbhost_device_changes() {
  // Print out information about different devices.
  for(uint8_t i = 0; i < 2; i++) {
    if(*drivers[i] != driver_active[i]) {
      if(driver_active[i]) {
        Serial.printf("*** Device %s - disconnected ***\n", driver_names[i]);
        driver_active[i] = false;
      } else {
        Serial.printf("*** Device %s %x:%x - connected ***\n", driver_names[i], drivers[i]->idVendor(), drivers[i]->idProduct());
        driver_active[i] = true;

        const uint8_t *psz = drivers[i]->manufacturer();

        if(psz && *psz) Serial.printf("  manufacturer: %s\n", psz);
        psz = drivers[i]->product();
        if(psz && *psz) Serial.printf("  product: %s\n", psz);
        psz = drivers[i]->serialNumber();
        if(psz && *psz) Serial.printf("  Serial: %s\n", psz);
        if(drivers[i] == &usbHostSerial) {
          usbHostSerial.begin(baud);
        }
      }
    }
  }
}
*/
#endif

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

FLASHMEM void UsbHostSetup() {
  usbHost.begin();

#ifdef HOST_KEYBOARD_MOUSE_SUPPORT
  KeyboardSetup();
#endif

#ifdef HOST_SERIAL_SUPPORT
#endif

  delay(1000);
}

void UsbHostLoop() {
  usbHost.Task();

#ifdef HOST_KEYBOARD_MOUSE_SUPPORT
  MouseLoop();
#endif

#ifdef HOST_CAT_CONTROL_SUPPORT
  T41ControlLoop();
#endif

/*
#ifdef HOST_SERIAL_SUPPORT
  // check if the USB virtual serial wants a new baud rate
  // ignore if 0 as current Serial monitor of Arduino sets to 0..
  uint32_t cur_usb_baud = Serial.baud();
  if (cur_usb_baud && (cur_usb_baud != baud)) {
    baud = cur_usb_baud;
    Serial.print("Changed baud rate: ");
    if (baud == 57600) {
      // This ugly hack is necessary for talking
      // to the arduino bootloader, which actually
      // communicates at 58824 baud (+2.1% error).
      // Teensyduino will configure the UART for
      // the closest baud rate, which is 57143
      // baud (-0.8% error).  Serial communication
      // can tolerate about 2.5% error, so the
      // combined error is too large.  Simply
      // setting the baud rate to the same as
      // arduino's actual baud rate works.
      usbHostSerial.begin(58824);
      Serial.println(58824);
    } else {
      usbHostSerial.begin(baud);
      Serial.println(baud);
    }
  }

  check_for_usbhost_device_changes();
#endif
*/
}
