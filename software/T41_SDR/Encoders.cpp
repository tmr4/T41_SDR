#include "SDT.h"
#include "Button.h"
#include "ButtonProc.h"
#include "CWProcessing.h"
#include "Display.h"
#include "EEPROM.h"
#include "Encoders.h"
#include "Filter.h"
#include "ft8.h"
#include "InfoBox.h"
#include "Menu.h"
#include "MenuProc.h"
#include "Tune.h"
#include "Utility.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

#define MAX_WPM                  60

#define MENU_F_LO_CUT            40

//------------------------- Global Variables ----------
bool volumeChangeFlag = false;
bool resetTuningFlag = false;
bool fineTuneFlag = false;

long posFilterEncoder = 0;
long lastFilterEncoder = 0;

long filter_pos_BW = 0;
long last_filter_pos_BW = 0;

//------------------------- Local Variables ----------

bool getEncoderValueFlag = false;

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

/*****
  Purpose: Set bandwidth filters based on accumulated filter encoder changes, update BW values on display

  Parameter list:
    int FW - filter width

  Return value;
    void
*****/
void SetBWFilters() {
  int filter_change = posFilterEncoder - lastFilterEncoder;

  lastFilterEncoder = posFilterEncoder;

  switch (bands[currentBand].mode) {
    case DEMOD_USB:
    case DEMOD_PSK31_WAV:
    case DEMOD_PSK31:
    case DEMOD_FT8:
    case DEMOD_FT8_WAV:
      if (lowerAudioFilterActive) { // false - high, true - low filter
        bands[currentBand].FLoCut = bands[currentBand].FLoCut - filter_change * 50 * ENCODER_FACTOR;
      } else {
        bands[currentBand].FHiCut = bands[currentBand].FHiCut - filter_change * 50 * ENCODER_FACTOR;
      }
      break;

    case DEMOD_LSB:
      if (lowerAudioFilterActive) {
        bands[currentBand].FHiCut = bands[currentBand].FHiCut + filter_change * 50 * ENCODER_FACTOR;
      } else {
        bands[currentBand].FLoCut = bands[currentBand].FLoCut + filter_change * 50 * ENCODER_FACTOR;
      } 
      break;

    case DEMOD_AM:
    case DEMOD_SAM:
      bands[currentBand].FHiCut = bands[currentBand].FHiCut - filter_change * 50 * ENCODER_FACTOR;
      bands[currentBand].FLoCut = -bands[currentBand].FHiCut;
      break;

    case DEMOD_NFM:
      if (nfmBWFilterActive) {
        filter_change = filter_pos_BW - last_filter_pos_BW;
        last_filter_pos_BW = filter_pos_BW;
        nfmFilterBW = (nfmFilterBW / 2.0 - filter_change * 50 * ENCODER_FACTOR) * 2;
      } else {
        //bands[currentBand].FHiCut = bands[currentBand].FHiCut - filter_change * 50 * ENCODER_FACTOR;
        //bands[currentBand].FLoCut = -bands[currentBand].FHiCut;
        if (lowerAudioFilterActive) { // false - high, true - low filter
          bands[currentBand].FLoCut = bands[currentBand].FLoCut - filter_change * 50 * ENCODER_FACTOR;
        } else {
          bands[currentBand].FHiCut = bands[currentBand].FHiCut - filter_change * 50 * ENCODER_FACTOR;
        }
      }
      break;
  }

  CalcFilters();
}

/*****
  Purpose: Set center tune frequency based on 

  Parameter list:
    void
    
  Return value;
    void
*****/
void EncoderCenterTune() {
  long tuneChange = 0L;

  unsigned char result = tuneEncoder.process();  // Read the encoder

  if (result == 0)  // Nothing read
    return;

  if (xmtMode == CW_MODE && decoderFlag == ON) {  // No reason to reset if we're not doing decoded CW
    ResetHistograms();
  }

  switch (result) {
    case DIR_CW:  // Turned it clockwise, 16
      tuneChange = 1L;
      break;

    case DIR_CCW:  // Turned it counter-clockwise
      tuneChange = -1L;
      break;
  }

  SetCenterTune((long)freqIncrement * tuneChange);
}

