#include "SDT.h"
#include "Button.h"
#include "ButtonProc.h"
#include "Display.h"
#include "DSP_Fn.h"
#include "EEPROM.h"
#include "Encoders.h"
#include "FFT.h"
#include "Filter.h"
#include "ft8.h"
#include "InfoBox.h"
#include "Menu.h"
#include "Noise.h"
#include "Process.h"
#include "psk31.h"
#include "Tune.h"
#include "Utility.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

#define MAX_FREQ_INDEX              8

bool lowerAudioFilterActive = false; // false - upper, true - lower audio filter active
int liveNoiseFloorFlag = OFF;

bool nfmBWFilterActive = false; // false - audio, true - demod BW filter active
bool ft8MsgSelectActive = false; // false - audio filters, true - msg select active

//------------------------- Local Variables ----------
bool save_last_frequency = false;
bool directFreqFlag = false;
long TxRxFreqOld;
int priorDemodMode; // preserves SSB demod mode between mode and band changes
int currentDataMode = DEMOD_PSK31; // preserves data mode between mode and band changes

//-------------------------------------------------------------------------------------------------------------
// Forwards
//-------------------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

/*****
  Purpose: To process a band increase/decrease

  Parameter list:
    void

  Return value:
    void
*****/
FLASHMEM void BandChange(int change) {
  // Added if so unused GPOs will not be touched
  if(currentBand < BAND_12M) {
    digitalWrite(bandswitchPins[currentBand], LOW);
  }

  if(xmtMode == DATA_MODE) {
    // restore old demodulation mode before we change bands
    bands[currentBand].mode = priorDemodMode;
  }

  currentBand += change;
  if(currentBand == NUMBER_OF_BANDS) {  // Incremented too far?
    currentBand = 0;                     // Yep. Roll to list front.
  }
  if(currentBand < 0) {                 // Incremented too far?
    currentBand = NUMBER_OF_BANDS - 1;  // Yep. Roll to list front.
  }

  NCOFreq = 0L;

  switch (activeVFO) {
    case VFO_A:
      if (save_last_frequency) {
        lastFrequencies[currentBandA][VFO_A] = TxRxFreq;
      } else {
        if (directFreqFlag) {
          lastFrequencies[currentBandA][VFO_A] = TxRxFreqOld;
          directFreqFlag = false;
        } else {
          lastFrequencies[currentBandA][VFO_A] = TxRxFreq;
        }
        TxRxFreqOld = TxRxFreq;
      }
      currentBandA = currentBand;
      centerFreq = TxRxFreq = currentFreqA = lastFrequencies[currentBandA][VFO_A];
      break;

    case VFO_B:
      if (save_last_frequency) {
        lastFrequencies[currentBandB][VFO_B] = TxRxFreq;
      } else {
        if (directFreqFlag) {
          lastFrequencies[currentBandB][VFO_B] = TxRxFreqOld;
          directFreqFlag = false;
        } else {
          lastFrequencies[currentBandB][VFO_B] = TxRxFreq;
        }
        TxRxFreqOld = TxRxFreq;
      }
      currentBandB = currentBand;
      centerFreq = TxRxFreq = currentFreqB = lastFrequencies[currentBandB][VFO_B];
      break;

    case VFO_SPLIT:
      DoSplitVFO();
      break;
  }

  EEPROMWrite();

  if(xmtMode == DATA_MODE) {
    priorDemodMode = bands[currentBand].mode; // save demod mode for restoration later

    switch(currentDataMode) {
      case DEMOD_PSK31:
      case DEMOD_PSK31_WAV:
        break;

      case DEMOD_FT8:
      case DEMOD_FT8_WAV:
        syncFlag = false;
        ft8State = 1;
        UpdateInfoBoxItem(IB_ITEM_FT8);
        break;
    }

    bands[currentBand].mode = currentDataMode;
  }

  SetBand();

  if(currentBand < BAND_12M) {
    digitalWrite(bandswitchPins[currentBand], HIGH);
  }
}

