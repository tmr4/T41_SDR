#include "SDT.h"
#include "Beacon.h"
#include "Button.h"
#include "ButtonProc.h"
#include "Display.h"
#include "EEPROM.h"
#include "ft8.h"
#include "InfoBox.h"
#include "Menu.h"
#include "MenuProc.h"
#include "Process.h"
#include "Process2.h"
#include "psk31.h"
#include "Tune.h"
#include "Utility.h"

#include "debug.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

int buttonRead = 0;
int minPinRead = 1024;

/*
The button interrupt routine implements a first-order recursive filter, or "leaky integrator,"
as described at:

  https://www.edn.com/a-simple-software-lowpass-filter-suits-embedded-system-applications/

Filter bandwidth is dependent on the sample rate and the "k" parameter, as follows:

                                1 Hz
                          k   Bandwidth   Rise time (samples)
                          1   0.1197      3
                          2   0.0466      8
                          3   0.0217      16
                          4   0.0104      34
                          5   0.0051      69
                          6   0.0026      140
                          7   0.0012      280
                          8   0.0007      561

Thus, the default values below create a filter with 10000 * 0.0217 = 217 Hz bandwidth
*/

#ifndef ALT_ISR
#define BUTTON_FILTER_SAMPLERATE 10000  // Hz
#define BUTTON_FILTER_SHIFT 3           // Filter parameter k
#define BUTTON_DEBOUNCE_DELAY 5000      // uSec

#define BUTTON_STATE_UP 0
#define BUTTON_STATE_DEBOUNCE 1
#define BUTTON_STATE_PRESSED 2

#define BUTTON_USEC_PER_ISR (1000000 / BUTTON_FILTER_SAMPLERATE)

#define BUTTON_OUTPUT_UP 1023  // Value to be output when in the UP state

IntervalTimer buttonInterrupts;
bool buttonInterruptsEnabled = false;
static unsigned long buttonFilterRegister;
static int buttonState, buttonADCPressed, buttonElapsed;
static volatile int buttonADCOut;
#else
#define BUTTON_FILTER_SAMPLERATE 10000  // Hz
#define BUTTON_FILTER_SHIFT 3           // Filter parameter k
//#define BUTTON_DEBOUNCE_DELAY 5000      // uSec
#define BUTTON_DEBOUNCE_DELAY 2500      // uSec
#define BUTTON_DEBOUNCE_RELEASE_DELAY 2500      // uSec

#define BUTTON_STATE_UP 0
#define BUTTON_STATE_DEBOUNCE 1
#define BUTTON_STATE_PRESSED 2
#define BUTTON_STATE_RELEASE_DEBOUNCE 3

#define BUTTON_USEC_PER_ISR (1000000 / BUTTON_FILTER_SAMPLERATE)

#define BUTTON_OUTPUT_UP 1023  // Value to be output when in the UP state

IntervalTimer buttonInterrupts;
bool buttonInterruptsEnabled = false;
static unsigned long buttonFilterRegister;
//static int buttonState, buttonADCPressed, buttonElapsed;
static int buttonADCPressed, buttonElapsed;
int buttonState = BUTTON_STATE_UP;
static volatile int buttonADCOut;
#endif

//-------------------------------------------------------------------------------------------------------------
// Forwards
//-------------------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

