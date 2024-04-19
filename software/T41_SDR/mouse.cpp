// library: https://github.com/PaulStoffregen/USBHost_t36
//

#include "MyConfigurationFile.h"

#ifdef KEYBOARD_SUPPORT

#include <USBHost_t36.h>
#include <RA8875.h>                    // https://github.com/mjs513/RA8875/tree/RA8875_t4

#include "Tune.h"

extern USBHost usbHost;
extern RA8875 tft;

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

#define MOUSE_BUTTON_DOWN_LEFT  1
#define MOUSE_BUTTON_DOWN_RIGHT 2

// active VFO y axis frequency position
#define FREQ_T  30
#define FREQ_B  60

// active VFO x axis frequency digit centerline position
// 40m band and below
#define FREQ_40_6  10
#define FREQ_40_5  80
#define FREQ_40_4  110
#define FREQ_40_3  140
#define FREQ_40_2  200
#define FREQ_40_1  235
#define FREQ_40_0  270

// 20m and above
#define FREQ_20_7  10
#define FREQ_20_6  45
#define FREQ_20_5  110
#define FREQ_20_4  140
#define FREQ_20_3  170
#define FREQ_20_2  240
#define FREQ_20_1  270
#define FREQ_20_0  300

USBHIDParser mouseParser(usbHost); // each device needs a parser
MouseController mouseController(usbHost);

int cursorL, cursorT, cursorR, cursorB;
int cursorX, cursorY, oldCursorX, oldCursorY;
int low = 15;
int high = 5;
//-------------------------------------------------------------------------------------------------------------
// Forwards
//-------------------------------------------------------------------------------------------------------------

void ShowFrequency();

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

void MoveCursor(int x, int y) {
  cursorX += x * 4;
  cursorY += y * 2;

  if(cursorX > cursorR) cursorX = cursorR;
  if(cursorX < cursorL) cursorX = cursorL;
  if(cursorY > cursorB) cursorY = cursorB;
  if(cursorY < cursorT) cursorY = cursorT;

  //Serial.print("cursorX = "); Serial.print(cursorX); Serial.print(" cursorY = "); Serial.println(cursorY);
  tft.setFontScale((enum RA8875tsize)1);

  tft.writeTo(L2); // switch to layer 2

  // erase old cursor
  tft.setTextColor(RA8875_BLACK);
  tft.setCursor(oldCursorX, oldCursorY);
  //tft.print("o");
  tft.print((char)7);

  // draw new cursor
  //tft.setTextColor(RA8875_YELLOW);
  tft.setTextColor(RA8875_WHITE);
  tft.setCursor(cursorX, cursorY);
  //tft.print("o");
  tft.print((char)7);

  tft.writeTo(L1); // switch to layer 1

  oldCursorX = cursorX;
  oldCursorY = cursorY;
}

void SetMouseArea(int left, int top, int width, int height) {
  cursorL = left;
  cursorT = top;
  cursorR = left + width;
  cursorB = top + height;

  cursorX = left;
  cursorY = top;
  oldCursorX = left;
  oldCursorY = top;
}