/*****
  Purpose: Toggle which filter is adjusted by filter encoder

  Parameter list:
    void

  Return value:
    void
*****/
FLASHMEM void ButtonFilter() {
  switch(bands[currentBand].mode) {
    case DEMOD_NFM:
    // Active filter in NFM demod mode:
    // At startup:  high audio
    // 1st press:   NFM BW
    // 2nd press:   low audio
    // 3rd press:   high audio
    // repeat @ 1
    if(nfmBWFilterActive) {
      nfmBWFilterActive = !nfmBWFilterActive;
      lowerAudioFilterActive = !lowerAudioFilterActive;
      DisplayMessages();
    } else {
      if(lowerAudioFilterActive) {
        lowerAudioFilterActive = !lowerAudioFilterActive;
      } else {
        nfmBWFilterActive = !nfmBWFilterActive;
      }
    }
    break;

  case DEMOD_FT8:
  case DEMOD_FT8_WAV:
    // Filter sequence in FT8 mode:
    // At startup:  high audio
    // 1st press:   FT8 msg selection
    // 2nd press:   low audio
    // 3rd press:   high audio
    // repeat @ 1
    if(ft8MsgSelectActive) {
      ft8MsgSelectActive = !ft8MsgSelectActive;
      lowerAudioFilterActive = !lowerAudioFilterActive;
      DisplayMessages();
    } else {
      if(lowerAudioFilterActive) {
        lowerAudioFilterActive = !lowerAudioFilterActive;
      } else {
        ft8MsgSelectActive = !ft8MsgSelectActive;
      }
    }
    break;

  default:
    lowerAudioFilterActive = !lowerAudioFilterActive;
    break;
  }

  ShowBandwidthBarValues(); // change color of active filter value
}

/*****
  Purpose: Change the demodulation mode
          *** TODO: rework this if modes are expanded ***
  Parameter list:
    void

  Return value:
    void
*****/
FLASHMEM void ChangeDemodMode(int mode) {
  // wrap up current demod mode
  if(xmtMode == DATA_MODE) {
    switch(currentDataMode) {
      case DEMOD_PSK31:
        // try to set up FT8
        if(setupFT8()) {
          // FT8 set up successful
          bands[currentBand].mode = DEMOD_FT8;
          currentDataMode = DEMOD_FT8;
          ShowOperatingStats();
          syncFlag = false;
          ft8State = 1;
          UpdateInfoBoxItem(IB_ITEM_FT8);
        }
        break;

      case DEMOD_FT8:
        exitFT8();
        UpdateInfoBoxItem(IB_ITEM_FT8);
        bands[currentBand].mode = DEMOD_PSK31;
        currentDataMode = DEMOD_PSK31;
        ShowOperatingStats();
        break;

      case DEMOD_PSK31_WAV:
      case DEMOD_FT8_WAV:
      default:
        break;
    }

    return;
  }

  bands[currentBand].mode = mode;
  if(bands[currentBand].mode > DEMOD_MAX) {
    bands[currentBand].mode = DEMOD_MIN;
  }
  if(bands[currentBand].mode < DEMOD_MIN) {
    bands[currentBand].mode = DEMOD_MAX;
  }

  // skip Data modes (*** these assume we're cycling through up or down ***)
  if(bands[currentBand].mode == DEMOD_PSK31_WAV) {
    bands[currentBand].mode += 4;
  }
  if(bands[currentBand].mode == DEMOD_FT8) {
    bands[currentBand].mode -= 4;
  }

  SetupMode();
  //UpdateBWFilters();

  ShowOperatingStats();
  ShowBandwidthBarValues();
  DrawBandwidthBar();
}