/*****
  Purpose: Encoder volume control

  Parameter list:
    void

  Return value;
    void
*****/
// why not FASTRUN
void EncoderVolume() {
  char result;
  int increment [[maybe_unused]] = 0;
  static float adjustVolEncoder = 1;


  result = volumeEncoder.process();  // Read the encoder

  if (result == 0) {  // Nothing read
    return;
  }
  switch (result) {
    case DIR_CW:  // Turned it clockwise, 16
      adjustVolEncoder = 1;
      break;

    case DIR_CCW:  // Turned it counter-clockwise
      adjustVolEncoder = -1;
      break;
  }
  audioVolume += adjustVolEncoder;
  // simulate log taper.  As we go higher in volume, the increment increases.

  if (audioVolume < (MIN_AUDIO_VOLUME + 10)) increment = 2;
  else if (audioVolume < (MIN_AUDIO_VOLUME + 20)) increment = 3;
  else if (audioVolume < (MIN_AUDIO_VOLUME + 30)) increment = 4;
  else if (audioVolume < (MIN_AUDIO_VOLUME + 40)) increment = 5;
  else if (audioVolume < (MIN_AUDIO_VOLUME + 50)) increment = 6;
  else if (audioVolume < (MIN_AUDIO_VOLUME + 60)) increment = 7;
  else increment = 8;


  if (audioVolume > MAX_AUDIO_VOLUME) {
    audioVolume = MAX_AUDIO_VOLUME;
  } else {
    if (audioVolume < MIN_AUDIO_VOLUME)
      audioVolume = MIN_AUDIO_VOLUME;
  }

  volumeChangeFlag = true;  // Need this because of unknown timing in display updating.
}

/*****
  Purpose: Use the encoder to change the value of a number in some other function

  Parameter list:
    int minValue                the lowest value allowed
    int maxValue                the largest value allowed
    int startValue              the numeric value to begin the count
    int increment               the amount by which each increment changes the value
    char prompt[]               the input prompt
  Return value;
    int                         the new value
*****/
float GetEncoderValueLive(float minValue, float maxValue, float startValue, float increment, char prompt[]) {
  float currentValue = startValue;

  getEncoderValueFlag = true;

  tft.setFontScale((enum RA8875tsize)1);
  tft.setTextColor(RA8875_WHITE);
  tft.fillRect(250, 0, 285, CHAR_HEIGHT, RA8875_BLACK);  // Increased rectangle size to full erase value
  tft.setCursor(257, 1);
  tft.print(prompt);
  tft.setCursor(440, 1);
  if (abs(startValue) > 2) {
    tft.print(startValue, 0);
  } else {
    tft.print(startValue, 3);
  }
  //while (true) {
  if (menuEncoderMove != 0) {
    currentValue += menuEncoderMove * increment;  // Bump up or down...
    if (currentValue < minValue)
      currentValue = minValue;
    else if (currentValue > maxValue)
      currentValue = maxValue;

    tft.setCursor(440, 1);
    if (abs(startValue) > 2) {
      tft.print(startValue, 0);
    } else {
      tft.print(startValue, 3);
    }
    menuEncoderMove = 0;
  }

  getEncoderValueFlag = false;
  return currentValue;
}

