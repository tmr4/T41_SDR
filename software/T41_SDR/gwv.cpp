#include "SDT.h"
#include "EEPROM.h"
#include "Utility.h"

#include "debug.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

// Configuration working global variables; to be deleted in favor of using the EEPROMData version, but that's a lot of work
// commented lines have no corresponding global extern.  These need to be initialized, not sure exactly why but audioVolume is critical.  Set them equal to EEPROMData for now.

char versionSettings[10] = VERSION;
int AGCMode = 1;
int audioVolume = 30;
int rfGainAllBands = 1;
int spectrumNoiseFloor = 247; // SPECTRUM_NOISE_FLOOR;
int tuneIndex = DEFAULTFREQINDEX;
int ftIndex = DEFAULT_FT_INDEX;
float32_t transmitPowerLevel = DEFAULT_POWER_LEVEL;
int xmtMode = SSB_MODE;  // 0 = SSB, 1 = CW
int nrOptionSelect = 0;
int currentScale = 1;  // 20 dB/division
long spectrum_zoom = 1; // SPECTRUM_ZOOM_2
float spectrum_display_scale = 20.0;     // 30.0

int CWFilterIndex = 5;
int paddleDit = KEYER_DIT_INPUT_TIP;
int paddleDah = KEYER_DAH_INPUT_RING;
int decoderFlag = DECODER_STATE;  // Startup state for decoder
int keyType = STRAIGHT_KEY_OR_PADDLES;
int currentWPM =  DEFAULT_KEYER_WPM;
float32_t sidetoneVolume = 20;
uint32_t cwTransmitDelay = 750;

int activeVFO = 0;

int freqIncrement = 100000; // *** these need to be automated according to defines in config file ***
int ftIncrement = 500;

int currentBand = BAND_40M;
int currentBandA = BAND_40M;
int currentBandB = BAND_40M;
int currentFreqA = CURRENT_FREQ_A;  //Initial VFOA center freq
int currentFreqB = CURRENT_FREQ_B;  //Initial VFOB center freq
int freqCorrectionFactor = 1200;

int equalizerRec[EQUALIZER_CELL_COUNT] = { 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100 };
int equalizerXmt[EQUALIZER_CELL_COUNT] = {0, 0, 100, 100, 100, 100, 100, 100, 100, 100, 100, 0, 0, 0};   // Provide equalizer optimized for SSB voice based on Neville's tests.  KF5N November 2, 2023

int currentMicThreshold = -10;
float currentMicCompRatio = 5.0;
float currentMicAttack = 0.1;
float currentMicRelease = 2.0;
int currentMicGain = -10;

int switchValues[NUMBER_OF_SWITCHES] = { 905, 853, 802, 752, 705, 653, 604, 556, 502, 451, 399, 344, 291, 237, 181, 124, 65, 4 };

float LPFcoeff = 0.0;
float NR_PSI = 0.0;
float NR_alpha = 0.95;
float NR_beta = 0.85;
float omegaN = 200.0;                       // PLL bandwidth 50.0 - 1000.0
float pll_fmax = +4000.0;

float powerOutCW[NUMBER_OF_BANDS] = { 0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02 };
float powerOutSSB[NUMBER_OF_BANDS] = { 0.03, 0.03, 0.03, 0.03, 0.03, 0.03, 0.03 };
float CWPowerCalibrationFactor[NUMBER_OF_BANDS] = { 0.019, 0.019, .0190, .019, .019, .019, .019 };       // 0.019;
float SSBPowerCalibrationFactor[NUMBER_OF_BANDS] = { 0.008, 0.008, 0.008, 0.008, 0.008, 0.008, 0.008 };  // 0.008
float IQAmpCorrectionFactor[NUMBER_OF_BANDS] = { 1, 1, 1, 1, 1, 1, 1 };
float IQPhaseCorrectionFactor[NUMBER_OF_BANDS] = { 0, 0, 0, 0, 0, 0, 0 };
float IQXAmpCorrectionFactor[NUMBER_OF_BANDS] = { 1, 1, 1, 1, 1, 1, 1 };
float IQXPhaseCorrectionFactor[NUMBER_OF_BANDS] = { 0, 0, 0, 0, 0, 0, 0 };

long favoriteFreqs[13] = { 3560000, 3690000, 7030000, 7200000, 14060000, 14200000, 21060000, 21285000, 28060000, 28365000, 5000000, 10000000, 15000000 };
int lastFrequencies[NUMBER_OF_BANDS][2] = { { 3548000, 3560000 }, { 7048000, 7030000 }, { 14048000, 14100000 }, { 18116000, 18110000 }, { 21048000, 21150000 }, { 24937000, 24930000 }, { 28048000, 28200000 } };

long centerFreq = 7048000;
char mapFileName[50];
char myCall[10];
char myTimeZone[10];
int separationCharacter = (int) '.';

int paddleFlip = PADDLE_FLIP;
int sdCardPresent = 0;  // Do they have an micro SD card installed?

float myLat = MY_LAT;
float myLong = MY_LON;
int currentNoiseFloor[NUMBER_OF_BANDS] = { 0, 0, 0, 0, 0, 0, 0 };
int compressorFlag = 0;

#ifndef ALT_ISR
int buttonThresholdPressed = 944;   // switchValues[0] + WIGGLE_ROOM
int buttonThresholdReleased = 964;  // buttonThresholdPressed + WIGGLE_ROOM
int buttonRepeatDelay = 300000;     // Increased to 300000 from 200000 to better handle cheap, wornout buttons.
#else
int buttonThresholdPressed = switchValues[0] + WIGGLE_ROOM;   // switchValues[0] + WIGGLE_ROOM
int buttonThresholdReleased = buttonThresholdPressed + WIGGLE_ROOM;  // buttonThresholdPressed + WIGGLE_ROOM
int buttonRepeatDelay = 0;     // Increased to 300000 from 200000 to better handle cheap, wornout buttons.
#endif

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