/*****
  Purpose: ISR to read button ADC and detect button presses

  Parameter list:
    none
  Return value;
    void
*****/
void ButtonISR() {
#ifndef ALT_ISR
  int filteredADCValue;

  buttonFilterRegister = buttonFilterRegister - (buttonFilterRegister >> BUTTON_FILTER_SHIFT) + analogRead(BUSY_ANALOG_PIN);
  filteredADCValue = (int)(buttonFilterRegister >> BUTTON_FILTER_SHIFT);

  switch (buttonState) {
    case BUTTON_STATE_UP:
      if (filteredADCValue <= buttonThresholdPressed) {
        buttonElapsed = 0;
        buttonState = BUTTON_STATE_DEBOUNCE;
      }
      break;

    case BUTTON_STATE_DEBOUNCE:
      if (buttonElapsed < BUTTON_DEBOUNCE_DELAY) {
        buttonElapsed += BUTTON_USEC_PER_ISR;
      } else {
        buttonADCOut = buttonADCPressed = filteredADCValue;
        buttonElapsed = 0;
        buttonState = BUTTON_STATE_PRESSED;
      }
      break;

    case BUTTON_STATE_PRESSED:
      if (filteredADCValue >= buttonThresholdReleased) {
        buttonState = BUTTON_STATE_UP;
        } else if (buttonRepeatDelay != 0) {  // buttonRepeatDelay of 0 disables repeat
          if (buttonElapsed < buttonRepeatDelay) {
            buttonElapsed += BUTTON_USEC_PER_ISR;
          } else {
            buttonADCOut = buttonADCPressed;
            buttonElapsed = 0;
          }
        }
      break;
  }
#else
  int filteredADCValue;

  buttonFilterRegister = buttonFilterRegister - (buttonFilterRegister >> BUTTON_FILTER_SHIFT) + analogRead(BUSY_ANALOG_PIN);
  filteredADCValue = (int)(buttonFilterRegister >> BUTTON_FILTER_SHIFT);

  switch (buttonState) {
    case BUTTON_STATE_UP:
      if (filteredADCValue <= buttonThresholdPressed) {
        buttonElapsed = 0;
        buttonState = BUTTON_STATE_DEBOUNCE;
      }
      break;

    case BUTTON_STATE_DEBOUNCE:
      if (buttonElapsed < BUTTON_DEBOUNCE_DELAY) {
        buttonElapsed += BUTTON_USEC_PER_ISR;
      } else {
        buttonADCOut = buttonADCPressed = filteredADCValue;
        //buttonADCPressed = filteredADCValue;
        buttonElapsed = 0;
        buttonState = BUTTON_STATE_PRESSED;
      }
      break;

    case BUTTON_STATE_PRESSED:
      if (filteredADCValue >= buttonThresholdReleased) {
        buttonState = BUTTON_STATE_UP;
        //buttonState = BUTTON_STATE_RELEASE_DEBOUNCE;
      } else {
        if (buttonRepeatDelay != 0) {  // buttonRepeatDelay of 0 disables repeat
          if (buttonElapsed < buttonRepeatDelay) {
            buttonElapsed += BUTTON_USEC_PER_ISR;
          } else {
            buttonADCOut = buttonADCPressed;
            buttonElapsed = 0;
          }
        }
      }
      break;

    case BUTTON_STATE_RELEASE_DEBOUNCE:
      if (buttonElapsed < BUTTON_DEBOUNCE_RELEASE_DELAY) {
        buttonElapsed += BUTTON_USEC_PER_ISR;
      } else {
        buttonADCOut = buttonADCPressed;
        buttonElapsed = 0;
        buttonState = BUTTON_STATE_UP;
      }
      break;
  }
#endif
}

/*****
  Purpose: Starts button IntervalTimer and toggles subsequent button
           functions into interrupt mode.

  Parameter list:
    none
  Return value;
    void
*****/
FLASHMEM void EnableButtonInterrupts() {
  buttonADCOut = BUTTON_OUTPUT_UP;
  buttonFilterRegister = buttonADCOut << BUTTON_FILTER_SHIFT;
  buttonState = BUTTON_STATE_UP;
  buttonADCPressed = BUTTON_STATE_UP;
  buttonElapsed = 0;
  buttonInterrupts.begin(ButtonISR, 1000000 / BUTTON_FILTER_SAMPLERATE);
  buttonInterruptsEnabled = true;
}

/*****
  Purpose: Determine which UI button was pressed

  Parameter list:
    int valPin            the ADC value from analogRead()

  Return value;
    int                   -1 if not valid push button, index of push button if valid
*****/
int ProcessButtonPress(int valPin) {
  int switchIndex;

  if (valPin == BOGUS_PIN_READ) {  // Not valid press
#ifdef DEBUG_SW
//  NoActiveMenu();
  Serial.println("NAM BOGUS_PIN_READ");
#endif
    return -1;
  }

  if (valPin == MENU_OPTION_SELECT && menuStatus == NO_MENUS_ACTIVE) {
#ifdef DEBUG_SW
  NoActiveMenu();
  Serial.println("NAM #2");
#endif
    return -1;
  }

  for (switchIndex = 0; switchIndex < NUMBER_OF_SWITCHES; switchIndex++) {
    if (abs(valPin - EEPROMData.switchValues[switchIndex]) < WIGGLE_ROOM)  // ...because ADC does return exact values every time
    {
      return switchIndex;
    }
  }

  return -1;  // Really should never do this
}