void MouseLoop() {
  int x, y, button, wheel;

  if (mouseController.available()) {
    x = mouseController.getMouseX();
    y = mouseController.getMouseY();
    button = mouseController.getButtons();
    wheel = mouseController.getWheel();

    if(x || y)
      MoveCursor(x, y);

    if(button == 2) {
      //Serial.print(cursorX); Serial.print(","); Serial.println(cursorY);
      if(cursorY > FREQ_T && cursorY < FREQ_B) {
        x = cursorX;
        if(cursorX > 0 && cursorX < 310) {
          int inc = 0;

          if(TxRxFreq < 10000000) {
            if(x > FREQ_40_3 - low && x < FREQ_40_3 + high) {
              inc = TxRxFreq % 10000;
            } else if(x > FREQ_40_4 - low && x < FREQ_40_4 + high) {
              inc = TxRxFreq % 100000;
            } else if(x > FREQ_40_5 - low && x < FREQ_40_5 + high) {
              inc = TxRxFreq % 1000000;
            } else if(x > FREQ_40_2 - low && x < FREQ_40_2 + high) {
              inc = TxRxFreq % 1000;
            } else if(x > FREQ_40_1 - low && x < FREQ_40_1 + high) {
              inc = TxRxFreq % 100;
            } else if(x > FREQ_40_6 - low && x < FREQ_40_6 + high) {
              inc = TxRxFreq % 10000000;
            } else if(x > FREQ_40_0 - low && x < FREQ_40_0 + high) {
              inc = TxRxFreq % 10;
            } else if(x > FREQ_40_7 - low && x < FREQ_40_7 + high) {
              inc = TxRxFreq % 100000000;
            }
          } else {
            if(x > FREQ_20_3 - low && x < FREQ_20_3 + high) {
              inc = TxRxFreq % 10000;
            } else if(x > FREQ_20_4 - low && x < FREQ_20_4 + high) {
              inc = TxRxFreq % 100000;
            } else if(x > FREQ_20_5 - low && x < FREQ_20_5 + high) {
              inc = TxRxFreq % 1000000;
            } else if(x > FREQ_20_2 - low && x < FREQ_20_2 + high) {
              inc = TxRxFreq % 1000;
            } else if(x > FREQ_20_1 - low && x < FREQ_20_1 + high) {
              inc = TxRxFreq % 100;
            } else if(x > FREQ_20_6 - low && x < FREQ_20_6 + high) {
              inc = TxRxFreq % 10000000;
            } else if(x > FREQ_20_0 - low && x < FREQ_20_0 + high) {
              inc = TxRxFreq % 10;
            } else if(x > FREQ_20_7 - low && x < FREQ_20_7 + high) {
              inc = TxRxFreq % 100000000;
            }
          }
          SetCenterTune(-inc);
        }
      }
    }

    if(wheel) {
      //Serial.print(",  wheel = "); Serial.println(wheel);
      if(cursorY > FREQ_T && cursorY < FREQ_B) {
        x = cursorX;
        if(cursorX > 0 && cursorX < 310) {
          int inc = 0;

          if(TxRxFreq < 10000000) {

          } else {
            if(x > FREQ_20_3 - low && x < FREQ_20_3 + high) {
              inc = 1000;
            } else if(x > FREQ_20_4 - low && x < FREQ_20_4 + high) {
              inc = 10000;
            } else if(x > FREQ_20_5 - low && x < FREQ_20_5 + high) {
              inc = 100000;
            } else if(x > FREQ_20_2 - low && x < FREQ_20_2 + high) {
              inc = 100;
            } else if(x > FREQ_20_1 - low && x < FREQ_20_1 + high) {
              inc = 10;
            } else if(x > FREQ_20_6 - low && x < FREQ_20_6 + high) {
              inc = 1000000;
            } else if(x > FREQ_20_0 - low && x < FREQ_20_0 + high) {
              inc = 1;
            } else if(x > FREQ_20_7 - low && x < FREQ_20_7 + high) {
              inc = 10000000;
            }
          }
          SetCenterTune(wheel * inc);
        }
      }
    }
    //Serial.print("Mouse: buttons = "); Serial.print(button);
    //Serial.print(",  mouseX = "); Serial.print(x);
    //Serial.print(",  mouseY = "); Serial.print(y);
    //Serial.print(",  wheel = "); Serial.print(wheel);
    //Serial.print(",  wheelH = "); Serial.print(mouseController.getWheelH());
    //Serial.println();
    mouseController.mouseDataClear();
  }
}

/*
extern KeyboardController kbController;
extern USBHIDParser hidParser;
// Lets also look at HID Input devices
USBHIDInput *hiddrivers[] = { &kbController, &mouseController };
#define CNT_HIDDEVICES (sizeof(hiddrivers) / sizeof(hiddrivers[0]))
const char *hid_driver_names[CNT_HIDDEVICES] = { "Keyboard1", "Mouse1" };
bool hid_driver_active[CNT_HIDDEVICES] = { false, false };
USBHIDParser hid2(usbHost);
USBDriver *drivers[] = { &hidParser, &hid2 };
#define CNT_DEVICES (sizeof(drivers) / sizeof(drivers[0]))
const char *driver_names[CNT_DEVICES] = { "hidParser", "hid2" };
bool driver_active[CNT_DEVICES] = { false, false };

void ShowUpdatedDeviceListInfo() {
  for (uint8_t i = 0; i < CNT_DEVICES; i++) {
    if (*drivers[i] != driver_active[i]) {
      if (driver_active[i]) {
        Serial.printf("*** Device %s - disconnected ***\n", driver_names[i]);
        driver_active[i] = false;
      } else {
        Serial.printf("*** Device %s %x:%x - connected ***\n", driver_names[i], drivers[i]->idVendor(), drivers[i]->idProduct());
        driver_active[i] = true;
  
        const uint8_t *psz = drivers[i]->manufacturer();
        if (psz && *psz) Serial.printf("  manufacturer: %s\n", psz);
        psz = drivers[i]->product();
        if (psz && *psz) Serial.printf("  product: %s\n", psz);
        psz = drivers[i]->serialNumber();
        if (psz && *psz) Serial.printf("  Serial: %s\n", psz);
      }
    }
  }

  for (uint8_t i = 0; i < CNT_HIDDEVICES; i++) {
    if (*hiddrivers[i] != hid_driver_active[i]) {
      if (hid_driver_active[i]) {
        Serial.printf("*** HID Device %s - disconnected ***\n", hid_driver_names[i]);
        hid_driver_active[i] = false;
      } else {
        Serial.printf("*** HID Device %s %x:%x - connected ***\n", hid_driver_names[i], hiddrivers[i]->idVendor(), hiddrivers[i]->idProduct());
        hid_driver_active[i] = true;

        const uint8_t *psz = hiddrivers[i]->manufacturer();
        if (psz && *psz) Serial.printf("  manufacturer: %s\n", psz);
        psz = hiddrivers[i]->product();
        if (psz && *psz) Serial.printf("  product: %s\n", psz);
        psz = hiddrivers[i]->serialNumber();
        if (psz && *psz) Serial.printf("  Serial: %s\n", psz);
      }
    }
  }
}
*/
#endif
