#include "SDT.h"
#include "Button.h"
#include "ButtonProc.h"
#include "CWProcessing.h"
#include "Display.h"
//#include "EEPROM.h"
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
  Purpose: Encoder volume control ISR

  Parameter list:
    void

  Return value;
    void
*****/
// why not FASTRUN
void EncoderVolumeISR() {
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
  Purpose: Fine tune control ISR

  Parameter list:
    void

  Return value;
    void
*****/
FASTRUN void EncoderFineTuneISR() {
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
  Purpose: Menu/Change/Filter encoder movement ISR

  Parameter list:
    void

  Return value;
    void
*****/
FASTRUN void EncoderMenuChangeFilterISR() {
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

  if(calibrateFlag >= 0) return; // we're calibrating

  // interpret encoder according to flag settings
  if(getEncoderValueFlag) {
    return; // menuEncoderMove processed in GetEncoderValueLive and GetMenuValueLoop routines
  }

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
        // we're adjusting audio spectrum filter
        posFilterEncoder = lastFilterEncoder - 5 * menuEncoderMove;
      }
    }
  }
}