/*****
  Purpose: Check for UI button press. If pressed, return the ADC value

  Parameter list:
    none

  Return value;
    int                   -1 if not valid push button, ADC value if valid
*****/
int ReadSelectedPushButton() {
  minPinRead = 0;
  int buttonReadOld = 1023;

  if (buttonInterruptsEnabled) {
    noInterrupts();
    buttonRead = buttonADCOut;

    /*
    Clear the button read.  If the button remains pressed, the ISR will reset the value nearly
    instantly.  Clearing the value here rather than in the ISR provides more consistent button
    press "feel" when calls to ReadSelectedPushButton have variable timing.
    */

    buttonADCOut = BUTTON_OUTPUT_UP;
    interrupts();
  } else {
    while (abs(minPinRead - buttonReadOld) > 3) {  // do averaging to smooth out the button response
      minPinRead = analogRead(BUSY_ANALOG_PIN);

      buttonRead = .1 * minPinRead + (1 - .1) * buttonReadOld;  // See expected values in next function.
      buttonReadOld = buttonRead;
    }
  }

  if (buttonRead > EEPROMData.switchValues[0] + WIGGLE_ROOM) {
    return -1;
  }
  minPinRead = buttonRead;
  if (!buttonInterruptsEnabled) {
    delay(100L);
  }

  return minPinRead;
}