/*****
  Purpose: Use the encoder to change the value of a number in some other function

  Parameter list:
    int minValue                the lowest value allowed
    int maxValue                the largest value allowed
    int startValue              the numeric value to begin the count
    int increment               the amount by which each increment changes the value
    char prompt[]               the input prompt
  Return value;
    int                         the new value
*****/
int GetEncoderValue(int minValue, int maxValue, int startValue, int increment, char prompt[]) {
  int currentValue = startValue;
  int val;

  getEncoderValueFlag = true;

  tft.setFontScale((enum RA8875tsize)1);
  tft.setTextColor(RA8875_WHITE);
  //tft.fillRect(250, 0, 280, CHAR_HEIGHT, RA8875_MAGENTA);
  tft.fillRect(SECONDARY_MENU_X, 0, EACH_MENU_WIDTH, CHAR_HEIGHT, RA8875_MAGENTA);
  tft.setCursor(257, 1);
  tft.print(prompt);
  tft.setCursor(470, 1);
  tft.print(startValue);

  while (true) {
    if (menuEncoderMove != 0) {
      currentValue += menuEncoderMove * increment;  // Bump up or down...
      if (currentValue < minValue)
        currentValue = minValue;
      else if (currentValue > maxValue)
        currentValue = maxValue;

      tft.fillRect(465, 0, 55, CHAR_HEIGHT, RA8875_MAGENTA);
      tft.setCursor(470, 1);
      tft.print(currentValue);
      menuEncoderMove = 0;
    }

    val = ReadSelectedPushButton();  // Read the ladder value
    //delay(100L);
    if (val != -1 && val < (EEPROMData.switchValues[0] + WIGGLE_ROOM)) {
      val = ProcessButtonPress(val);    // Use ladder value to get menu choice
      if (val == MENU_OPTION_SELECT) {  // Make a choice??
        getEncoderValueFlag = false;
        return currentValue;
      }
    }
  }
}

/*****
  Purpose: Fine tune control

  Parameter list:
    void

  Return value;
    void
*****/
FASTRUN void EncoderFineTune() {
  char result;

  result = fineTuneEncoder.process();  // Read the encoder
  if (result == 0) {                   // Nothing read
    fineTuneEncoderMove = 0L;
    return;
  } else {
    if (result == DIR_CW) {  // 16 = CW, 32 = CCW
      fineTuneEncoderMove = 1L;
    } else {
      fineTuneEncoderMove = -1L;
    }
  }

  SetFineTune(ftIncrement * fineTuneEncoderMove);

  fineTuneEncoderMove = 0L;
}

/*****
  Purpose: Process Menu/Change/Filter encoder movement

  Parameter list:
    void

  Return value;
    void
*****/
FASTRUN void EncoderMenuChangeFilter() {
  char result;

  result = menuChangeEncoder.process();  // Read the encoder

  if (result == 0) {
    return;
  }

  switch (result) {
    case DIR_CW:  // Turned it clockwise, 16
      menuEncoderMove = 1;
      break;

    case DIR_CCW:  // Turned it counter-clockwise
      menuEncoderMove = -1;
      break;
  }

  if(calibrateFlag != 0) return; // we're calibrating

  // interpret encoder according to flag settings
  if(liveNoiseFloorFlag) {
    // we're setting noise floor
    currentNoiseFloor[currentBand] += menuEncoderMove;
  } else {
    if (ft8MsgSelectActive) {
      if(num_decoded_msg > 0) {
        activeMsg += menuEncoderMove;
        if(activeMsg >= num_decoded_msg) {
          activeMsg = 0;
        } else {
          if(activeMsg < 0) {
            activeMsg = num_decoded_msg - 1;
          }
        }
      }
      menuEncoderMove = 0;
    } else {
      if (bands[currentBand].mode == DEMOD_NFM && nfmBWFilterActive) {
        // we're adjusting NFM demod bandwidth
        filter_pos_BW = last_filter_pos_BW - 5 * menuEncoderMove;
      } else {
        if(getEncoderValueFlag) {
          return; // menuEncoderMove processed in GetEncoderValue(Live) routines
        } else {
          // we're adjusting audio spectrum filter
          posFilterEncoder = lastFilterEncoder - 5 * menuEncoderMove;
        }
      }
    }
  }
}