/*****
  Purpose: Sets operating mode, SSB, CW or Data

  Parameter list:
    void

  Return value:
    void
*****/
FLASHMEM void ChangeMode(int mode) {
  if(mode > 2) {
    mode = 0;
  }
  if(mode < 0) {
    mode = 2;
  }

  // wrap up current mode
  switch(xmtMode) {
    case SSB_MODE:
      // save demod mode if changing to Data mode
      if(mode == DATA_MODE) {
        priorDemodMode = bands[currentBand].mode; // save demod mode for restoration later
      }
      break;

    case CW_MODE:
      // save demod mode if changing to Data mode
      if(mode == DATA_MODE) {
        priorDemodMode = bands[currentBand].mode; // save demod mode for restoration later
      }
      break;

    case DATA_MODE:
      // return demod mode to previous mode
      bands[currentBand].mode = priorDemodMode;

      if(bands[currentBand].mode == DEMOD_FT8) {
        exitFT8();
        UpdateInfoBoxItem(IB_ITEM_FT8);
      } else {
        exitPSK31();
      }
      break;
  }

  // set new mode and update
  xmtMode = mode;
  switch(xmtMode) {
    case SSB_MODE:
      break;

    case CW_MODE:
      // reduce waterfall height if we're decoding CW
      if(decoderFlag == ON) {
        tft.fillRect(WATERFALL_L, YPIXELS - 35, WATERFALL_W, CHAR_HEIGHT + 3, RA8875_BLACK);  // Erase waterfall in decode area
        tft.writeTo(L2); // it's on layer 2 as well
        tft.fillRect(WATERFALL_L, YPIXELS - 35, WATERFALL_W, CHAR_HEIGHT + 3, RA8875_BLACK);  // Erase waterfall in decode area
        tft.writeTo(L1);
        wfRows = WATERFALL_H - CHAR_HEIGHT - 3;
      }
      break;

    case DATA_MODE:
      if(currentDataMode == DEMOD_FT8) {
        // try to set up FT8
        if(setupFT8()) {
          // FT8 set up successful
          bands[currentBand].mode = DEMOD_FT8;
        } else {
          // can't set up FT8, move to psk31
          bands[currentBand].mode = DEMOD_PSK31;
        }
      } else {
        setupPSK31();
        bands[currentBand].mode = DEMOD_PSK31;
      }
      break;
  }

  SetupMode();
  UpdateCWFilter();
  ShowOperatingStats();
  ShowBandwidthBarValues();
  DrawBandwidthBar();
}

/*****
  Purpose: To process select noise reduction

  Parameter list:
    void

  Return value:
    void
*****/
FLASHMEM void ButtonNR() {
  nrOptionSelect++;
  if (nrOptionSelect > NR_OPTIONS) {
    nrOptionSelect = 0;
  }

  UpdateInfoBoxItem(IB_ITEM_FILTER);
}

/*****
  Purpose: To set the notch filter

  Parameter list:
    void

  Return value:
    void
*****/
FLASHMEM void ButtonNotchFilter() {
  ANR_notchOn = !ANR_notchOn;
  delay(100L);
}


/*****
  Purpose:  Toggles flag to allow quick setting of noise floor in spectrum display.
            Saves current noise floor to EEPROM when toggled to Off.  A band's
            current noise floor isn't preserved in EEPROM if you switch bands while
            toggle is On.

  Parameter list:
    void

  Return value;
    void
*****/
FLASHMEM void ToggleLiveNoiseFloorFlag() {
  // save final noise floor setting if toggling flag off
  if(liveNoiseFloorFlag) {
    EEPROMData.currentNoiseFloor[currentBand]  = currentNoiseFloor[currentBand];
    EEPROMWrite();
  }

  liveNoiseFloorFlag = !liveNoiseFloorFlag;
  UpdateInfoBoxItem(IB_ITEM_FLOOR);
}

/*****
  Purpose: To process a frequency increment button push

  Parameter list:
    void

  Return value:
    void
*****/
FLASHMEM void ChangeFreqIncrement(int change) {
  long incrementValues[] = { 10, 50, 100, 250, 1000, 10000, 100000, 1000000 };

  tuneIndex += change;
  if (tuneIndex < 0) {
    tuneIndex = MAX_FREQ_INDEX - 1;
  }
  if (tuneIndex >= MAX_FREQ_INDEX) {
    tuneIndex = 0;
  }

  freqIncrement = incrementValues[tuneIndex];

  UpdateInfoBoxItem(IB_ITEM_TUNE);
}

