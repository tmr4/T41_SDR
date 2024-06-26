#include "SDT.h"
#include "Button.h"
#include "CWProcessing.h"
#include "CW_Excite.h"
#include "Display.h"
#include "DSP_Fn.h"
#include "EEPROM.h"
#include "Encoders.h"
#include "FFT.h"
#include "InfoBox.h"
#include "Menu.h"
#include "MenuProc.h"
#include "Utility.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

#define MAX_DECODE_CHARS        32                    // Max chars that can appear on decoder line.  Increased to 32.  KF5N October 29, 2023
#define DECODER_BUFFER_SIZE     128                   // Max chars in binary search string with , . ?
#define HISTOGRAM_ELEMENTS      750
#define LOWEST_ATOM_TIME         20                   // 60WPM has an atom of 20ms
#define ADAPTIVE_SCALE_FACTOR   0.8                   // The amount of old histogram values are presesrved
#define SCALE_CONSTANT          (1.0 / (1.0 - ADAPTIVE_SCALE_FACTOR)) // Insure array has enough observations to scale

unsigned long ditLength;
unsigned long transmitDitLength;
long signalElapsedTime;
long gapLength;    // Time for noise measures
float32_t combinedCoeff;
float CWLevelTimer = 0.0;
float CWLevelTimerOld = 0.0;

//=== CW Filter ===