/*****
  Purpose: Allows quick setting of WPM without going through a menu

  Parameter list:
    void

  Return value;
    int           the current WPM
*****/
int SetWPM() {
  int val;
  long lastWPM = currentWPM;

  tft.setFontScale((enum RA8875tsize)1);

  tft.fillRect(SECONDARY_MENU_X, MENUS_Y, EACH_MENU_WIDTH, CHAR_HEIGHT, RA8875_MAGENTA);
  tft.setTextColor(RA8875_WHITE);
  tft.setCursor(SECONDARY_MENU_X + 1, MENUS_Y);
  tft.print("current WPM:");
  tft.setCursor(SECONDARY_MENU_X + 200, MENUS_Y);
  tft.print(currentWPM);

  while (true) {
    if (menuEncoderMove != 0) {       // Changed encoder?
      currentWPM += menuEncoderMove;  // Yep
      lastWPM = currentWPM;
      if (lastWPM < 5)  // Set minimum keyer speed to 5 wpm.  KF5N August 20, 2023
        lastWPM = 5;
      else if (lastWPM > MAX_WPM)
        lastWPM = MAX_WPM;

      tft.fillRect(SECONDARY_MENU_X + 200, MENUS_Y, 50, CHAR_HEIGHT, RA8875_MAGENTA);
      tft.setCursor(SECONDARY_MENU_X + 200, MENUS_Y);
      tft.print(lastWPM);
      menuEncoderMove = 0;
    }

    val = ReadSelectedPushButton();  // Read pin that controls all switches
    val = ProcessButtonPress(val);
    if (val == MENU_OPTION_SELECT) {  // Make a choice??
      currentWPM = lastWPM;
      EEPROMData.currentWPM = currentWPM;
      EEPROMWrite();
      UpdateInfoBoxItem(IB_ITEM_KEY);
      break;
    }
  }

  tft.setTextColor(RA8875_WHITE);
  return currentWPM;
}

/*****
  Purpose: Determines how long the transmit relay remains on after last CW atom is sent.

  Parameter list:
    void

  Return value;
    long            the delay length in milliseconds
*****/
long SetTransmitDelay() {
  int val;
  long lastDelay = cwTransmitDelay;
  long increment = 250;  // Means a quarter second change per detent

  tft.setFontScale((enum RA8875tsize)1);

  tft.fillRect(SECONDARY_MENU_X - 150, MENUS_Y, EACH_MENU_WIDTH + 150, CHAR_HEIGHT, RA8875_MAGENTA);  // scoot left cuz prompt is long
  tft.setTextColor(RA8875_WHITE);
  tft.setCursor(SECONDARY_MENU_X - 149, MENUS_Y);
  tft.print("current delay:");
  tft.setCursor(SECONDARY_MENU_X + 79, MENUS_Y);
  tft.print(cwTransmitDelay);

  while (true) {
    if (menuEncoderMove != 0) {                  // Changed encoder?
      lastDelay += menuEncoderMove * increment;  // Yep
      if (lastDelay < 0L)
        lastDelay = 250L;

      tft.fillRect(SECONDARY_MENU_X + 80, MENUS_Y, 200, CHAR_HEIGHT, RA8875_MAGENTA);
      tft.setCursor(SECONDARY_MENU_X + 79, MENUS_Y);
      tft.print(lastDelay);
      menuEncoderMove = 0;
    }

    val = ReadSelectedPushButton();  // Read pin that controls all switches
    val = ProcessButtonPress(val);
    //delay(150L);  //ALF 09-22-22
    if (val == MENU_OPTION_SELECT) {  // Make a choice??
      cwTransmitDelay = lastDelay;
      EEPROMData.cwTransmitDelay = cwTransmitDelay;
      EEPROMWrite();
      break;
    }
  }
  tft.setTextColor(RA8875_WHITE);
  EraseMenus();
  return cwTransmitDelay;
}
