// library: https://github.com/PaulStoffregen/USBHost_t36
//

//#include "MyConfigurationFile.h"
#include "SDT.h"

#ifdef KEYBOARD_SUPPORT

#include "ButtonProc.h"
#include "Display.h"
#include "Encoders.h"
#include "ft8.h"
#include "gwv.h"
#include "InfoBox.h"
#include "Menu.h"
#include "MenuProc.h"
#include "mouse.h"
#include "Tune.h"

#ifdef BUFFER_SIZE // BUFFER_SIZE conflicts in USBHost_t36.h
#undef BUFFER_SIZE
#endif

#include <USBHost_t36.h>


extern USBHost usbHost;

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

#define MOUSE_BUTTON_DOWN_LEFT  1
#define MOUSE_BUTTON_DOWN_RIGHT 2

#define FREQ_W  32
#define FREQ_H  48
#define CURSOR_W  16
#define CURSOR_H  32

// active VFO y axis frequency position translated for center of cursor
#define FREQ_T  FREQUENCY_Y - (CURSOR_H / 2)
#define FREQ_B  FREQ_T + FREQ_H - 10

// active VFO x axis frequency digit left position translated for center of cursor
// 40m band and below
#define FREQ_40_6  FREQUENCY_X
#define FREQ_40_5  FREQ_40_6 + FREQ_W * 2  - (CURSOR_W / 2)
#define FREQ_40_4  FREQ_40_5 + FREQ_W
#define FREQ_40_3  FREQ_40_4 + FREQ_W
#define FREQ_40_2  FREQ_40_3 + FREQ_W * 2
#define FREQ_40_1  FREQ_40_2 + FREQ_W
#define FREQ_40_0  FREQ_40_1 + FREQ_W

// 20m and above
#define FREQ_20_7  FREQUENCY_X
#define FREQ_20_6  FREQ_20_7 + FREQ_W - (CURSOR_W / 2)
#define FREQ_20_5  FREQ_20_6 + FREQ_W * 2
#define FREQ_20_4  FREQ_20_5 + FREQ_W
#define FREQ_20_3  FREQ_20_4 + FREQ_W
#define FREQ_20_2  FREQ_20_3 + FREQ_W * 2
#define FREQ_20_1  FREQ_20_2 + FREQ_W
#define FREQ_20_0  FREQ_20_1 + FREQ_W

USBHIDParser mouseParser(usbHost); // each device needs a parser
MouseController mouseController(usbHost);

int cursorL, cursorT, cursorR, cursorB;
int cursorX, cursorY, oldCursorX, oldCursorY;

bool mouseCenterTuneActive = false;
int mouseWheelValue = 0;
int menuBarSelected = false;

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

  if(cursorX > cursorR - CURSOR_W) cursorX = cursorR - CURSOR_W;
  if(cursorX < cursorL) cursorX = cursorL;
  if(cursorY > cursorB - CURSOR_H) cursorY = cursorB - CURSOR_H;
  if(cursorY < cursorT) cursorY = cursorT;

  //Serial.print("cursorX = "); Serial.print(cursorX); Serial.print(" cursorY = "); Serial.println(cursorY);
  tft.setFontScale((enum RA8875tsize)1);

  // the cursor is drawn on layer 2, switch to it
  tft.writeTo(L2);

  // other items occupy layer 2
  // we need to prevent the cursor from overwriting them we do this by copying what
  // will be under the cursor for restoration elsewhere.  The RA8875 has limited
  // functionality to do this.  I've used a cursor sized block at 0,0 to handle this.
  // A white block on layer 1 hides the layer 2 copy under it.  All's not wasted as
  // this block has some functionality.  A problem occurs when the cursor is within
  // this block so we'll handle that separately.
  if(cursorY < FREQ_T) {
    // there's no layer 2 items in this area
    // erase old cursor by simply drawing it again in black
    tft.setTextColor(RA8875_BLACK);
    tft.setCursor(oldCursorX, oldCursorY);
    tft.print((char)7);
  } else {
    // replace what was previously on layer 2 under the cursor
    //BTE_move(SourceX, SourceY, Width, Height, DestX, DestY, SourceLayer, DestLayer,bool Transparent, uint8_t ROP, bool Monochrome, bool ReverseDir)
    tft.BTE_move(0, 0, 16, 32, oldCursorX, oldCursorY, 2, 2);

    // copy the background under the cursor for replacement next time
    tft.BTE_move(cursorX, cursorY, 16, 32, 0, 0, 2, 2);
  }

  // draw new cursor
  tft.setTextColor(RA8875_WHITE);
  tft.setCursor(cursorX, cursorY);
  tft.print((char)7);

  tft.writeTo(L1); // switch to layer 1

  oldCursorX = cursorX;
  oldCursorY = cursorY;
}

