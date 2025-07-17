
#include "SDT.h"

#include <Rotary.h>                    // https://github.com/brianlow/Rotary

#include "Button.h"
#include "ButtonProc.h"
#include "CW_Excite.h"
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
bool volumeChangeFlag, resetTuningFlag, fineTuneFlag, getEncoderValueFlag;
long filter_pos_BW, last_filter_pos_BW;
int posFilterEncoder, lastFilterEncoder;

volatile int menuEncoderMove;
volatile long fineTuneEncoderMove;

//------------------------- Local Variables ----------

Rotary fineTuneEncoder = Rotary(FINETUNE_ENCODER_A, FINETUNE_ENCODER_B);  // ( 4,  5)
Rotary menuChangeEncoder = Rotary(FILTER_ENCODER_A, FILTER_ENCODER_B);    // (15, 14)
Rotary tuneEncoder = Rotary(TUNE_ENCODER_A, TUNE_ENCODER_B);              // (16, 17)
Rotary volumeEncoder = Rotary(VOLUME_ENCODER_A, VOLUME_ENCODER_B);        // ( 2,  3)

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

// set up encoders
void EncodersInit() {
  pinMode(FILTER_ENCODER_A, INPUT);
  pinMode(FILTER_ENCODER_B, INPUT);

  fineTuneEncoder.begin(true);
  menuChangeEncoder.begin(true);
  tuneEncoder.begin(true);
  volumeEncoder.begin(true);

  attachInterrupt(digitalPinToInterrupt(VOLUME_ENCODER_A), EncoderVolumeISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(VOLUME_ENCODER_B), EncoderVolumeISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(FILTER_ENCODER_A), EncoderMenuChangeFilterISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(FILTER_ENCODER_B), EncoderMenuChangeFilterISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(FINETUNE_ENCODER_A), EncoderFineTuneISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(FINETUNE_ENCODER_B), EncoderFineTuneISR, CHANGE);
}

void AdjustFilterBW(int filterChange) {
  if(lowerAudioFilterActive) { // false - high, true - low filter
    currentFilterLoCut = currentFilterLoCut - filterChange * 50 * ENCODER_FACTOR;

    // restrain filter
    if(currentFilterLoCut < 0.0) currentFilterLoCut = 0.0;
    if(currentFilterLoCut > currentFilterHiCut) currentFilterLoCut = currentFilterHiCut;
  } else {
    currentFilterHiCut = currentFilterHiCut - filterChange * 50 * ENCODER_FACTOR;

    // restrain filter
    if(currentFilterHiCut < currentFilterLoCut) currentFilterHiCut = currentFilterLoCut;
  }
}

/*****
  Purpose: Set bandwidth filters based on accumulated filter encoder changes, update BW values on display

  Parameter list:
    int FW - filter width
*****/
void SetBWFilters() {
  int filterChange = posFilterEncoder - lastFilterEncoder;

  lastFilterEncoder = posFilterEncoder;

  switch(bands[currentBand].demod) {
    case DEMOD_USB:
    case DEMOD_LSB:
    case DEMOD_PSK31_WAV:
    case DEMOD_PSK31:
    case DEMOD_FT8:
    case DEMOD_FT8_WAV:
      AdjustFilterBW(filterChange);
      break;

    case DEMOD_AM:
    case DEMOD_SAM:
      currentFilterHiCut = currentFilterHiCut - filterChange * 50 * ENCODER_FACTOR;
      currentFilterLoCut = -currentFilterHiCut;
      break;

    case DEMOD_NFM:
      if(nfmBWFilterActive) {
        filterChange = filter_pos_BW - last_filter_pos_BW;
        last_filter_pos_BW = filter_pos_BW;
        nfmFilterBW = (nfmFilterBW / 2.0 - filterChange * 50 * ENCODER_FACTOR) * 2;
      } else {
        AdjustFilterBW(filterChange);
      }
      break;
  }

  CalcFilters();
}

/*****
  Purpose: Set center tune frequency based on
*****/
void EncoderCenterTune() {
  long tuneChange = 0L;

  unsigned char result = tuneEncoder.process();  // Read the encoder

  if(result == 0)  // Nothing read
    return;

  if(radioMode == CW_MODE && decoderFlag == ON) {  // No reason to reset if we're not doing decoded CW
    ResetHistograms();
  }

  switch(result) {
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
  Return value:
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
  if(abs(startValue) > 2) {
    tft.print(startValue, 0);
  } else {
    tft.print(startValue, 3);
  }

  if(menuEncoderMove != 0) {
    currentValue += menuEncoderMove * increment;  // Bump up or down...
    if(currentValue < minValue)
      currentValue = minValue;
    else if(currentValue > maxValue)
      currentValue = maxValue;

    tft.setCursor(440, 1);
    if(abs(startValue) > 2) {
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
*****/
// why not FASTRUN
void EncoderVolumeISR() {
  char result;
  static float adjustVolEncoder = 1;


  result = volumeEncoder.process();  // Read the encoder

  if(result == 0) {  // Nothing read
    return;
  }
  switch(result) {
    case DIR_CW:  // Turned it clockwise, 16
      adjustVolEncoder = 1;
      break;

    case DIR_CCW:  // Turned it counter-clockwise
      adjustVolEncoder = -1;
      break;
  }
  audioVolume += adjustVolEncoder;

  if(audioVolume > MAX_AUDIO_VOLUME) {
    audioVolume = MAX_AUDIO_VOLUME;
  } else if(audioVolume < MIN_AUDIO_VOLUME) {
    audioVolume = MIN_AUDIO_VOLUME;
  }

  volumeChangeFlag = true; // flag needed for display update
}

/*****
  Purpose: Fine tune control ISR
*****/
FASTRUN void EncoderFineTuneISR() {
  char result;

  result = fineTuneEncoder.process();  // Read the encoder
  if(result == 0) {                   // Nothing read
    fineTuneEncoderMove = 0L;
    return;
  } else {
    if(result == DIR_CW) {  // 16 = CW, 32 = CCW
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
*****/
FASTRUN void EncoderMenuChangeFilterISR() {
  char result;

  result = menuChangeEncoder.process();  // Read the encoder

  if(result == 0) {
    return;
  }

  switch(result) {
    case DIR_CW:  // Turned it clockwise, 16
      menuEncoderMove = 1;
      break;

    case DIR_CCW:  // Turned it counter-clockwise
      menuEncoderMove = -1;
      break;
  }

  if(calibrateFlag >= 0) return; // we're calibrating

  // interpret encoder according to flag settings
  if(getEncoderValueFlag || (displayState == DISPLAY_FULL_MENU)) {
    return; // menuEncoderMove processed in GetEncoderValueLive and GetMenuValueLoop routines
  }

  if(liveNoiseFloorFlag == 2) {
    // we're setting noise floor
    currentNoiseFloor[currentBand] += menuEncoderMove;
  } else {
    if(ft8MsgSelectActive) {
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
      if(bands[currentBand].demod == DEMOD_NFM && nfmBWFilterActive) {
        // we're adjusting NFM demod bandwidth
        filter_pos_BW = last_filter_pos_BW - 5 * menuEncoderMove;
      } else {
        // we're adjusting audio spectrum filter
        posFilterEncoder = lastFilterEncoder - 5 * menuEncoderMove;
      }
    }
  }
}
