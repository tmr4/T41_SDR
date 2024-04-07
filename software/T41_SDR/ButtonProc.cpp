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
#include "Tune.h"
#include "Utility.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

#define MAX_ZOOM_ENTRIES      5

bool lowerAudioFilterActive = false; // false - upper, true - lower audio filter active
int liveNoiseFloorFlag = OFF;

bool nfmBWFilterActive = false; // false - audio, true - demod BW filter active

//------------------------- Local Variables ----------
bool save_last_frequency = false;
bool directFreqFlag = false;
long TxRxFreqOld;

//-------------------------------------------------------------------------------------------------------------
// Forwards
//-------------------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

/*****
  Purpose: To process a menu down button push

  Parameter list:
    void

  Return value:
    void
*****/
void ButtonMenuDown() {
  switch (menuStatus) {
    case PRIMARY_MENU_ACTIVE:
      mainMenuIndex++;
      if (mainMenuIndex == TOP_MENU_COUNT) {  // At last menu option, so...
        mainMenuIndex = 0;                    // ...wrap around to first menu option
      }
      break;

    case SECONDARY_MENU_ACTIVE:
      secondaryMenuIndex++;
      if (secondaryMenuIndex == subMenuMaxOptions) {  // Same here...
        secondaryMenuIndex = 0;
      }
      break;

    default:
      break;
  }
}

/*****
  Purpose: To process a menu up button push

  Parameter list:
    void

  Return value:
    void
*****/
void ButtonMenuUp() {
  switch (menuStatus) {
    case PRIMARY_MENU_ACTIVE:
      mainMenuIndex--;
      if (mainMenuIndex < 0) {               // At last menu option, so...
        mainMenuIndex = TOP_MENU_COUNT - 1;  // ...wrap around to first menu option
      }
      break;

    case SECONDARY_MENU_ACTIVE:
      secondaryMenuIndex--;
      if (secondaryMenuIndex < 0) {  // Same here...
        secondaryMenuIndex = subMenuMaxOptions - 1;
      }
      break;

    default:
      break;
  }
}
//==================  AFP 09-27-22

/*****
  Purpose: To process a band increase/decrease button push

  Parameter list:
    void

  Return value:
    void
*****/
void ButtonBandChange() {
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

  SetBand();
}

/*****
  Purpose: Change the horizontal scale of the frequency display

  Parameter list:
    void

  Return value:
    void
*****/
void ButtonZoom() {
  spectrum_zoom++;

  if (spectrum_zoom == MAX_ZOOM_ENTRIES) {
    spectrum_zoom = 0;
  }

  SetZoom();
}

/*****
  Purpose: Toggle which filter is adjusted by filter encoder

  Parameter list:
    void

  Return value:
    void
*****/
void ButtonFilter() {
  if (bands[currentBand].mode == DEMOD_NFM) {
    // Active filter in NFM demod mode:
    // At startup:  high audio
    // 1st press:   NFM BW
    // 2nd press:   low audio
    // 3rd press:   high audio
    // repeat @ 1
    if(nfmBWFilterActive) {
      nfmBWFilterActive = !nfmBWFilterActive;
      lowerAudioFilterActive = !lowerAudioFilterActive;
    } else {
      if(lowerAudioFilterActive) {
        lowerAudioFilterActive = !lowerAudioFilterActive;
      } else {
        nfmBWFilterActive = !nfmBWFilterActive;
      }
    }
  }
  else {
    lowerAudioFilterActive = !lowerAudioFilterActive;
  }

  ShowBandwidthBarValues(); // change color of active filter value
}