float32_t CW_Filter_state[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
float32_t CW_AudioFilter1_state[12] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
float32_t CW_AudioFilter2_state[12] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
float32_t CW_AudioFilter3_state[12] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
float32_t CW_AudioFilter4_state[12] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
float32_t CW_AudioFilter5_state[12] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
//---------  Code Filter instance -----
arm_biquad_cascade_df2T_instance_f32 S1_CW_Filter = { IIR_CW_NUMSTAGES, CW_Filter_state, CW_Filter_Coeffs };
arm_biquad_cascade_df2T_instance_f32 S1_CW_AudioFilter1 = { 6, CW_AudioFilter1_state, CW_AudioFilterCoeffs1 };
arm_biquad_cascade_df2T_instance_f32 S1_CW_AudioFilter2 = { 6, CW_AudioFilter2_state, CW_AudioFilterCoeffs2 };
arm_biquad_cascade_df2T_instance_f32 S1_CW_AudioFilter3 = { 6, CW_AudioFilter3_state, CW_AudioFilterCoeffs3 };
arm_biquad_cascade_df2T_instance_f32 S1_CW_AudioFilter4 = { 6, CW_AudioFilter4_state, CW_AudioFilterCoeffs4 };
arm_biquad_cascade_df2T_instance_f32 S1_CW_AudioFilter5 = { 6, CW_AudioFilter5_state, CW_AudioFilterCoeffs5 };
//=== end CW Filter ===

float32_t aveCorrResult;
float32_t aveCorrResultR;
float32_t aveCorrResultL;

float goertzelMagnitude;

arm_fir_instance_f32 FIR_CW_DecodeL;
arm_fir_instance_f32 FIR_CW_DecodeR;

//------------------------- Local Variables ----------
// *** looks like many of these should be function variables ***

float32_t corrResult;
uint32_t corrResultIndex;
float32_t corrResultR;
uint32_t corrResultIndexR;
float32_t corrResultL;
uint32_t corrResultIndexL;
float32_t combinedCoeff2;
float32_t combinedCoeff2Old;
int CWCoeffLevelOld = 0.0;

int endGapFlag = 0;
int topGapIndex;
int topGapIndexOld;

//float32_t float_Corr_Buffer[511];
float32_t *float_Corr_Buffer;

//int32_t gapHistogram[HISTOGRAM_ELEMENTS];
//int32_t signalHistogram[HISTOGRAM_ELEMENTS];
int32_t *gapHistogram, *signalHistogram;

long valRef1;
long valRef2;
long gapRef1;
int valFlag = 0;
long signalStartOld = 0;
long aveDitLength = 80;
long aveDahLength = 200;
float thresholdGeometricMean = 140.0;  // This changes as decoder runs
float thresholdArithmeticMean;

char decodeBuffer[33];    // The buffer for holding the decoded characters
byte currentDashJump = DECODER_BUFFER_SIZE;
byte currentDecoderIndex = 0;
int dahLength;
int gapAtom;  // Space between atoms
int gapChar;  // Space between characters

float32_t DMAMEM float_buffer_CW[256];

//-------------------------------------------------------------------------------------------------------------
// Forwards
//-------------------------------------------------------------------------------------------------------------

void DoCWDecoding(int audioValue);
void DoGapHistogram(long gapLen);
void JackClusteredArrayMax(int32_t *array, int32_t elements, int32_t *maxCount, int32_t *maxIndex, int32_t *firstNonZero, int32_t spread);
void DoSignalHistogram(long val);
float goertzel_mag(int numSamples, int TARGET_FREQUENCY, int SAMPLING_RATE, float *data);

void Dah();
void Dit();

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------------------
// CW menu options
//-------------------------------------------------------------------------------------------------------------

/*****
  Purpose: wrap up SetWPM and save to EEPROM

  Parameter list:
    void

  Return value;
    void
*****/
FLASHMEM void SetWPMFollowup() {
  SetTransmitDitLength();
  EEPROMData.currentWPM = currentWPM;
  EEPROMWrite();
  UpdateInfoBoxItem(IB_ITEM_KEY);
}

FLASHMEM void SetKeyTypeFollowup() {
  // Make sure the paddleDit and paddleDah variables are set correctly for straight key.
  // Paddle flip can reverse these, making the straight key inoperative.
  if (keyType == 0) {
    paddleDit = KEYER_DIT_INPUT_TIP;
    paddleDah = KEYER_DAH_INPUT_RING;
  }
}
/*****
  Purpose: Select straight key or keyer

  Parameter list:
    void

  Return value:
    void
*****/
FLASHMEM void SetKeyType() {
  if(USE_FULL_MENU) {
    keyType = SetSecondaryMenuIndex();
    SetKeyTypeFollowup();
  } else {
    //GetMenuOption(optionIndex, *currentValue, *setup(), *getValue(), *followup());
    GetMenuOption(0, &keyType, NULL, NULL, &SetKeyTypeFollowup);
  }
}

/*****
  Purpose: Set up key at power-up.

  Parameter list:
    void

  Return value:
    void
*****/
FLASHMEM void SetKeyPowerUp() {
  if (keyType == 0) {
    paddleDit = KEYER_DIT_INPUT_TIP;
    paddleDah = KEYER_DAH_INPUT_RING;
    return;
  }
  if (paddleFlip) {  // Means right-paddle dit
    paddleDit = KEYER_DAH_INPUT_RING;
    paddleDah = KEYER_DIT_INPUT_TIP;
  } else {
    paddleDit = KEYER_DIT_INPUT_TIP;
    paddleDah = KEYER_DAH_INPUT_RING;
  }
}

FLASHMEM void SelectCWFilterFollowup() {
  // update CW filters if index is different
  if(CWFilterIndex != getMenuInc) {
    ShowOperatingStats();
    if (xmtMode == CW_MODE) {
      UpdateCWFilter();
    }
  }
}

/*****
  Purpose: Select CW Filter. CWFilterIndex has these values:
           0 = 840Hz
           1 = 1kHz
           2 = 1.3kHz
           3 = 1.8kHz
           4 = 2kHz
           5 = Off

  Parameter list:
    void

  Return value:
    void
*****/
FLASHMEM void SelectCWFilter() {
  // save CW filter index for later
  getMenuInc = CWFilterIndex;

  //GetMenuOption(optionIndex, *currentValue, *setup(), *getValue(), *followup());
  GetMenuOption(1, &CWFilterIndex, NULL, NULL, &SelectCWFilterFollowup);
}

FLASHMEM void DoPaddleFlipFollowup() {
  if(getMenuInc) {  // right-paddle dit
    paddleDit = KEYER_DAH_INPUT_RING;
    paddleDah = KEYER_DIT_INPUT_TIP;
    paddleFlip = 1;
  } else {
    paddleDit = KEYER_DIT_INPUT_TIP;
    paddleDah = KEYER_DAH_INPUT_RING;
    paddleFlip = 0;
  }

  EEPROMData.paddleDit = paddleDit;
  EEPROMData.paddleDah = paddleDah;
  UpdateInfoBoxItem(IB_ITEM_KEY);
}

/*****
  Purpose: This option reverses the dit and dah paddles on the keyer

  Parameter list:
    void

  Return value
    void
*****/
FLASHMEM void DoPaddleFlip() {
  getMenuInc = paddleDah == KEYER_DAH_INPUT_RING ? 0 : 1;

  //GetMenuOption(optionIndex, *currentValue, *setup(), *getValue(), *followup());
  GetMenuOption(2, &getMenuInc, NULL, NULL, &DoPaddleFlipFollowup);

}

/*****
  Purpose: Allow user to set the sidetone volume

  Parameter list:
    void

  Return value;
    void
*****/
FLASHMEM void SetSideToneVolumeSetup() {
  // I assume this is supposed to play a tone, varying the volume as we make the adjustment
  // *** TODO: this is non-functional w/o key before change in my version. Check if works with key down or if T41EEE works. Looks like he runs it through CW_Exciter ***
  //Q_in_L.clear();  // Clear other buffers too?
  //Q_in_R.clear();
  //
  //modeSelectInR.gain(0, 0);
  //modeSelectInL.gain(0, 0);
  //modeSelectInExR.gain(0, 0);
  //modeSelectOutL.gain(0, 0);
  //modeSelectOutR.gain(0, 0);
  //modeSelectOutExL.gain(0, 0);
  //modeSelectOutExR.gain(0, 0);
  //digitalWrite(MUTE, LOW);      // unmutes audio
  //modeSelectOutL.gain(1, 0.0);  // Sidetone
  //modeSelectOutR.gain(1, 0.0);  // Sidetone
}

void SetSideToneVolumeValue() {
  //if(digitalRead(paddleDit) == LOW || digitalRead(paddleDah) == LOW) {
  //  CW_ExciterIQData();
  //}
  //
  //modeSelectOutL.gain(1, volumeLog[sidetoneVolume]);
}

FLASHMEM void SetSideToneVolumeFollowup() {
  EEPROMData.sidetoneVolume = sidetoneVolume;
  EEPROMWrite();
  //lastState = -1;  // This is required due to the function deactivating the receiver.  This forces a pass through the receiver set-up code.  KF5N October 7, 2023
}

/*****
  Purpose: Determines how long the transmit relay remains on after last CW atom is sent.

  Parameter list:
    void

  Return value;
    void
*****/
FLASHMEM void SetTransmitDelayFollowup() {
  EEPROMData.cwTransmitDelay = cwTransmitDelay;
  EEPROMWrite();
}

/*****
  Purpose: to process CW specific signals

  Parameter list:
    void

  Return value:
    void

*****/
void DoCWReceiveProcessing() {
  float goertzelMagnitude1;
  float goertzelMagnitude2;
  int audioTemp;

  if (decoderFlag == ON) {

    // left channel first
    arm_fir_f32(&FIR_CW_DecodeL, float_buffer_L, float_buffer_CW, 256); // Park McClellan FIR filter const Group delay

    // ----------------------  Correlation calculation  -------------------------
    //Calculate correlation between calc sine and incoming signal
    arm_correlate_f32(float_buffer_CW, 256, sinBuffer, 256, float_Corr_Buffer);
    //get max value of correlation
    arm_max_f32(float_Corr_Buffer, 511, &corrResultL, &corrResultIndexL);
    //running average of corr coeff. L
    aveCorrResultL = .7 * corrResultL + .3 * aveCorrResultL;
    aveCorrResult = (corrResultR + corrResultL) / 2;

    // Calculate Goertzel Mahnitude of incomming signal
    goertzelMagnitude1 = goertzel_mag(256, 750, 24000, float_buffer_CW);

    // now right channel
    arm_fir_f32(&FIR_CW_DecodeR, float_buffer_R, float_buffer_CW, 256);

    arm_correlate_f32(float_buffer_CW, 256, sinBuffer, 256, float_Corr_Buffer);
    arm_max_f32(float_Corr_Buffer, 511, &corrResultR, &corrResultIndexR);
    //running average of corr coeff. R
    aveCorrResultR = .7 * corrResultR + .3 * aveCorrResultR;

    goertzelMagnitude2 = goertzel_mag(256, 750, 24000, float_buffer_CW);

    goertzelMagnitude = (goertzelMagnitude1 + goertzelMagnitude2) / 2;

    //Combine Correlation and Gowetzel Coefficients
    combinedCoeff = 10 * aveCorrResult * 100 * goertzelMagnitude;
    combinedCoeff2 = combinedCoeff;

    UpdateDecodeLockIndicator();

    combinedCoeff2Old = combinedCoeff2;
    tft.drawFastVLine(AUDIO_SPEC_BOX_L + 29, AUDIO_SPEC_BOX_T, AUDIO_SPEC_BOX_H, ORANGE);  //CW lower freq indicator
    tft.drawFastVLine(AUDIO_SPEC_BOX_L + 37, AUDIO_SPEC_BOX_T, AUDIO_SPEC_BOX_H, ORANGE);  //CW upper freq indicator
    if (combinedCoeff > 50) {                                                                  // if  have a reasonable corr coeff, >50, then we have a keeper. // AFP 10-26-22
      audioTemp = 1;
    } else {
      audioTemp = 0;
    }
    //==============  acquire data on CW  ================
    DoCWDecoding(audioTemp);
  }
}

/*****
  Purpose: to provide spacing between letters

  Parameter list:
  void

  Return value:
  void

  CAUTION: Assumes that a global named ditLength holds the value for dit spacing
*****/
void LetterSpace() {
  delay(3UL * ditLength);
}

/*****
  Purpose: to provide spacing between words

  Parameter list:
  void

  Return value:
  void

  CAUTION: Assumes that a global named ditLength holds the value for dit spacing
*****/
void WordSpace() {
  delay(7UL * ditLength);
}

/*****
  Purpose: to send a Morse code character

  Parameter list:
    char code       the code for the letter to send

  Return value:
    void
*****/
void SendCode(char code) {
  int i;

  for (i = 7; i >= 0; i--)  // Find the sentinel. Loop looks for first 1,
    if (code & (1 << i))    // which marks the start of the letter:   0b11000 = 'B'
      break;

  for (i--; i >= 0; i--) {  // Now look at rest of binary value: 0b1000 = B after reading sentinel
    if (code & (1 << i))    // If it's a 1, send a dah, otherwise...
      Dah();
    else
      Dit();  // ...send a dit
  }

  LetterSpace();
}

/*****
  Purpose: establish the dit length for code transmission. Crucial since
    all spacing is done using dit length

  Parameter list:
    int wpm

  Return value:
    void
*****/
FLASHMEM void SetDitLength(int wpm) {
  ditLength = 1200 / wpm;
}

/*****
  Purpose: establish the dit length for code transmission. Crucial since
    all spacing is done using dit length

  Parameter list:
    int wpm

  Return value:
    void
*****/
FLASHMEM void SetTransmitDitLength() {
  transmitDitLength = 1200 / currentWPM;
}

//==================================== Decoder =================
//DB2OO, 29-AUG-23: moved col declaration here
static int col = 0;  // Start at lower left

/*****
  Purpose: This function displays the decoded Morse code below waterfall. Arranged as:

  Parameter list:
    char currentLetter

  Return value
    void
*****/
void MorseCharacterDisplay(char currentLetter) {
  if (col < MAX_DECODE_CHARS) {  // Start scrolling??
    decodeBuffer[col] = currentLetter;
    col++;
    decodeBuffer[col] = '\0';  // Make is a string
  } else {
    //DB2OO, 25-AUG-23: use memmove instead of memcpy(), to avoid the warning
    memmove(decodeBuffer, &decodeBuffer[1], MAX_DECODE_CHARS - 1);  // Slide array down 1 character.
    decodeBuffer[col - 1] = currentLetter;                          // Add to end
    decodeBuffer[col] = '\0';                                       // Make is a string
  }
  tft.fillRect(CW_TEXT_START_X, CW_TEXT_START_Y, CW_MESSAGE_WIDTH, CW_MESSAGE_HEIGHT * 2, RA8875_BLACK);
  tft.setFontScale((enum RA8875tsize)1);

  tft.setTextColor(RA8875_WHITE);
  tft.setCursor(CW_TEXT_START_X, CW_TEXT_START_Y);
  tft.print(decodeBuffer);
}

/*****
  Purpose: This function uses the current WPM to set an estimate ditLength any time the tune
           endcoder is changed

  Parameter list:
    void

  Return value
    void
*****/
FLASHMEM void ResetHistograms() {
  gapAtom = 80;
  ditLength = 80;  // Start with 15wpm ditLength
  gapChar = 240;
  dahLength = 240;
  thresholdGeometricMean = 160;  // Use simple mean for starters so we don't have 0
  aveDitLength = ditLength;
  aveDahLength = dahLength;
  valRef1 = 0;
  valRef2 = 0;

  // Clear graph arrays
  memset(signalHistogram, 0, HISTOGRAM_ELEMENTS * sizeof(uint32_t));
  memset(gapHistogram, 0, HISTOGRAM_ELEMENTS * sizeof(uint32_t));
  currentWPM = 1200 / ditLength;
  UpdateInfoBoxItem(IB_ITEM_KEY);
}

// This function was re-factored into a state machine by KF5N October 29, 2023.
/*****
  Purpose: Called when in CW mode and decoder flag is set. Function assumes:

      dit           = 1
      dah           = dit * 3
      inter-atom    = dit
      inter-letter  = dit * 3
      inter-word    = dit * 7

      You can distinguish between dah and inter-letter by presence/absence of signal. Same for inter-atom.

  Parameter list:
    float audioValue        the strength of audio signal

  Return value;
    void
*****/
// charProcessFlag means a character is being decoded.  blankFlag indicates a blank has already been printed.
bool charProcessFlag, blankFlag;
int currentTime, interElementGap, noSignalTimeStamp;
char *bigMorseCodeTree = (char *)"-EISH5--4--V---3--UF--------?-2--ARL---------.--.WP------J---1--TNDB6--.--X/-----KC------Y------MGZ7----,Q------O-8------9--0----";
//                                012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678
//                                         10        20        30        40        50        60        70        80        90       100       110       120
// This enum is used by an experimental Morse decoder.
enum states {state0, state1, state2, state3, state4, state5, state6};

void DoCWDecoding(int audioValue) {
  static long oldTime = millis();
  static enum states decodeStates = state0;
  static long signalStart;
  static long signalEnd;  // Start-end of dit or dah

  switch (decodeStates) {
    
    case state0:                                                // State 0.  Detects start of signal and starts timer.
      // Detect signal and redirect to appropriate state.
      if (audioValue == 1) {
        signalStart = millis();                                                                              // Time stamp beginning of signal.
        decodeStates = state1;                                                                               // Go to "signalStart" state.
        gapLength = signalStart - signalEnd;                                                                 // Calculate the time gap between the start of this new signal and the end of the last one.
        if (gapLength > LOWEST_ATOM_TIME                                                                     // range
            && (uint32_t)gapLength < (uint32_t)(thresholdGeometricMean * 3)
            && signalStart - oldTime > 5000L) {                                                              // Only call histogram every 5 seconds
          DoGapHistogram(gapLength);                                                                         // Map the gap in the signal
          oldTime = signalStart;                                                                             // Reset old time
        }
        break;
      }
      noSignalTimeStamp = millis();
      interElementGap = noSignalTimeStamp - signalEnd;
      if ((interElementGap > ditLength * 1.95) && charProcessFlag) {  // use thresholdGeometricMean??? was ditLength. End of character!  65 * 2
        decodeStates = state5;                                        // Character ended, print it!
        break;
      }
      if (interElementGap > ditLength * 4.5 && not blankFlag && not charProcessFlag) {  // A big gap, print a blank, but don't repeat a blank.  85 * 3.5
        decodeStates = state6;
        break;
      }
      decodeStates = state0;  // Stay in state0; no signal.
      break;                                                                                                 // End state0

    case state1:                                                // Times a signal and measures its duration.  The next state determines if the signal is a dit or a dah.
      if (audioValue == 0) {
        currentTime       = millis();
        signalElapsedTime = currentTime - signalStart;          // Calculate the duration of the signal.
//                                                                 Ignore short noisy signal bursts:
        if (signalElapsedTime < LOWEST_ATOM_TIME) {             // A hiccup or a real signal?  Make this a fraction of ditLength instead???
          decodeStates    = state0;                             // False signal, start over.
          break;
        }
        if (signalElapsedTime > LOWEST_ATOM_TIME                // Valid elapsed time?
            && signalElapsedTime < HISTOGRAM_ELEMENTS
            && currentTime - oldTime > 5000L) {                 // Only call histogram every 5 seconds
          DoSignalHistogram(signalElapsedTime);                 // Yep
          oldTime = currentTime;                                // Reset old time
        }
        signalEnd         = currentTime;                        // Time gap to next signal.
        decodeStates      = state2;                             // Proceed to state2.  A timed signal is available and must be processed.
        break;
      }
      decodeStates = state1;  // Signal still present, stay in state1.
      break;                  // End state1

    case state2:                                                // Determine if a timed signal was a dit or a dah and increment the decode tree.
      if (signalElapsedTime > (0.5 * ditLength)) {              // Use the geometric mean instead of ditLength???
        currentDashJump = currentDashJump >> 1;                 // Fast divide by 2
        if (signalElapsedTime < (int)thresholdGeometricMean) {  // It was a dit
          charProcessFlag = true;                               
          currentDecoderIndex++;
        } else {  // It's a dah!
          charProcessFlag = true;
          currentDecoderIndex += currentDashJump;
        }
      }

      decodeStates = state0;  // Begin process again.
      break;                  // End state2

    case state5:                                                // Display the character
      MorseCharacterDisplay(bigMorseCodeTree[currentDecoderIndex]);  // This always prints.  How do blanks get printed.
      currentDecoderIndex = 0;                                       //Reset everything if char or word
      currentDashJump     = DECODER_BUFFER_SIZE;
      charProcessFlag     = false;  // Char printed and no longer in progress.
      decodeStates        = state0;    // Start process for next incoming character.
      blankFlag           = false;
      break;                                                    // End state5

    case state6:                                                //  Blank printing state.
      MorseCharacterDisplay(' ');
      UpdateIBWPM();
      //tft.setTextColor(RA8875_WHITE);
      //tft.setFontScale((enum RA8875tsize)3);
      blankFlag = true;
      decodeStates = state0;  // Start process for next incoming character.
      break;

    default:
      break;
  }
}

/*****
  Purpose: This function creates a distribution of the gaps between signals, expressed
           in milliseconds. The result is a tri-modal distribution around three timings:
            1. inter-atom time (one dit length)
            2. inter-character (three dit lengths)
            3. word end (seven dit lengths)

  Parameter list:
    long val        the duration of the signal gap (ms)

  Return value;
    void

*****/
void DoGapHistogram(long gapLen) {
  int32_t tempAtom, tempChar;
  int32_t atomIndex, charIndex, firstDit, temp;
  uint32_t offset;

  if (gapHistogram[gapLen] > 10) {  // Need over 1 so we don't have fractional value
    for (int k = 0; k < HISTOGRAM_ELEMENTS; k++) {
      gapHistogram[k] = (uint32_t)(.8 * (float)gapHistogram[k]);
    }
  }

  gapHistogram[gapLen]++;  // Add new signal to distribution

  atomIndex = charIndex = 0;
  if (gapLen <= thresholdGeometricMean) {                                                                                 // Find new dit length
    JackClusteredArrayMax(gapHistogram, (uint32_t)thresholdGeometricMean, &tempAtom, &atomIndex, &firstDit, (int32_t)1);  // Find max dit gap
    if (atomIndex) {                                                                                                      // if something found
      gapAtom = atomIndex;
    }
    for (int j = 0; j < HISTOGRAM_ELEMENTS; j++) {                        // count down
      if (gapHistogram[HISTOGRAM_ELEMENTS - j] > 0 && endGapFlag == 0) {  //Look for non-zero entries in the histogram
        if (HISTOGRAM_ELEMENTS - j < gapAtom * 2) {                       // limit search to probable gapAtom entries
          topGapIndex = HISTOGRAM_ELEMENTS - j;                           //Upper end of gapAtom range
          endGapFlag = 1;                                                 // set flag so we know tha this is the top of the gapAtom range
        }
      }
      if (topGapIndex > 2 * gapAtom) topGapIndex = topGapIndexOld;  // discard outliers
    }
    endGapFlag = 0;                //reset flag
    topGapIndexOld = topGapIndex;  //Keep good value for reference
  } else {                         // dah calculation
    if (gapLen <= thresholdGeometricMean * 2) {
      offset = (uint32_t)(thresholdGeometricMean * 2);  // Find number of elements to check
      JackClusteredArrayMax(&gapHistogram[(int32_t)thresholdGeometricMean + 1], offset, &tempChar, &charIndex, &temp, (int32_t)3);
      if (charIndex)  // if something found
        gapChar = charIndex;
    }
  }
  if (atomIndex) {
    gapAtom = atomIndex;
  }
  if (charIndex) {
    gapChar = charIndex;
  }
}

/*****
  Purpose: This function replaces the arm_max_float32() function that finds the maximum element in an array.
           The histograms are "fuzzy" in the sense that dits and dahs "cluster" around a maximum value rather
           than having a single max value. This algorithm looks at a given cell and the adds in the previous
           (index - 1) and next (index + 1) cells to get the total for that index.

  Parameter list:
    int32_t *array         the base address of the array to search
    int32_t elements       the number of elements of the array to examine
    int32_t *maxCount      the largest clustered value found
    int32_t *maxIndex      the index of the center of the cluster
    int32_t *firstNonZero  the first cell that has a non-zero value
    int32_t clusterSpread  tells how far previous and ahead elements are to be included in the measure.
                            Must be an odd integer > 1.

  Return value;
    void
*****/
void JackClusteredArrayMax(int32_t *array, int32_t elements, int32_t *maxCount, int32_t *maxIndex, int32_t *firstNonZero, int32_t spread) {
  int32_t i, j, clusteredIndex;
  int32_t clusteredMax, temp;

  *maxCount = '\0';  // Reset to empty
  *maxIndex = '\0';

  clusteredMax = 0;
  clusteredIndex = -1;  // Now we can check for an error

  for (i = spread; i < elements - spread; i++) {  // Start with 1 so we can look at the previous element's value
    temp = 0;
    for (j = i - spread; j <= i + spread; j++) {
      temp += array[j];
      ;  // Include adjacent elements
    }

    if (temp >= clusteredMax) {
      clusteredMax = temp;
      clusteredIndex = i;
    }
  }
  if (clusteredIndex > 0) {
    *maxCount = array[clusteredIndex];
    *maxIndex = clusteredIndex;
  }
}
/*****
  Purpose: This function creates a distribution of the dit and dahs lengths, expressed in
  milliseconds. The result is a bi-modal distribution around those two timings. The
  modal value is then used for the timing of the decoder. The range should be between 20
  (60wpm) and 240 (5wpm)

  Parameter list:
  long val        the strength of audio signal

  Return value;
  void

*****/
void DoSignalHistogram(long val) {
  float compareFactor = 2.0;
  int32_t firstNonEmpty;
  int32_t tempDit, tempDah;
  int32_t offset;

  if (valFlag == 0) {
    valRef1 = signalElapsedTime;
    signalStartOld = millis();
    valFlag = 1;
  }

  if (millis() - signalStartOld > LOWEST_ATOM_TIME && valFlag == 1) {
    gapRef1 = gapLength;
    valRef2 = signalElapsedTime;
    valFlag = 0;
  }

  if ((valRef2 >= valRef1 * compareFactor && gapRef1 <= valRef1 * compareFactor)
      || (valRef1 >= valRef2 * compareFactor && gapRef1 <= valRef2 * compareFactor)) {
    // See if consecutive signal lengths in approximate dit to dah ratio and which one is larger
    if (valRef2 >= valRef1) {
      aveDitLength = (long)(0.9 * aveDitLength + 0.1 * valRef1);  //Do some dit length averaging
      aveDahLength = (long)(0.9 * aveDahLength + 0.1 * valRef2);
    } else {
      aveDitLength = (long)(0.9 * aveDitLength + 0.1 * valRef2);  // Use larger one. Note reversal of calc order
      aveDahLength = (long)(0.9 * aveDahLength + 0.1 * valRef1);  // Do some dah length averaging
    }
  }
  thresholdGeometricMean = sqrt(aveDitLength * aveDahLength);    //calculate geometric mean
  thresholdArithmeticMean = (aveDitLength + aveDahLength) >> 1;  // Fast divide by 2 on integer data

  signalHistogram[val]++;  // Don't care which half it's in, just put it in

  offset = (uint32_t)thresholdGeometricMean - 1;  // Only do cast once
  // Dit calculation
  // 2nd parameter means we only look for dits below the geomean.

  for (int32_t j = (int32_t)thresholdGeometricMean; j; j--) {
    if (signalHistogram[j] != 0) {
      firstNonEmpty = j;
      break;
    }
  }

  JackClusteredArrayMax(signalHistogram, offset, &tempDit, (int32_t *)&ditLength, &firstNonEmpty, (int32_t)1);
  // dah calculation
  // Elements above the geomean. Note larger spread: higher variance
  JackClusteredArrayMax(&signalHistogram[offset], HISTOGRAM_ELEMENTS - offset, &tempDah, (int32_t *)&dahLength, &firstNonEmpty, (uint32_t)3);
  dahLength += (uint32_t)offset;

  if (tempDit > SCALE_CONSTANT && tempDah > SCALE_CONSTANT) {  //Adaptive dit signalHistogram[]
    for (int k = 0; k < HISTOGRAM_ELEMENTS; k++) {
      signalHistogram[k] = ADAPTIVE_SCALE_FACTOR * signalHistogram[k];
    }
  }
}

/*****
  Purpose: Calculate Goertzal Algorithn to enable decoding CW

  Parameter list:
    int numSamples,         // number of sample in data array
    int TARGET_FREQUENCY,   // frequency for which the magnitude of the transform is to be found
    int SAMPLING_RATE,      // Sampling rate in our case 24ksps
    float* data             // pointer to input data array

  Return value;
    float magnitude     //magnitude of the transform at the target frequency

*****/
float goertzel_mag(int numSamples, int TARGET_FREQUENCY, int SAMPLING_RATE, float *data) {
  int k, i;
  float floatnumSamples;
  float omega, sine, cosine, coeff, q0, q1, q2, magnitude, real, imag;

  float scalingFactor = numSamples / 2.0;

  floatnumSamples = (float)numSamples;
  k = (int)(0.5 + ((floatnumSamples * TARGET_FREQUENCY) / SAMPLING_RATE));
  omega = (2.0 * M_PI * k) / floatnumSamples;
  sine = sin(omega);
  cosine = cos(omega);
  coeff = 2.0 * cosine;
  q0 = 0;
  q1 = 0;
  q2 = 0;

  for (i = 0; i < numSamples; i++) {
    q0 = coeff * q1 - q2 + data[i];
    q2 = q1;
    q1 = q0;
  }
  real = (q1 - q2 * cosine) / scalingFactor;  // calculate the real and imaginary results scaling appropriately
  imag = (q2 * sine) / scalingFactor;

  magnitude = sqrtf(real * real + imag * imag);
  return magnitude;
}

FLASHMEM void initCW(void) {
  //Serial.println(sizeof(float32_t));
  
  int offset = 0;

  float_Corr_Buffer = (float32_t *)&sharedRAM1[0]; // 511 * 4 bytes (round up to 2048)
  offset += 512 * 4;
  gapHistogram = (int32_t *)&sharedRAM2[offset]; // HISTOGRAM_ELEMENTS * 4 bytes (round up to 3072)
  offset += 3072 * 4;
  signalHistogram = (int32_t *)&sharedRAM2[offset]; // HISTOGRAM_ELEMENTS * 4 bytes (round up to 3072)
  offset += 3072 * 4;

  //Serial.println(offset);

}