FLASHMEM void SetMouseArea(int left, int top, int width, int height) {
  cursorL = left;
  cursorT = top;
  cursorR = left + width;
  cursorB = top + height;

  cursorX = left;
  cursorY = top;
  oldCursorX = left;
  oldCursorY = top;
}

bool CursorInMenuArea() {
  return (cursorY < FREQ_T) && (cursorX < TIME_X - 20);
}

bool CursorInFreqArea() {
  return (cursorY > FREQ_T && cursorY < FREQ_B) && (cursorX > 0 && cursorX < TIME_X - 20);
}

bool CursorInOpStatsArea() {
  return (cursorY > OPERATION_STATS_T - CURSOR_H / 2) && (cursorY < OPERATION_STATS_T - CURSOR_H / 2 + OPERATION_STATS_H) && (cursorX >= 0 && cursorX <= OPERATION_STATS_W);
}

bool CursorInAudioSpectrum() {
  return (cursorY > AUDIO_SPEC_BOX_T - CURSOR_H / 2) && (cursorY < AUDIO_SPEC_BOTTOM - CURSOR_H / 2) && cursorX > AUDIO_SPEC_BOX_L;
}

bool CursorInSpectrumWaterfall() {
  return (cursorY > SPEC_BOX_T) && (cursorX < SPEC_BOX_W);
}

bool CursorInInfoBox() {
  return (cursorY > INFO_BOX_T) && (cursorY < INFO_BOX_T + INFO_BOX_H) && (cursorX > INFO_BOX_L && cursorX < INFO_BOX_L + INFO_BOX_W);
}

void MouseButtonMenuArea(int button) {
  if(button == 1) {
    if(getMenuValueActive || getMenuOptionActive) {
      menuBarSelected = true;
    } else {
      MenuBarSelect();
    }
  } else if(button == 2) {
    ShowMenuBar();
  }
}

void MouseWheelMenuArea(int wheel) {
  if(getMenuValueActive || getMenuOptionActive) {
    mouseWheelValue = wheel;
  } else {
    MenuBarChange(wheel);
  }
}

void MouseButtonFreqArea(int button) {
  int inc = 0;
  int vfoOffset = activeVFO == VFO_A ? 0 : VFO_B_ACTIVE_OFFSET;
  int x = cursorX - vfoOffset; // adjust cursor position for active VFO

  switch(button) {
    case 1:
      // we're switching to the other VFO if we click within its field
      if((activeVFO == VFO_B && cursorX < VFO_B_ACTIVE_OFFSET - 50) || (activeVFO == VFO_A && cursorX > VFO_B_INACTIVE_OFFSET)) {
        VFOSelect(activeVFO == VFO_A ? VFO_B : VFO_A);
      }
      break;

    case 2:
      // we're zeroing a portion of the frequency if we're within the active VFO
      //Serial.print(cursorX); Serial.print(","); Serial.println(cursorY);

      if(TxRxFreq < 10000000) {
        if(x > FREQ_40_3 && x < FREQ_40_3 + FREQ_W) {
          inc = TxRxFreq % 10000;
        } else if(x > FREQ_40_4 && x < FREQ_40_4 + FREQ_W) {
          inc = TxRxFreq % 100000;
        } else if(x > FREQ_40_5 && x < FREQ_40_5 + FREQ_W) {
          inc = TxRxFreq % 1000000;
        } else if(x > FREQ_40_2 && x < FREQ_40_2 + FREQ_W) {
          inc = TxRxFreq % 1000;
        } else if(x > FREQ_40_1 && x < FREQ_40_1 + FREQ_W) {
          inc = TxRxFreq % 100;
        } else if(x > FREQ_40_6 && x < FREQ_40_6 + FREQ_W) {
          inc = TxRxFreq % 10000000;
        } else if(x > FREQ_40_0 && x < FREQ_40_0 + FREQ_W) {
          inc = TxRxFreq % 10;
        }
      } else {
        if(x > FREQ_20_3 && x < FREQ_20_3 + FREQ_W) {
          inc = TxRxFreq % 10000;
        } else if(x > FREQ_20_4 && x < FREQ_20_4 + FREQ_W) {
          inc = TxRxFreq % 100000;
        } else if(x > FREQ_20_5 && x < FREQ_20_5 + FREQ_W) {
          inc = TxRxFreq % 1000000;
        } else if(x > FREQ_20_2 && x < FREQ_20_2 + FREQ_W) {
          inc = TxRxFreq % 1000;
        } else if(x > FREQ_20_1 && x < FREQ_20_1 + FREQ_W) {
          inc = TxRxFreq % 100;
        } else if(x > FREQ_20_6 && x < FREQ_20_6 + FREQ_W) {
          inc = TxRxFreq % 10000000;
        } else if(x > FREQ_20_0 && x < FREQ_20_0 + FREQ_W) {
          inc = TxRxFreq % 10;
        } else if(x > FREQ_20_7 && x < FREQ_20_7 + FREQ_W) {
          inc = TxRxFreq % 100000000;
        }
      }
      if(inc < SampleRate / (1 << spectrumZoom)) {
        SetNCOFreq(NCOFreq - inc);
      } else {
        SetCenterTune(-inc);
      }
      break;

      default:
        break;
  }
}

