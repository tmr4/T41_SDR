#include "SDT.h"

int buttonRead = 0;
int minPinRead = 1024;
int secondaryMenuChoiceMade;
long incrementValues[] = { 10, 50, 100, 250, 1000, 10000, 100000, 1000000 };

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

/*****
  Purpose: ISR to read button ADC and detect button presses

  Parameter list:
    none
  Return value;
    void
*****/

void ButtonISR() {
  int filteredADCValue;

  buttonFilterRegister = buttonFilterRegister - (buttonFilterRegister >> BUTTON_FILTER_SHIFT) + analogRead(BUSY_ANALOG_PIN);
  filteredADCValue = (int)(buttonFilterRegister >> BUTTON_FILTER_SHIFT);

  switch (buttonState) {
    case BUTTON_STATE_UP:
      if (filteredADCValue <= EEPROMData.buttonThresholdPressed) {
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
      if (filteredADCValue >= EEPROMData.buttonThresholdReleased) {
        buttonState = BUTTON_STATE_UP;
      } else if (EEPROMData.buttonRepeatDelay != 0) {  // buttonRepeatDelay of 0 disables repeat
        if (buttonElapsed < EEPROMData.buttonRepeatDelay) {
          buttonElapsed += BUTTON_USEC_PER_ISR;
        } else {
          buttonADCOut = buttonADCPressed;
          buttonElapsed = 0;
        }
      }

      break;
  }
}

/*****
  Purpose: Starts button IntervalTimer and toggles subsequent button
           functions into interrupt mode.

  Parameter list:
    none
  Return value;
    void
*****/

void EnableButtonInterrupts() {
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
    return -1;
  }

  if (valPin == MENU_OPTION_SELECT && menuStatus == NO_MENUS_ACTIVE) {
    NoActiveMenu();
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

  if (buttonRead > EEPROMData.switchValues[0] + WIGGLE_ROOM) {  //AFP 10-29-22 per Jack Wilson
    return -1;
  }
  minPinRead = buttonRead;
  if (!buttonInterruptsEnabled) {
    MyDelay(100L);
  }

  return minPinRead;
}

/*****
  Purpose: Function is designed to route program control to the proper execution point in response to
           a button press.

  Parameter list:
    int vsl               the value from analogRead in loop()

  Return value;
    void
*****/
void ExecuteButtonPress(int val) {
  switch (val) {
    case MENU_OPTION_SELECT:  // 0

      if(USE_FULL_MENU) {
        if (val == MENU_OPTION_SELECT && menuStatus == NO_MENUS_ACTIVE) {  // Pressed Select with no primary/secondary menu selected
          NoActiveMenu();
          return;
        } else {
          menuStatus = PRIMARY_MENU_ACTIVE;
        }

        if (menuStatus == PRIMARY_MENU_ACTIVE) {  // Doing primary menu
          ErasePrimaryMenu();
          secondaryMenuChoiceMade = functionPtr[mainMenuIndex]();  // These are processed in MenuProc.cpp
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
        switch (menuStatus) {
          case NO_MENUS_ACTIVE:
            NoActiveMenu();
            break;

          case PRIMARY_MENU_ACTIVE:
            if(mainMenuIndex == TOP_MENU_COUNT - 1) {
              menuStatus = NO_MENUS_ACTIVE;
              mainMenuIndex = 0;
              EraseMenus();
            } else {
              menuStatus = SECONDARY_MENU_ACTIVE;
              secondaryMenuIndex = 0;
              subMenuMaxOptions = secondaryMenuCount[mainMenuIndex];
              //secondaryMenuIndex = SubmenuSelect(secondaryChoices[mainMenuIndex], secondaryMenuCount[mainMenuIndex], 0);
              ShowMenu(&secondaryChoices[mainMenuIndex][secondaryMenuIndex], SECONDARY_MENU);
            }
            break;

          case SECONDARY_MENU_ACTIVE:
            if(secondaryMenuIndex == secondaryMenuCount[mainMenuIndex] - 1) {
              // cancel selected
              menuStatus = PRIMARY_MENU_ACTIVE;
//              ShowMenu(&topMenus[mainMenuIndex], PRIMARY_MENU);
              EraseSecondaryMenu();
            } else {
              secondaryMenuChoiceMade = functionPtr[mainMenuIndex]();
              EraseMenus();
              menuStatus = NO_MENUS_ACTIVE;
            }
            break;

          default:
            break;
        }
      }
      break;

    case MAIN_MENU_UP:  // 1
      if(USE_FULL_MENU) {
        DrawMenuDisplay();                    // Draw selection box and primary menu
        SetPrimaryMenuIndex();                // Scroll through primary indexes and select one
        if(mainMenuIndex == TOP_MENU_COUNT - 1) {
          EraseMenus();
          break;
        }
        SetSecondaryMenuIndex();              // Use the primary index selection to redraw the secondary menu and set its index
        secondaryMenuChoiceMade = functionPtr[mainMenuIndex](); 
        tft.fillRect(1, SPECTRUM_TOP_Y + 1, 513, 379, RA8875_BLACK);          // Erase Menu box
        DrawSpectrumDisplayContainer();
        EraseMenus();
      } else {
        if (menuStatus == NO_MENUS_ACTIVE) {
          menuStatus = PRIMARY_MENU_ACTIVE;
          mainMenuIndex = 0;
          ShowMenu(&topMenus[mainMenuIndex], PRIMARY_MENU);
        } else {
          ButtonMenuUp();

          switch (menuStatus) {
            case PRIMARY_MENU_ACTIVE:
//              EraseMenus();
              ShowMenu(&topMenus[mainMenuIndex], PRIMARY_MENU);
              break;

            case SECONDARY_MENU_ACTIVE:
              ShowMenu(&secondaryChoices[mainMenuIndex][secondaryMenuIndex], SECONDARY_MENU);
              break;

            default:
              break;
          }
        }
      }
      break;

    case BAND_UP:  // 2
      EraseMenus();
      if(currentBand < 5) digitalWrite(bandswitchPins[currentBand], LOW);  // Added if so unused GPOs will not be touched.  KF5N October 16, 2023.
      ButtonBandIncrease();
      if(currentBand < 5) digitalWrite(bandswitchPins[currentBand], HIGH);
      BandInformation();
      NCOFreq = 0L;
      DrawBandWidthIndicatorBar();  // AFP 10-20-22
      SetFreq();
      ShowSpectrum();
      break;

    case ZOOM:  // 3
      menuStatus = PRIMARY_MENU_ACTIVE;
      EraseMenus();
      ButtonZoom();
      break;

    case MAIN_MENU_DN:  // 4
      if(USE_FULL_MENU) {
        DrawMenuDisplay();                    // Draw selection box and primary menu
        SetPrimaryMenuIndex();                // Scroll through primary indexes and select one
        if(mainMenuIndex == TOP_MENU_COUNT - 1) {
          EraseMenus();
          break;
        }
        SetSecondaryMenuIndex();              // Use the primary index selection to redraw the secondary menu and set its index
        secondaryMenuChoiceMade = functionPtr[mainMenuIndex](); 
        tft.fillRect(1, SPECTRUM_TOP_Y + 1, 513, 379, RA8875_BLACK);          // Erase Menu box
        DrawSpectrumDisplayContainer();
        EraseMenus();
      } else {
        if (menuStatus == NO_MENUS_ACTIVE) {
          menuStatus = PRIMARY_MENU_ACTIVE;
          mainMenuIndex = TOP_MENU_COUNT - 2;
          ShowMenu(&topMenus[mainMenuIndex], PRIMARY_MENU);
        } else {
          ButtonMenuDown();

          switch (menuStatus) {
            case PRIMARY_MENU_ACTIVE:
              EraseMenus();
              ShowMenu(&topMenus[mainMenuIndex], PRIMARY_MENU);
              break;

            case SECONDARY_MENU_ACTIVE:
              ShowMenu(&secondaryChoices[mainMenuIndex][secondaryMenuIndex], SECONDARY_MENU);
              break;

            default:
              break;
          }
        }
      }
      break;

    case BAND_DN:  // 5
      EraseMenus();
      ShowSpectrum();  //Now calls ProcessIQData and Encoders calls
      if(currentBand < 5) digitalWrite(bandswitchPins[currentBand], LOW);
      ButtonBandDecrease();
      if(currentBand < 5) digitalWrite(bandswitchPins[currentBand], HIGH);
      BandInformation();
      NCOFreq = 0L;
      DrawBandWidthIndicatorBar();  //AFP 10-20-22
      break;

    case FILTER:  // 6
      EraseMenus();
      ButtonFilter();
      break;

    case DEMODULATION:  // 7
      EraseMenus();
      ButtonDemodMode();
      break;

    case SET_MODE:  // 8
      ButtonMode();
      ShowSpectrumdBScale();
      break;

    case NOISE_REDUCTION:  // 9
      ButtonNR();
      break;

    case NOTCH_FILTER:  // 10
      ButtonNotchFilter();
      UpdateNotchField();
      break;

    case NOISE_FLOOR:  // 11
      ButtonSetNoiseFloor();
      break;

    case FINE_TUNE_INCREMENT:  // 12
      UpdateIncrementField();
      break;

    case DECODER_TOGGLE:  // 13
      decoderFlag = !decoderFlag;
      UpdateDecoderField();
      break;

    case MAIN_TUNE_INCREMENT:  // 14
      ButtonFreqIncrement();
      UpdateEEPROMSyncIndicator(0);
      break;

    case RESET_TUNING:  // 15   AFP 10-11-22
      ResetTuning();    // AFP 10-11-22
      break;            // AFP 10-11-22

    case UNUSED_1:  // 16
      if (calOnFlag == 0) {
        ButtonFrequencyEntry();
      }
      break;

    case BEARING:  // 17  // AFP 10-11-22
      int buttonIndex, doneViewing, valPin;
      float retVal;
      
      tft.clearScreen(RA8875_BLACK);

      DrawKeyboard();
      CaptureKeystrokes();
      retVal = BearingHeading(keyboardBuffer);

     
      if (retVal != -1.0) {                           // We have valid country
        bmpDraw((char *)myMapFiles[selectedMapIndex].mapNames, IMAGE_CORNER_X, IMAGE_CORNER_Y);
        doneViewing = false;
      } else {
        tft.setTextColor(RA8875_RED);
        tft.setCursor(380 - (17 * tft.getFontWidth(0)) / 2, 240);   // Center message
        tft.print("Country not found");  
        tft.setTextColor(RA8875_WHITE);        
      }
      while (true) {
        valPin = ReadSelectedPushButton();            // Poll UI push buttons
        MyDelay(100L);
        if (valPin != BOGUS_PIN_READ) {               // If a button was pushed...
          buttonIndex = ProcessButtonPress(valPin);   // Winner, winner...chicken dinner!
          switch (buttonIndex) {
            case BEARING:                             // Pressed puchbutton 18
              doneViewing = true;
              break;
            default:
              break;
          }
        }

        if (doneViewing == true) {
            //tft.clearMemory();          // Need to clear overlay too
            //tft.writeTo(L2);
            //tft.fillWindow();
          break;
        }
      }
      RedrawDisplayScreen();
      ShowFrequency();
      DrawFrequencyBarValue();
      
      break;
  }
}


/*****
  Purpose: To process a band decrease button push

  Parameter list:
    void

  Return value:
    void
*****/
void ButtonFreqIncrement() {
  tuneIndex--;
  if (tuneIndex < 0)
    tuneIndex = MAX_FREQ_INDEX - 1;
  freqIncrement = incrementValues[tuneIndex];
  //UpdateEEPROMSyncIndicator(0);   //  JJP 7/25/23
  DisplayIncrementField();
}


/*****
  Purpose: Error message if Select button pressed with no Menu active

  Parameter list:
    void

  Return value;
    void
*****/
void NoActiveMenu() {
  tft.setFontScale((enum RA8875tsize)1);
  tft.setTextColor(RA8875_RED);
  tft.setCursor(PRIMARY_MENU_X + 1, MENUS_Y);
  tft.print("No menu selected");

  menuStatus = NO_MENUS_ACTIVE;
Serial.print("at #4: mainMenuIndex = ");
Serial.println(mainMenuIndex);
  mainMenuIndex = 0;
  secondaryMenuIndex = 0;
}