/*****
  Purpose: Function is designed to route program control to the proper execution point in response to
           a button press.  To avoid duplication, display updates are generally handled in the routed routine.

  Parameter list:
    int vsl               the value from analogRead in loop()

  Return value;
    void
*****/
FLASHMEM void ExecuteButtonPress(int val) {
#ifdef DEBUG_SW
  Serial.print("ExecuteButtonPress TOP: val = ");
  Serial.println(val);
#endif
  switch (val) {
    case MENU_OPTION_SELECT:  // 0

      if(USE_FULL_MENU) {
        if (val == MENU_OPTION_SELECT && menuStatus == NO_MENUS_ACTIVE) {  // Pressed Select with no primary/secondary menu selected
#ifdef DEBUG_SW
  //NoActiveMenu();
  Serial.print("NAM #0: val = ");
  Serial.println(val);
#endif
          return;
        } else {
          menuStatus = PRIMARY_MENU_ACTIVE;
        }

        if (menuStatus == PRIMARY_MENU_ACTIVE) {  // Doing primary menu
          ErasePrimaryMenu();
          functionPtr[mainMenuIndex]();  // These are processed in MenuProc.cpp
          menuStatus = SECONDARY_MENU_ACTIVE;
          secondaryMenuIndex = -1;  // Reset secondary menu
        } else {
          if (menuStatus == SECONDARY_MENU_ACTIVE) {  // Doing primary menu
            menuStatus = PRIMARY_MENU_ACTIVE;
            mainMenuIndex = 0;
          }
        }

        EraseMenus();
      } else {
        MenuBarSelect();
      }
      break;

    case MAIN_MENU_UP:  // 1
      if(USE_FULL_MENU) {
        DrawMenuDisplay();                    // Draw selection box and primary menu
        SetPrimaryMenuIndex();                // Scroll through primary indexes and select one
        if(mainMenuIndex < TOP_MENU_COUNT - 1) {
          SetSecondaryMenuIndex();              // Use the primary index selection to redraw the secondary menu and set its index
          functionPtr[mainMenuIndex]();
        }

        tft.fillRect(1, SPECTRUM_TOP_Y + 1, 513, 379, RA8875_BLACK);          // Erase Menu box
        DrawSpectrumFrame();
        EraseMenus();
      } else {
        ShowMenuBar(0, 1);
      }
      break;

    case BAND_UP:  // 2
      ChangeBand(1);
      break;

    case ZOOM:  // 3
      SetZoom(spectrumZoom+1);
      break;

    case MAIN_MENU_DN:  // 4
      if(USE_FULL_MENU) {
        DrawMenuDisplay();                    // Draw selection box and primary menu
        SetPrimaryMenuIndex();                // Scroll through primary indexes and select one
        if(mainMenuIndex < TOP_MENU_COUNT - 1) {
          SetSecondaryMenuIndex();              // Use the primary index selection to redraw the secondary menu and set its index
          functionPtr[mainMenuIndex]();
        }

        tft.fillRect(1, SPECTRUM_TOP_Y + 1, 513, 379, RA8875_BLACK);          // Erase Menu box
        DrawSpectrumFrame();
        EraseMenus();
      } else {
        ShowMenuBar(TOP_MENU_COUNT - 2, -1);
      }
      break;

    case BAND_DN:  // 5
      ChangeBand(-1);
      break;

    case FILTER:  // 6
      ButtonFilter();
      break;

    case DEMODULATION:  // 7
      // change to the next demod mode
      ChangeDemodMode(bands[currentBand].mode + 1);
      break;

    case SET_MODE:  // 8
      // change to the next mode: SSB -> CW -> DATA -> SSB
      ChangeMode(xmtMode + 1);
      break;

    case NOISE_REDUCTION:  // 9
      ButtonNR();
      break;

    case NOTCH_FILTER:  // 10
      ButtonNotchFilter();
      UpdateInfoBoxItem(IB_ITEM_NOTCH);
      break;

    case NOISE_FLOOR:  // 11
      ToggleLiveNoiseFloorFlag();
      break;

    case FINE_TUNE_INCREMENT:  // 12
      ChangeFtIncrement(1);
      break;

    case DECODER_TOGGLE:  // 13
      decoderFlag = !decoderFlag;
      UpdateInfoBoxItem(IB_ITEM_DECODER);

      if(xmtMode == CW_MODE) {
        if(decoderFlag == ON) {
          // reduce waterfall height if we're decoding CW
          tft.fillRect(WATERFALL_L, YPIXELS - 35, WATERFALL_W, CHAR_HEIGHT + 3, RA8875_BLACK);  // Erase waterfall in decode area
          tft.writeTo(L2); // it's on layer 2 as well
          tft.fillRect(WATERFALL_L, YPIXELS - 35, WATERFALL_W, CHAR_HEIGHT + 3, RA8875_BLACK);  // Erase waterfall in decode area
          tft.writeTo(L1);
          wfRows = WATERFALL_H - CHAR_HEIGHT - 3;
        } else {
          // erase any decoded CW and return waterfall to normal
          tft.fillRect(WATERFALL_L, YPIXELS - 35, WATERFALL_W, CHAR_HEIGHT + 3, RA8875_BLACK);  // Erase waterfall in decode area
          wfRows = WATERFALL_H;
        }
      }
      break;

    case MAIN_TUNE_INCREMENT:  // 14
      ChangeFreqIncrement(-1);
      break;

    case RESET_TUNING:  // 15
      ResetTuning();
      break;

    case UNUSED_1:  // 16
      if(xmtMode == DATA_MODE) {
        switch (bands[currentBand].mode) {
          case DEMOD_PSK31:
            // try to load wav file
            if(setupPSK31Wav()) {
              // switch to play a wav file
              bands[currentBand].mode = DEMOD_PSK31_WAV;
              currentDataMode = DEMOD_PSK31_WAV;
              ShowOperatingStats();
            }
            break;

          case DEMOD_FT8:
            // try to load wav file
            if(setupFT8Wav()) {
              // switch to play a wav file
              bands[currentBand].mode = DEMOD_FT8_WAV;
              currentDataMode = DEMOD_FT8_WAV;
              ShowOperatingStats();
              syncFlag = true;
              ft8State = 2;
              UpdateInfoBoxItem(IB_ITEM_FT8);
            } else {
              // couldn't load wav file
              syncFlag = false;
              ft8State = 1;
              UpdateInfoBoxItem(IB_ITEM_FT8);
            }
            break;
        }
      } else {
        if (calOnFlag == 0) {
          ButtonFrequencyEntry();
        }
      }
      break;

    //case BEARING:  // 17
    case BEACON:     // 17
      //ButtonBearing();
      if(beaconFlag) {
        BeaconExit();
        beaconFlag = false;
      } else {
        BeaconInit();
        beaconFlag = true;
      }
      break;
  }
}

/*****
  Purpose: Error message if Select button pressed with no Menu active

  Parameter list:
    void

  Return value;
    void
*****/
FLASHMEM void NoActiveMenu() {
  tft.setFontScale((enum RA8875tsize)1);
  tft.setTextColor(RA8875_RED);
  tft.setCursor(PRIMARY_MENU_X + 1, MENUS_Y);
  tft.print("No menu selected");

  menuStatus = NO_MENUS_ACTIVE;
  mainMenuIndex = 0;
  secondaryMenuIndex = 0;
}