void MouseWheelFreqArea(int wheel) {
  int inc = 0;
  int vfoOffset = activeVFO == VFO_A ? 0 : VFO_B_ACTIVE_OFFSET;
  int x = cursorX - vfoOffset; // adjust cursor position for active VFO

  //Serial.println(wheel);

  if(TxRxFreq < 10000000) {
    if(x > FREQ_40_3 && x < FREQ_40_3 + FREQ_W) {
      inc = 1000;
    } else if(x > FREQ_40_4 && x < FREQ_40_4 + FREQ_W) {
      inc = 10000;
    } else if(x > FREQ_40_5 && x < FREQ_40_5 + FREQ_W) {
      inc = 100000;
    } else if(x > FREQ_40_2 && x < FREQ_40_2 + FREQ_W) {
      inc = 100;
    } else if(x > FREQ_40_1 && x < FREQ_40_1 + FREQ_W) {
      inc = 10;
    } else if(x > FREQ_40_6 && x < FREQ_40_6 + FREQ_W) {
      inc = 1000000;
    } else if(x > FREQ_40_0 && x < FREQ_40_0 + FREQ_W) {
      inc = 1;
    }
  } else {
    if(x > FREQ_20_3 && x < FREQ_20_3 + FREQ_W) {
      inc = 1000;
    } else if(x > FREQ_20_4 && x < FREQ_20_4 + FREQ_W) {
      inc = 10000;
    } else if(x > FREQ_20_5 && x < FREQ_20_5 + FREQ_W) {
      inc = 100000;
    } else if(x > FREQ_20_2 && x < FREQ_20_2 + FREQ_W) {
      inc = 100;
    } else if(x > FREQ_20_1 && x < FREQ_20_1 + FREQ_W) {
      inc = 10;
    } else if(x > FREQ_20_6 && x < FREQ_20_6 + FREQ_W) {
      inc = 1000000;
    } else if(x > FREQ_20_0 && x < FREQ_20_0 + FREQ_W) {
      inc = 1;
    } else if(x > FREQ_20_7 && x < FREQ_20_7 + FREQ_W) {
      inc = 10000000;
    }
  }
  //inc *= wheel;
  //Serial.println(inc);

  if(inc < SampleRate / (1 << spectrumZoom)) {
    SetNCOFreq(NCOFreq + inc * wheel);
  } else {
    SetCenterTune(inc * wheel);
  }
}

void MouseButtonOpStatsArea(int button) {
  if(button == 1 && cursorX < OPERATION_STATS_BD - 20) {
    ResetTuning();
  } else if(cursorX > OPERATION_STATS_BD - 5 && cursorX < OPERATION_STATS_MD - 20) {
    if(button == 1) {
      BandChange(1);
    } else {
      BandChange(-1);
    }
  } else if(button == 1 && cursorX > OPERATION_STATS_MD - 5 && cursorX < OPERATION_STATS_CWF) {
    ButtonMode();
  } else if(button == 1 && cursorX > OPERATION_STATS_DMD - 5 && cursorX < OPERATION_STATS_DMD + 35) {
    ButtonDemodMode();
  }
}