/*****
  Purpose: To process a fine tune frequency increment button push

  Parameter list:
    void

  Return value;
    void
*****/
FLASHMEM void ChangeFtIncrement(int change) {
  long selectFT[] = { 10, 50, 250, 500 };

  ftIndex += change;
  if (ftIndex > 3) {
    ftIndex = 0;
  }
  if (ftIndex < 0) {
    ftIndex = 3;
  }

  ftIncrement = selectFT[ftIndex];

  UpdateInfoBoxItem(IB_ITEM_FINE);
}

/*****
  Purpose: Direct Frequency Entry

  Parameter list:
    void

  Return value;
    void
    Base Code courtesy of Harry  GM3RVL
*****/
FLASHMEM void ButtonFrequencyEntry() {
  TxRxFreqOld = TxRxFreq;

#define show_FEHelp
  bool doneFE = false;                         // set to true when a valid frequency is entered
  long enteredF = 0L;                          // desired frequency
  char strF[6] = { ' ', ' ', ' ', ' ', ' ' };  // container for frequency string during entry
  String stringF;
  int valPin;
  int key;
  int numdigits = 0;  // number of digits entered
  int pushButtonSwitchIndex;
  lastFrequencies[currentBand][activeVFO] = TxRxFreq;
  //save_last_frequency = false;                    // prevents crazy frequencies when you change bands/save_last_frequency = true;
  // Arrays for allocating values associated with keys and switches - choose whether USB keypad or analogue switch matrix
  // USB keypad and analogue switch matrix
  const char *DE_Band[] = {"80m","40m","20m","17m","15m","12m","10m"};
  const char *DE_Flimit[] = {"4.5","9","16","26","26","30","30"};
  int numKeys[] = { 0x0D, 0x7F, 0x58,  // values to be allocated to each key push
                    0x37, 0x38, 0x39,
                    0x34, 0x35, 0x36,
                    0x31, 0x32, 0x33,
                    0x30, 0x7F, 0x7F,
                    0x7F, 0x7F, 0x99 };
  EraseMenus();
#ifdef show_FEHelp
  int keyCol[] = { YELLOW, RED, RED,
                   RA8875_BLUE, RA8875_GREEN, RA8875_GREEN,
                   RA8875_BLUE, RA8875_BLUE, RA8875_BLUE,
                   RED, RED, RED,
                   RED, RA8875_BLACK, RA8875_BLACK,
                   YELLOW, YELLOW, RA8875_BLACK };
  int textCol[] = { RA8875_BLACK, RA8875_WHITE, RA8875_WHITE,
                    RA8875_WHITE, RA8875_BLACK, RA8875_BLACK,
                    RA8875_WHITE, RA8875_WHITE, RA8875_WHITE,
                    RA8875_WHITE, RA8875_WHITE, RA8875_WHITE,
                    RA8875_WHITE, RA8875_WHITE, RA8875_WHITE,
                    RA8875_BLACK, RA8875_BLACK, RA8875_WHITE };
  const char *key_labels[] = { "<", "", "X",
                               "7", "8", "9",
                               "4", "5", "6",
                               "1", "2", "3",
                               "0", "D", "",
                               "", "", "S" };

#define KEYPAD_LEFT 350
#define KEYPAD_TOP SPECTRUM_TOP_Y + 35
#define KEYPAD_WIDTH 150
#define KEYPAD_HEIGHT 300
#define BUTTONS_LEFT KEYPAD_LEFT + 30
#define BUTTONS_TOP KEYPAD_TOP + 30
#define BUTTONS_SPACE 45
#define BUTTONS_RADIUS 15
#define TEXT_OFFSET -8

  tft.writeTo(L1);
  tft.fillRect(WATERFALL_L, SPECTRUM_TOP_Y + 1, WATERFALL_W, WATERFALL_BOTTOM - SPECTRUM_TOP_Y, RA8875_BLACK);  // Make space for FEInfo
  tft.fillRect(WATERFALL_W, SPEC_BOX_LABELS - 10, 15, 30, RA8875_BLACK);
  tft.writeTo(L2);

  tft.fillRect(WATERFALL_L, SPECTRUM_TOP_Y + 1, WATERFALL_W, WATERFALL_BOTTOM - SPECTRUM_TOP_Y, RA8875_BLACK);

  tft.setCursor(centerLine - 140, SPEC_BOX_LABELS);
  tft.drawRect(SPECTRUM_LEFT_X - 1, SPECTRUM_TOP_Y, WATERFALL_W + 2, 360, RA8875_YELLOW);  // Spectrum box

  // Draw keypad box
  tft.fillRect(KEYPAD_LEFT, KEYPAD_TOP, KEYPAD_WIDTH, KEYPAD_HEIGHT, DARKGREY);
  // put some circles
  tft.setFontScale((enum RA8875tsize)1);
  for (unsigned i = 0; i < 6; i++) {
    for (unsigned j = 0; j < 3; j++) {
      tft.fillCircle(BUTTONS_LEFT + j * BUTTONS_SPACE, BUTTONS_TOP + i * BUTTONS_SPACE, BUTTONS_RADIUS, keyCol[j + 3 * i]);
      tft.setCursor(BUTTONS_LEFT + j * BUTTONS_SPACE + TEXT_OFFSET, BUTTONS_TOP + i * BUTTONS_SPACE - 18);
      tft.setTextColor(textCol[j + 3 * i]);
      tft.print(key_labels[j + 3 * i]);
    }
  }
  tft.setFontScale((enum RA8875tsize)0);
  tft.setCursor(WATERFALL_L + 20, SPECTRUM_TOP_Y + 50);
  tft.setTextColor(RA8875_WHITE);
  tft.print("Direct Frequency Entry");
  tft.setCursor(WATERFALL_L + 20, SPECTRUM_TOP_Y + 100);
  tft.print("<   Apply entered frequency");
  tft.setCursor(WATERFALL_L + 20, SPECTRUM_TOP_Y + 130);
  tft.print("X   Exit without changing frequency");
  tft.setCursor(WATERFALL_L + 20, SPECTRUM_TOP_Y + 160);
  tft.print("D   Delete last digit");
  tft.setCursor(WATERFALL_L + 20, SPECTRUM_TOP_Y + 190);
  tft.print("S   Save Direct to Last Freq. ");
  tft.setCursor(WATERFALL_L + 20, SPECTRUM_TOP_Y + 240);
  tft.print("Direct Entry was called from ");
  tft.print(DE_Band[currentBand]);
  tft.print(" band");
  tft.setCursor(WATERFALL_L + 20, SPECTRUM_TOP_Y + 270);
  tft.print("Frequency response limited above ");
  tft.print(DE_Flimit[currentBand]);
  tft.print("MHz");
  tft.setCursor(WATERFALL_L + 20, SPECTRUM_TOP_Y + 300);
  tft.print("For widest direct entry frequency range");
  tft.setCursor(WATERFALL_L + 20, SPECTRUM_TOP_Y + 330);
  tft.print("call from 12m or 10m band");

#endif

  tft.writeTo(L2);

  tft.setFontScale((enum RA8875tsize)1);
  tft.setTextColor(RA8875_WHITE);
  tft.setCursor(10, 0);
  tft.print("Enter Frequency");

  tft.fillRect(SECONDARY_MENU_X + 20, MENUS_Y, EACH_MENU_WIDTH + 10, CHAR_HEIGHT, RA8875_MAGENTA);
  //tft.setTextColor(RA8875_WHITE);
  tft.setTextColor(RA8875_BLACK);       // JJP 7/17/23
  tft.setCursor(SECONDARY_MENU_X + 21, MENUS_Y);
  tft.print("kHz or MHz:");
  tft.setFontScale((enum RA8875tsize)0);
  tft.setCursor(WATERFALL_L + 50, SPECTRUM_TOP_Y + 260);
  tft.print("Save Direct to Last Freq.= ");
  tft.setCursor(WATERFALL_L + 270, SPECTRUM_TOP_Y + 190);
  if (save_last_frequency) {
    tft.setTextColor(RA8875_GREEN);
    tft.print("On");
  } else {
    tft.setTextColor(RA8875_MAGENTA);
    tft.print("Off");
  }

  while (doneFE == false) {
    valPin = ReadSelectedPushButton();                     // Poll UI push buttons
    if (valPin != BOGUS_PIN_READ) {                        // If a button was pushed...
      pushButtonSwitchIndex = ProcessButtonPress(valPin);  // Winner, winner...chicken dinner!
      key = numKeys[pushButtonSwitchIndex];
      switch (key) {
        case 0x7F:  // erase last digit =127
          if (numdigits != 0) {
            numdigits--;
            strF[numdigits] = ' ';
          }
          break;
        case 0x58:  // Exit without updating frequency =88
          doneFE = true;
          break;
        case 0x0D:  // Apply the entered frequency (if valid) =13
          stringF = String(strF);
          enteredF = stringF.toInt();
          if ((numdigits == 1) || (numdigits == 2)) {
            enteredF = enteredF * 1000000;
          }
          if ((numdigits == 4) || (numdigits == 5)) {
            enteredF = enteredF * 1000;
          }
          if ((enteredF > 30000000) || (enteredF < 1250000)) {
            stringF = "     ";  // 5 spaces
            stringF.toCharArray(strF, stringF.length());
            numdigits = 0;
          } else {
            doneFE = true;
          }
          break;
        case 0x99:
          save_last_frequency = !save_last_frequency;
          tft.setFontScale((enum RA8875tsize)0);
          tft.fillRect(WATERFALL_L + 269, SPECTRUM_TOP_Y + 190, 50, CHAR_HEIGHT, RA8875_BLACK);
          tft.setCursor(WATERFALL_L + 260, SPECTRUM_TOP_Y + 190);
          if (save_last_frequency) {
            tft.setTextColor(RA8875_GREEN);
            tft.print("On");
          } else {
            tft.setTextColor(RA8875_MAGENTA);
            tft.print("Off");
          }
          break;
        default:
          if ((numdigits == 5) || ((key == 0x30) & (numdigits == 0))) {
          } else {
            strF[numdigits] = char(key);
            numdigits++;
          }
          break;
      }
      tft.setTextColor(RA8875_WHITE);
      tft.setFontScale((enum RA8875tsize)1);
      tft.fillRect(SECONDARY_MENU_X + 195, MENUS_Y, 85, CHAR_HEIGHT, RA8875_MAGENTA);
      tft.setCursor(SECONDARY_MENU_X + 200, MENUS_Y);
      tft.print(strF);
      delay(250);  // only for analogue switch matrix
    }
  }

  if (key != 0x58) {
    TxRxFreq = enteredF;
  }

  NCOFreq = 0L;
  directFreqFlag = true;
  centerFreq = TxRxFreq;
  fineTuneFlag = true;  // Put back in so tuning bar is refreshed
  SetFreq();  // Used here instead of fineTuneFlag

  if (save_last_frequency) {
    lastFrequencies[currentBand][activeVFO] = enteredF;
  } else {
    lastFrequencies[currentBand][activeVFO] = TxRxFreqOld;
  }
  tft.fillRect(0, 0, 799, 479, RA8875_BLACK);   // Clear layer 2
  tft.writeTo(L1);

  EEPROMWrite();

  SetBand();
  RedrawDisplayScreen(); // *** we can get rid of this by adjusting above to not write to right portion of screen ***
}