// temp function to load EEPROM data into operating variables
void LoadOpVars() {
  AGCMode = EEPROMData.AGCMode;
  audioVolume = EEPROMData.audioVolume;
  rfGainAllBands = EEPROMData.rfGainAllBands;
  spectrumNoiseFloor = EEPROMData.spectrumNoiseFloor;
  tuneIndex = EEPROMData.tuneIndex;
  ftIndex = EEPROMData.ftIndex;
  transmitPowerLevel = EEPROMData.transmitPowerLevel;
  xmtMode = EEPROMData.xmtMode;
  nrOptionSelect = EEPROMData.nrOptionSelect;
  currentScale = EEPROMData.currentScale;
  spectrum_zoom = EEPROMData.spectrum_zoom;
  spectrum_display_scale = EEPROMData.spectrum_display_scale;

  CWFilterIndex = EEPROMData.CWFilterIndex;
  paddleDit = EEPROMData.paddleDit;
  paddleDah = EEPROMData.paddleDah;
  decoderFlag = EEPROMData.decoderFlag;
  keyType = EEPROMData.keyType;
  currentWPM = EEPROMData.currentWPM;
  sidetoneVolume = EEPROMData.sidetoneVolume;
  cwTransmitDelay = EEPROMData.cwTransmitDelay;

  activeVFO = EEPROMData.activeVFO;
  freqIncrement = EEPROMData.freqIncrement; // *** this isn't needed if tuneIndex is used to set initial value ***

  currentBand = EEPROMData.currentBand;
  currentBandA = EEPROMData.currentBandA;
  currentBandB = EEPROMData.currentBandB;
//  currentFreqA = EEPROMData.lastFrequencies[currentBandA][0];
//  currentFreqB = EEPROMData.lastFrequencies[currentBandB][1];
  currentFreqA = EEPROMData.currentFreqA;
  currentFreqB = EEPROMData.currentFreqB;
  freqCorrectionFactor = EEPROMData.freqCorrectionFactor;

  for (int i = 0; i < EQUALIZER_CELL_COUNT; i++) {
    equalizerRec[i] = EEPROMData.equalizerRec[i];
    equalizerXmt[i] = EEPROMData.equalizerXmt[i];
  }

  currentMicThreshold = EEPROMData.currentMicThreshold;
  currentMicCompRatio = EEPROMData.currentMicCompRatio;
  currentMicAttack = EEPROMData.currentMicAttack;
  currentMicRelease = EEPROMData.currentMicRelease;
  currentMicGain = EEPROMData.currentMicGain;

  for (int i = 0; i < NUMBER_OF_SWITCHES; i++) {
    switchValues[0] = EEPROMData.switchValues[0];
  }

  LPFcoeff = EEPROMData.LPFcoeff;
  NR_PSI = EEPROMData.NR_PSI;
  NR_alpha = EEPROMData.NR_alpha;
  NR_beta = EEPROMData.NR_beta;
  omegaN = EEPROMData.omegaN;
  pll_fmax = EEPROMData.pll_fmax;

  for (int i = 0; i < NUMBER_OF_BANDS; i++) {
    powerOutCW[i] = EEPROMData.powerOutCW[i];
    powerOutSSB[i] = EEPROMData.powerOutSSB[i];
    CWPowerCalibrationFactor[i] = EEPROMData.CWPowerCalibrationFactor[i];
    SSBPowerCalibrationFactor[i] = EEPROMData.SSBPowerCalibrationFactor[i];
    IQAmpCorrectionFactor[i] = EEPROMData.IQAmpCorrectionFactor[i];
    IQPhaseCorrectionFactor[i] = EEPROMData.IQPhaseCorrectionFactor[i];
    IQXAmpCorrectionFactor[i] = EEPROMData.IQXAmpCorrectionFactor[i];
    IQXPhaseCorrectionFactor[i] = EEPROMData.IQXPhaseCorrectionFactor[i];
  }

  for (int i = 0; i < 13; i++) {
    favoriteFreqs[i] = EEPROMData.favoriteFreqs[i];
  }

  for (int i = 0; i < NUMBER_OF_BANDS; i++) {
    lastFrequencies[i][0] = EEPROMData.lastFrequencies[i][0];
    lastFrequencies[i][1] = EEPROMData.lastFrequencies[i][1];
  }

//  centerFreq = EEPROMData.lastFrequencies[currentBand][activeVFO];
  centerFreq = EEPROMData.centerFreq;

  strncpy(mapFileName, EEPROMData.mapFileName, 50);
  strncpy(myCall, EEPROMData.myCall, 10);
  strncpy(myTimeZone, EEPROMData.myTimeZone, 10);

  paddleFlip = EEPROMData.paddleFlip;
  sdCardPresent = EEPROMData.sdCardPresent;

  myLat = EEPROMData.myLat;
  myLong = EEPROMData.myLong;
  for (int i = 0; i < NUMBER_OF_BANDS; i++) {
    currentNoiseFloor[i] = EEPROMData.currentNoiseFloor[i];
  }
  compressorFlag = EEPROMData.compressorFlag;

  buttonThresholdPressed = EEPROMData.buttonThresholdPressed;
  buttonThresholdReleased = EEPROMData.buttonThresholdReleased;
  buttonRepeatDelay = EEPROMData.buttonRepeatDelay;
}