void MouseButtonSpectrumWaterfall(int button) {
  if(button == 1) {
    if(bands[currentBand].mode == DEMOD_FT8 && (cursorY > YPIXELS - 25 * 5 - CURSOR_H / 2 - 8)) {
      ft8MsgSelectActive = true;
      //int msg = wfRows - (YPIXELS - cursorY - CURSOR_H / 2) / 5;
      int y = YPIXELS - cursorY - CURSOR_H / 2;
      //int msg = map(YPIXELS - cursorY - CURSOR_H / 2, YPIXELS - 25 * 5, 16, 0, 4);
      int msg = map(y, 25 * 5, 16, 0, 4);
      if(cursorX > 256) msg += 5;

      if(num_decoded_msg > 1 && msg < num_decoded_msg) {
        activeMsg = msg;
      }
    } else {
      // there was a left click is in the spectrum or waterfall area, set the NCO frequency

      // replace what was previously under the cursor
      tft.BTE_move(0, 0, 16, 32, oldCursorX, oldCursorY, 2, 2);

      SetNCOFreq((cursorX + CURSOR_W / 2 - centerLine) * SampleRate / (1 << spectrumZoom) / SPECTRUM_RES);

      DrawBandwidthBar();

      // background under the cursor may have changed, copy it for replacement next time
      tft.BTE_move(cursorX, cursorY, 16, 32, 0, 0, 2, 2);
    }
  }
}

void MouseWheelSpectrumWaterfall(int wheel) {
  if(bands[currentBand].mode == DEMOD_FT8 && (cursorY > YPIXELS - 25 * 5 - CURSOR_H / 2 - 8)) {
    if(num_decoded_msg > 0) {
      activeMsg += wheel;
      if(activeMsg >= num_decoded_msg) {
        activeMsg = 0;
      } else {
        if(activeMsg < 0) {
          activeMsg = num_decoded_msg - 1;
        }
      }
    }
    return;
  }
  if(mouseCenterTuneActive) {
    SetCenterTune((long)freqIncrement * wheel);
  } else {
    SetFineTune((long)ftIncrement * wheel);
  }

  // redraw cursor
  //tft.setFontScale((enum RA8875tsize)1);
  //tft.writeTo(L2); // switch to layer 2
  //tft.setTextColor(RA8875_WHITE);
  //tft.setCursor(cursorX, cursorY);
  //tft.print((char)7);
  //tft.writeTo(L1); // switch to layer 1
  //MoveCursor(0, 0);
}

void MouseLoop() {
  int x, y, button, wheel;

  if(mouseController.available()) {
    x = mouseController.getMouseX();
    y = mouseController.getMouseY();
    button = mouseController.getButtons();
    wheel = mouseController.getWheel();

    if(x || y)
      MoveCursor(x, y);

    // *** TODO: these can be refined ***
    if(button) {
      // check if the cursor is in any areas with an button action
      if(CursorInMenuArea()) {
        // the cursor is in the menu bar area
        MouseButtonMenuArea(button);
      } else if(CursorInFreqArea()) {
        // the cursor is in the frequency field
        MouseButtonFreqArea(button);
      } else if(CursorInOpStatsArea()) {
        // the cursor is in the operating stats area
        MouseButtonOpStatsArea(button);
      } else if(CursorInAudioSpectrum()) {
        ButtonFilter();
      } else if(CursorInSpectrumWaterfall()) {
        MouseButtonSpectrumWaterfall(button);
      } else if(CursorInInfoBox()) {
        MouseButtonInfoBox(button, cursorX + CURSOR_W / 2, cursorY + CURSOR_H / 2);
      }
    }

    if(wheel) {
      //Serial.print(",  wheel = "); Serial.println(wheel);
      if(CursorInMenuArea()) {
        // the cursor is in the menu bar area
        MouseWheelMenuArea(wheel);
      } else if(CursorInFreqArea()) {
        MouseWheelFreqArea(wheel);
      } else if(CursorInSpectrumWaterfall()) {
        MouseWheelSpectrumWaterfall(wheel);
      } else if(CursorInAudioSpectrum()) {
        // *** TODO: consider refactoring with similar code in EncoderMenuChangeFilterISR()
        if (ft8MsgSelectActive) {
          if(num_decoded_msg > 0) {
            activeMsg += wheel;
            if(activeMsg >= num_decoded_msg) {
              activeMsg = 0;
            } else {
              if(activeMsg < 0) {
                activeMsg = num_decoded_msg - 1;
              }
            }
          }
        } else {
          if (bands[currentBand].mode == DEMOD_NFM && nfmBWFilterActive) {
            // we're adjusting NFM demod bandwidth
            filter_pos_BW = last_filter_pos_BW - 5 * wheel;
          } else {
            // we're adjusting audio spectrum filter
            posFilterEncoder = lastFilterEncoder - 5 * wheel;
          }
        }
      } else if(CursorInInfoBox()) {
        if(liveNoiseFloorFlag) {
          currentNoiseFloor[currentBand] += wheel;
        } else {
          MouseWheelInfoBox(wheel, cursorX + CURSOR_W / 2, cursorY + CURSOR_H / 2);
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