/*****
  Purpose: Cycle to next demodulation mode

  Parameter list:
    void

  Return value:
    void
*****/
void ButtonDemodMode() {
  bands[currentBand].mode++;
  if(bands[currentBand].mode > DEMOD_MAX) {
    bands[currentBand].mode = DEMOD_MIN;  // cycle thru demod modes
  }

#ifdef FT8
  if(bands[currentBand].mode == DEMOD_FT8_WAV) {
    // continue to FT8 if we've just incremented to FT8 wave
    bands[currentBand].mode++;

    if(!ft8Init) {
      if(setupFT8()) {
        ft8Init = true;
      } else {
        // can't set up FT8, just continue to next mode
        bands[currentBand].mode++;
      }
    }

    // set up message area
    tft.fillRect(WATERFALL_L, YPIXELS - 20 * 6, WATERFALL_W, 20 * 6 + 3, RA8875_BLACK);  // Erase waterfall in decode area
    tft.writeTo(L2); // it's on layer 2 as well
    tft.fillRect(WATERFALL_L, YPIXELS - 20 * 6, WATERFALL_W, 20 * 6 + 3, RA8875_BLACK);  // Erase waterfall in decode area
    tft.writeTo(L1);
    wfRows = WATERFALL_H - 20 * 6 - 3;
  } else {
    // erase any decoded messages if we're coming from FT8
    if(bands[currentBand].mode - 1 == DEMOD_FT8) {
      exitFT8();
      ft8Init = false;

      tft.fillRect(WATERFALL_L, YPIXELS - 20 * 6, WATERFALL_W, 20 * 6 + 3, RA8875_BLACK);
      wfRows = WATERFALL_H;
    }
  }
#else
  // skip FT8 modes
  if(bands[currentBand].mode == DEMOD_FT8_WAV) {
    bands[currentBand].mode += 2;
  }
#endif

  SetupMode();
  //UpdateBWFilters();

  ShowOperatingStats();
  ShowBandwidthBarValues();
  DrawBandwidthBar();
}

/*****
  Purpose: Toggle operating mode (SSB or CW)

  Parameter list:
    void

  Return value:
    void
*****/
void ButtonMode() {
  // Toggle the current mode
  if(bands[currentBand].mode == DEMOD_FT8 || bands[currentBand].mode == DEMOD_FT8_WAV) {
    if(bands[currentBand].mode == DEMOD_FT8) {
      if(setupFT8Wav()) {
        // switch to play a wav file
        bands[currentBand].mode = DEMOD_FT8_WAV;
        ShowOperatingStats();
      }
    }
  } else {
    if (xmtMode == CW_MODE) {  
      xmtMode = SSB_MODE;

      // return waterfall to normal if decoder is on
      if(decoderFlag == ON) {
        // erase any decoded CW
        tft.fillRect(WATERFALL_L, YPIXELS - 35, WATERFALL_W, CHAR_HEIGHT + 3, RA8875_BLACK);  // Erase waterfall in decode area
        wfRows = WATERFALL_H;
      }
    } else {
      xmtMode = CW_MODE;

      // reduce waterfall height if we're decoding CW
      if(decoderFlag == ON) {
        tft.fillRect(WATERFALL_L, YPIXELS - 35, WATERFALL_W, CHAR_HEIGHT + 3, RA8875_BLACK);  // Erase waterfall in decode area
        tft.writeTo(L2); // it's on layer 2 as well
        tft.fillRect(WATERFALL_L, YPIXELS - 35, WATERFALL_W, CHAR_HEIGHT + 3, RA8875_BLACK);  // Erase waterfall in decode area
        tft.writeTo(L1);
        wfRows = WATERFALL_H - CHAR_HEIGHT - 3;
      }
    }

    UpdateCWFilter();
    ShowOperatingStats();
  }
}

/*****
  Purpose: To process select noise reduction

  Parameter list:
    void

  Return value:
    void
*****/
void ButtonNR() {
  nrOptionSelect++;
  if (nrOptionSelect > NR_OPTIONS) {
    nrOptionSelect = 0;
  }

  UpdateInfoBoxItem(&infoBox[IB_ITEM_FILTER]);
}

/*****
  Purpose: To set the notch filter

  Parameter list:
    void

  Return value:
    void
*****/
void ButtonNotchFilter() {
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
void ToggleLiveNoiseFloorFlag() {
  // save final noise floor setting if toggling flag off
  if(liveNoiseFloorFlag) {
    EEPROMData.currentNoiseFloor[currentBand]  = currentNoiseFloor[currentBand];
    EEPROMWrite();
  }

  liveNoiseFloorFlag = !liveNoiseFloorFlag;
  UpdateInfoBoxItem(&infoBox[IB_ITEM_FLOOR]);
}

/*****
  Purpose: Direct Frequency Entry

  Parameter list:
    void

  Return value;
    void
    Base Code courtesy of Harry  GM3RVL
*****/
void ButtonFrequencyEntry() {
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
