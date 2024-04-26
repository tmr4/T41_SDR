#include "SDT.h"
#include "EEPROM.h"
#include "debug.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

config_t oldEEPROMData;
int loopCounter = 0;
bool memCheck = false;

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

// set working variables to oldEEPROMData
void EnterLoop() {
  Serial.print("\nEntering loop #");
  Serial.println(loopCounter);

  oldEEPROMData.AGCMode = AGCMode;
  oldEEPROMData.audioVolume = audioVolume;
  oldEEPROMData.rfGainAllBands = rfGainAllBands;
  oldEEPROMData.spectrumNoiseFloor = spectrumNoiseFloor;
  oldEEPROMData.tuneIndex = tuneIndex;
  //oldEEPROMData.ftIncrement = ftIncrement;
  oldEEPROMData.transmitPowerLevel = transmitPowerLevel;
  oldEEPROMData.xmtMode = xmtMode;
  oldEEPROMData.nrOptionSelect = nrOptionSelect;
  oldEEPROMData.currentScale = currentScale;
  oldEEPROMData.spectrumZoom = spectrumZoom;
  oldEEPROMData.spectrum_display_scale = spectrum_display_scale;

  oldEEPROMData.CWFilterIndex = CWFilterIndex;
  oldEEPROMData.paddleDit = paddleDit;
  oldEEPROMData.paddleDah = paddleDah;
  oldEEPROMData.decoderFlag = decoderFlag;
  oldEEPROMData.keyType = keyType;
  oldEEPROMData.currentWPM = currentWPM;
  oldEEPROMData.sidetoneVolume = sidetoneVolume;
  oldEEPROMData.cwTransmitDelay = cwTransmitDelay;

  oldEEPROMData.activeVFO = activeVFO;
  oldEEPROMData.freqIncrement = freqIncrement;

  oldEEPROMData.currentBand = currentBand;
  oldEEPROMData.currentBandA = currentBandA;
  oldEEPROMData.currentBandB = currentBandB;
  oldEEPROMData.currentFreqA = currentFreqA;
  oldEEPROMData.currentFreqB = currentFreqB;
  oldEEPROMData.freqCorrectionFactor = freqCorrectionFactor;

  for (int i = 0; i < EQUALIZER_CELL_COUNT; i++) {
    oldEEPROMData.equalizerRec[i] = equalizerRec[i];
    oldEEPROMData.equalizerXmt[i] = equalizerXmt[i];
  }

  oldEEPROMData.currentMicThreshold = currentMicThreshold;
  oldEEPROMData.currentMicCompRatio = currentMicCompRatio;
  oldEEPROMData.currentMicAttack = currentMicAttack;
  oldEEPROMData.currentMicRelease = currentMicRelease;
  oldEEPROMData.currentMicGain = currentMicGain;

  for (int i = 0; i < NUMBER_OF_SWITCHES; i++) {
    oldEEPROMData.switchValues[0] = switchValues[0];
  }

  oldEEPROMData.LPFcoeff = LPFcoeff;
  oldEEPROMData.NR_PSI = NR_PSI;
  oldEEPROMData.NR_alpha = NR_alpha;
  oldEEPROMData.NR_beta = NR_beta;
  oldEEPROMData.omegaN = omegaN;
  oldEEPROMData.pll_fmax = pll_fmax;

  for (int i = 0; i < NUMBER_OF_BANDS; i++) {
    oldEEPROMData.powerOutCW[i] = powerOutCW[i];
    oldEEPROMData.powerOutSSB[i] = powerOutSSB[i];
    oldEEPROMData.CWPowerCalibrationFactor[i] = CWPowerCalibrationFactor[i];
    oldEEPROMData.SSBPowerCalibrationFactor[i] = SSBPowerCalibrationFactor[i];
    oldEEPROMData.IQAmpCorrectionFactor[i] = IQAmpCorrectionFactor[i];
    oldEEPROMData.IQPhaseCorrectionFactor[i] = IQPhaseCorrectionFactor[i];
    oldEEPROMData.IQXAmpCorrectionFactor[i] = IQXAmpCorrectionFactor[i];
    oldEEPROMData.IQXPhaseCorrectionFactor[i] = IQXPhaseCorrectionFactor[i];
  }

  for (int i = 0; i < 13; i++) {
    oldEEPROMData.favoriteFreqs[i] = favoriteFreqs[i];
  }

  for (int i = 0; i < NUMBER_OF_BANDS; i++) {
    oldEEPROMData.lastFrequencies[i][0] = lastFrequencies[i][0];
    oldEEPROMData.lastFrequencies[i][1] = lastFrequencies[i][1];
  }

  oldEEPROMData.centerFreq = centerFreq;

  strncpy(oldEEPROMData.mapFileName, EEPROMData.mapFileName, 50);
  strncpy(oldEEPROMData.myCall, EEPROMData.myCall, 10);
  strncpy(oldEEPROMData.myTimeZone, EEPROMData.myTimeZone, 10);

  oldEEPROMData.paddleFlip = paddleFlip;
  oldEEPROMData.sdCardPresent = sdCardPresent;

  oldEEPROMData.myLat = myLat;
  oldEEPROMData.myLong = myLong;
  for (int i = 0; i < NUMBER_OF_BANDS; i++) {
    oldEEPROMData.currentNoiseFloor[i] = currentNoiseFloor[i];
  }
  oldEEPROMData.compressorFlag = compressorFlag;

  oldEEPROMData.buttonThresholdPressed = buttonThresholdPressed;
  oldEEPROMData.buttonThresholdReleased = buttonThresholdReleased;
  oldEEPROMData.buttonRepeatDelay = buttonRepeatDelay;
}

// output information about the button that was pressed
//  valPin = value returned from ReadSelectedPushButton()
//  pushButtonSwitchIndex = value returned from ProcessButtonPress()
void ButtonInfoOut(int valPin, int pushButtonSwitchIndex) {
  //Serial.print("buttonState = " );  
  //Serial.println(buttonState);
  if (valPin != BOGUS_PIN_READ) {                        // If a button was pushed...
    Serial.print("  valPin = " );  
    Serial.println(valPin);

    Serial.print("  Switch Index = " );  
    Serial.println(pushButtonSwitchIndex + 1);
  }
}

// note change in working variables
void ExitLoop() {
  if(oldEEPROMData.AGCMode != AGCMode) { Serial.println("  AGCMode changed"); }
  if(oldEEPROMData.audioVolume != audioVolume) { Serial.println("  audioVolume changed"); }
  if(oldEEPROMData.rfGainAllBands != rfGainAllBands) { Serial.println("  rfGainAllBands changed"); }
  if(oldEEPROMData.spectrumNoiseFloor != spectrumNoiseFloor) { Serial.println("  spectrumNoiseFloor changed"); }
  if(oldEEPROMData.tuneIndex != tuneIndex) { Serial.println("  tuneIndex changed"); }
  //if(oldEEPROMData.ftIncrement != ftIncrement) { Serial.println("  ftIncrement changed"); }
  if(oldEEPROMData.transmitPowerLevel != transmitPowerLevel) { Serial.println("  transmitPowerLevel changed"); }
  if(oldEEPROMData.xmtMode != xmtMode) { Serial.println("  xmtMode changed"); }
  if(oldEEPROMData.nrOptionSelect != nrOptionSelect) { Serial.println("  nrOptionSelect changed"); }
  if(oldEEPROMData.currentScale != currentScale) { Serial.println("  currentScale changed"); }
  if(oldEEPROMData.spectrumZoom != spectrumZoom) { Serial.println("  spectrumZoom changed"); }
  if(oldEEPROMData.spectrum_display_scale != spectrum_display_scale) { Serial.println("  spectrum_display_scale changed"); }

  if(oldEEPROMData.CWFilterIndex != CWFilterIndex) { Serial.println("  CWFilterIndex changed"); }
  if(oldEEPROMData.paddleDit != paddleDit) { Serial.println("  paddleDit changed"); }
  if(oldEEPROMData.paddleDah != paddleDah) { Serial.println("  paddleDah changed"); }
  if(oldEEPROMData.decoderFlag != decoderFlag) { Serial.println("  decoderFlag changed"); }
  if(oldEEPROMData.keyType != keyType) { Serial.println("  keyType changed"); }
  if(oldEEPROMData.currentWPM != currentWPM) { Serial.println("  currentWPM changed"); }
  if(oldEEPROMData.sidetoneVolume != sidetoneVolume) { Serial.println("  sidetoneVolume changed"); }
  if(oldEEPROMData.cwTransmitDelay != cwTransmitDelay) { Serial.println("  cwTransmitDelay changed"); }

  if(oldEEPROMData.activeVFO != activeVFO) { Serial.println("  activeVFO changed"); }
  if(oldEEPROMData.freqIncrement != freqIncrement) { Serial.println("  freqIncrement changed"); }

  if(oldEEPROMData.currentBand != currentBand) { Serial.println("  currentBand changed"); }
  if(oldEEPROMData.currentBandA != currentBandA) { Serial.println("  currentBandA changed"); }
  if(oldEEPROMData.currentBandB != currentBandB) { Serial.println("  currentBandB changed"); }
  if(oldEEPROMData.currentFreqA != currentFreqA) { Serial.println("  currentFreqA changed"); }
  if(oldEEPROMData.currentFreqB != currentFreqB) { Serial.println("  currentFreqB changed"); }
  if(oldEEPROMData.freqCorrectionFactor != freqCorrectionFactor) { Serial.println("  freqCorrectionFactor changed"); }

  for(int i = 0; i < EQUALIZER_CELL_COUNT; i++) {
    if(oldEEPROMData.equalizerRec[i] != equalizerRec[i]) { Serial.println("  equalizerRec changed"); }
    if(oldEEPROMData.equalizerXmt[i] != equalizerXmt[i]) { Serial.println("  equalizerXmt changed"); }
  }

  if(oldEEPROMData.currentMicThreshold != currentMicThreshold) { Serial.println("  currentMicThreshold changed"); }
  if(oldEEPROMData.currentMicCompRatio != currentMicCompRatio) { Serial.println("  currentMicCompRatio changed"); }
  if(oldEEPROMData.currentMicAttack != currentMicAttack) { Serial.println("  currentMicAttack changed"); }
  if(oldEEPROMData.currentMicRelease != currentMicRelease) { Serial.println("  currentMicRelease changed"); }
  if(oldEEPROMData.currentMicGain != currentMicGain) { Serial.println("  currentMicGain changed"); }

  for(int i = 0; i < NUMBER_OF_SWITCHES; i++) {
    if(oldEEPROMData.switchValues[0] != switchValues[0]) { Serial.println("  switchValues changed"); }
  }

  if(oldEEPROMData.LPFcoeff != LPFcoeff) { Serial.println("  LPFcoeff changed"); }
  if(oldEEPROMData.NR_PSI != NR_PSI) { Serial.println("  NR_PSI changed"); }
  if(oldEEPROMData.NR_alpha != NR_alpha) { Serial.println("  NR_alpha changed"); }
  if(oldEEPROMData.NR_beta != NR_beta) { Serial.println("  NR_beta changed"); }
  if(oldEEPROMData.omegaN != omegaN) { Serial.println("  omegaN changed"); }
  if(oldEEPROMData.pll_fmax != pll_fmax) { Serial.println("  pll_fmax changed"); }

  for(int i = 0; i < NUMBER_OF_BANDS; i++) {
    if(oldEEPROMData.powerOutCW[i] != powerOutCW[i]) { Serial.println("  powerOutCW changed"); }
    if(oldEEPROMData.powerOutSSB[i] != powerOutSSB[i]) { Serial.println("  powerOutSSB changed"); }
    if(oldEEPROMData.CWPowerCalibrationFactor[i] != CWPowerCalibrationFactor[i]) { Serial.println("  CWPowerCalibrationFactor changed"); }
    if(oldEEPROMData.SSBPowerCalibrationFactor[i] != SSBPowerCalibrationFactor[i]) { Serial.println("  SSBPowerCalibrationFactor changed"); }
    if(oldEEPROMData.IQAmpCorrectionFactor[i] != IQAmpCorrectionFactor[i]) { Serial.println("  IQAmpCorrectionFactor changed"); }
    if(oldEEPROMData.IQPhaseCorrectionFactor[i] != IQPhaseCorrectionFactor[i]) { Serial.println("  IQPhaseCorrectionFactor changed"); }
    if(oldEEPROMData.IQXAmpCorrectionFactor[i] != IQXAmpCorrectionFactor[i]) { Serial.println("  IQXAmpCorrectionFactor changed"); }
    if(oldEEPROMData.IQXPhaseCorrectionFactor[i] != IQXPhaseCorrectionFactor[i]) { Serial.println("  IQXPhaseCorrectionFactor changed"); }
  }

  for(int i = 0; i < 13; i++) {
    if(oldEEPROMData.favoriteFreqs[i] != favoriteFreqs[i]) { Serial.println("  favoriteFreqs changed"); }
  }
  for(int i = 0; i < NUMBER_OF_BANDS; i++) {
    if(oldEEPROMData.lastFrequencies[i][0] != lastFrequencies[i][0]) { Serial.println("  lastFrequencies_0 changed"); }
    if(oldEEPROMData.lastFrequencies[i][1] != lastFrequencies[i][1]) { Serial.println("  lastFrequencies_1 changed"); }
  }

  if(oldEEPROMData.centerFreq != centerFreq) { Serial.println("  centerFreq changed"); }

  if(strcmp(oldEEPROMData.mapFileName, EEPROMData.mapFileName) != 0) { Serial.println("  mapFileName changed"); }
  if(strcmp(oldEEPROMData.myCall, EEPROMData.myCall) != 0) { Serial.println("  myCall changed"); }
  if(strcmp(oldEEPROMData.myTimeZone, EEPROMData.myTimeZone) != 0) { Serial.println("  myTimeZone changed"); }

  if(oldEEPROMData.paddleFlip != paddleFlip) { Serial.println("  paddleFlip changed"); }
  if(oldEEPROMData.sdCardPresent != sdCardPresent) { Serial.println("  sdCardPresent changed"); }

  if(oldEEPROMData.myLat != myLat) { Serial.println("  myLat changed"); }
  if(oldEEPROMData.myLong != myLong) { Serial.println("  myLong changed"); }
  for(int i = 0; i < NUMBER_OF_BANDS; i++) {
    if(oldEEPROMData.currentNoiseFloor[i] != currentNoiseFloor[i]) { Serial.println("  currentNoiseFloor changed"); }
  }
  if(oldEEPROMData.compressorFlag != compressorFlag) { Serial.println("  compressorFlag changed"); }

  if(oldEEPROMData.buttonThresholdPressed != buttonThresholdPressed) { Serial.println("  buttonThresholdPressed changed"); }
  if(oldEEPROMData.buttonThresholdReleased != buttonThresholdReleased) { Serial.println("  buttonThresholdReleased changed"); }
  if(oldEEPROMData.buttonRepeatDelay != buttonRepeatDelay) { Serial.println("  buttonRepeatDelay changed"); }

  Serial.print("Exiting loop #");
  Serial.println(loopCounter++);
}

/*
void EnterLoop() {
  oldEEPROMData.AGCMode = EEPROMData.AGCMode;
  oldEEPROMData.audioVolume = EEPROMData.audioVolume;
  oldEEPROMData.rfGainAllBands = EEPROMData.rfGainAllBands;
  oldEEPROMData.spectrumNoiseFloor = EEPROMData.spectrumNoiseFloor;
  oldEEPROMData.tuneIndex = EEPROMData.tuneIndex;
  oldEEPROMData.ftIncrement = EEPROMData.ftIncrement;
  oldEEPROMData.transmitPowerLevel = EEPROMData.transmitPowerLevel;
  oldEEPROMData.xmtMode = EEPROMData.xmtMode;
  oldEEPROMData.nrOptionSelect = EEPROMData.nrOptionSelect;
  oldEEPROMData.currentScale = EEPROMData.currentScale;
  oldEEPROMData.spectrumZoom = EEPROMData.spectrumZoom;
  oldEEPROMData.spectrum_display_scale = EEPROMData.spectrum_display_scale;

  oldEEPROMData.CWFilterIndex = EEPROMData.CWFilterIndex;
  oldEEPROMData.paddleDit = EEPROMData.paddleDit;
  oldEEPROMData.paddleDah = EEPROMData.paddleDah;
  oldEEPROMData.decoderFlag = EEPROMData.decoderFlag;
  oldEEPROMData.keyType = EEPROMData.keyType;
  oldEEPROMData.currentWPM = EEPROMData.currentWPM;
  oldEEPROMData.sidetoneVolume = EEPROMData.sidetoneVolume;
  oldEEPROMData.cwTransmitDelay = EEPROMData.cwTransmitDelay;

  oldEEPROMData.activeVFO = EEPROMData.activeVFO;
  oldEEPROMData.freqIncrement = EEPROMData.freqIncrement;

  oldEEPROMData.currentBand = EEPROMData.currentBand;
  oldEEPROMData.currentBandA = EEPROMData.currentBandA;
  oldEEPROMData.currentBandB = EEPROMData.currentBandB;
  oldEEPROMData.currentFreqA = EEPROMData.currentFreqA;
  oldEEPROMData.currentFreqB = EEPROMData.currentFreqB;
  oldEEPROMData.freqCorrectionFactor = EEPROMData.freqCorrectionFactor;

  for (int i = 0; i < EQUALIZER_CELL_COUNT; i++) {
    oldEEPROMData.equalizerRec[i] = EEPROMData.equalizerRec[i];
    oldEEPROMData.equalizerXmt[i] = EEPROMData.equalizerXmt[i];
  }

  oldEEPROMData.currentMicThreshold = EEPROMData.currentMicThreshold;
  oldEEPROMData.currentMicCompRatio = EEPROMData.currentMicCompRatio;
  oldEEPROMData.currentMicAttack = EEPROMData.currentMicAttack;
  oldEEPROMData.currentMicRelease = EEPROMData.currentMicRelease;
  oldEEPROMData.currentMicGain = EEPROMData.currentMicGain;

  for (int i = 0; i < NUMBER_OF_SWITCHES; i++) {
    oldEEPROMData.switchValues[0] = EEPROMData.switchValues[0];
  }

  oldEEPROMData.LPFcoeff = EEPROMData.LPFcoeff;
  oldEEPROMData.NR_PSI = EEPROMData.NR_PSI;
  oldEEPROMData.NR_alpha = EEPROMData.NR_alpha;
  oldEEPROMData.NR_beta = EEPROMData.NR_beta;
  oldEEPROMData.omegaN = EEPROMData.omegaN;
  oldEEPROMData.pll_fmax = EEPROMData.pll_fmax;

  for (int i = 0; i < NUMBER_OF_BANDS; i++) {
    oldEEPROMData.powerOutCW[i] = EEPROMData.powerOutCW[i];
    oldEEPROMData.powerOutSSB[i] = EEPROMData.powerOutSSB[i];
    oldEEPROMData.CWPowerCalibrationFactor[i] = EEPROMData.CWPowerCalibrationFactor[i];
    oldEEPROMData.SSBPowerCalibrationFactor[i] = EEPROMData.SSBPowerCalibrationFactor[i];
    oldEEPROMData.IQAmpCorrectionFactor[i] = EEPROMData.IQAmpCorrectionFactor[i];
    oldEEPROMData.IQPhaseCorrectionFactor[i] = EEPROMData.IQPhaseCorrectionFactor[i];
    oldEEPROMData.IQXAmpCorrectionFactor[i] = EEPROMData.IQXAmpCorrectionFactor[i];
    oldEEPROMData.IQXPhaseCorrectionFactor[i] = EEPROMData.IQXPhaseCorrectionFactor[i];
  }

  for (int i = 0; i < 13; i++) {
    oldEEPROMData.favoriteFreqs[i] = EEPROMData.favoriteFreqs[i];
  }

  for (int i = 0; i < NUMBER_OF_BANDS; i++) {
    oldEEPROMData.lastFrequencies[i][0] = EEPROMData.lastFrequencies[i][0];
    oldEEPROMData.lastFrequencies[i][1] = EEPROMData.lastFrequencies[i][1];
  }

  oldEEPROMData.centerFreq = EEPROMData.centerFreq;

  strncpy(oldEEPROMData.mapFileName, EEPROMData.mapFileName, 50);
  strncpy(oldEEPROMData.myCall, EEPROMData.myCall, 10);
  strncpy(oldEEPROMData.myTimeZone, EEPROMData.myTimeZone, 10);

  oldEEPROMData.paddleFlip = EEPROMData.paddleFlip;
  oldEEPROMData.sdCardPresent = EEPROMData.sdCardPresent;

  oldEEPROMData.myLat = EEPROMData.myLat;
  oldEEPROMData.myLong = EEPROMData.myLong;
  for (int i = 0; i < NUMBER_OF_BANDS; i++) {
    oldEEPROMData.currentNoiseFloor[i] = EEPROMData.currentNoiseFloor[i];
  }
  oldEEPROMData.compressorFlag = EEPROMData.compressorFlag;

  oldEEPROMData.buttonThresholdPressed = EEPROMData.buttonThresholdPressed;
  oldEEPROMData.buttonThresholdReleased = EEPROMData.buttonThresholdReleased;
  oldEEPROMData.buttonRepeatDelay = EEPROMData.buttonRepeatDelay;
}

// set oldEEPROMData equal to EEPROMData
void ExitLoop() {
  int loopCount = loopCounter++;
  
  if(oldEEPROMData.AGCMode != EEPROMData.AGCMode) { Serial.print("Loop #: "); Serial.println(loopCount); Serial.println("AGCMode changed"); }
  if(oldEEPROMData.audioVolume != EEPROMData.audioVolume) { Serial.print("Loop #: "); Serial.println(loopCount); Serial.println("audioVolume changed"); }
  if(oldEEPROMData.rfGainAllBands != EEPROMData.rfGainAllBands) { Serial.print("Loop #: "); Serial.println(loopCount); Serial.println("rfGainAllBands changed"); }
  if(oldEEPROMData.spectrumNoiseFloor != EEPROMData.spectrumNoiseFloor) { Serial.print("Loop #: "); Serial.println(loopCount); Serial.println("spectrumNoiseFloor changed"); }
  if(oldEEPROMData.tuneIndex != EEPROMData.tuneIndex) { Serial.print("Loop #: "); Serial.println(loopCount); Serial.println("tuneIndex changed"); }
  if(oldEEPROMData.ftIncrement != EEPROMData.ftIncrement) { Serial.print("Loop #: "); Serial.println(loopCount); Serial.println("ftIncrement changed"); }
  if(oldEEPROMData.transmitPowerLevel != EEPROMData.transmitPowerLevel) { Serial.print("Loop #: "); Serial.println(loopCount); Serial.println("transmitPowerLevel changed"); }
  if(oldEEPROMData.xmtMode != EEPROMData.xmtMode) { Serial.print("Loop #: "); Serial.println(loopCount); Serial.println("xmtMode changed"); }
  if(oldEEPROMData.nrOptionSelect != EEPROMData.nrOptionSelect) { Serial.print("Loop #: "); Serial.println(loopCount); Serial.println("nrOptionSelect changed"); }
  if(oldEEPROMData.currentScale != EEPROMData.currentScale) { Serial.print("Loop #: "); Serial.println(loopCount); Serial.println("currentScale changed"); }
  if(oldEEPROMData.spectrumZoom != EEPROMData.spectrumZoom) { Serial.print("Loop #: "); Serial.println(loopCount); Serial.println("spectrumZoom changed"); }
  if(oldEEPROMData.spectrum_display_scale != EEPROMData.spectrum_display_scale) { Serial.print("Loop #: "); Serial.println(loopCount); Serial.println("spectrum_display_scale changed"); }

  if(oldEEPROMData.CWFilterIndex != EEPROMData.CWFilterIndex) { Serial.print("Loop #: "); Serial.println(loopCount); Serial.println("CWFilterIndex changed"); }
  if(oldEEPROMData.paddleDit != EEPROMData.paddleDit) { Serial.print("Loop #: "); Serial.println(loopCount); Serial.println("paddleDit changed"); }
  if(oldEEPROMData.paddleDah != EEPROMData.paddleDah) { Serial.print("Loop #: "); Serial.println(loopCount); Serial.println("paddleDah changed"); }
  if(oldEEPROMData.decoderFlag != EEPROMData.decoderFlag) { Serial.print("Loop #: "); Serial.println(loopCount); Serial.println("decoderFlag changed"); }
  if(oldEEPROMData.keyType != EEPROMData.keyType) { Serial.print("Loop #: "); Serial.println(loopCount); Serial.println("keyType changed"); }
  if(oldEEPROMData.currentWPM != EEPROMData.currentWPM) { Serial.print("Loop #: "); Serial.println(loopCount); Serial.println("currentWPM changed"); }
  if(oldEEPROMData.sidetoneVolume != EEPROMData.sidetoneVolume) { Serial.print("Loop #: "); Serial.println(loopCount); Serial.println("sidetoneVolume changed"); }
  if(oldEEPROMData.cwTransmitDelay != EEPROMData.cwTransmitDelay) { Serial.print("Loop #: "); Serial.println(loopCount); Serial.println("cwTransmitDelay changed"); }

  if(oldEEPROMData.activeVFO != EEPROMData.activeVFO) { Serial.print("Loop #: "); Serial.println(loopCount); Serial.println("activeVFO changed"); }
  if(oldEEPROMData.freqIncrement != EEPROMData.freqIncrement) { Serial.print("Loop #: "); Serial.println(loopCount); Serial.println("freqIncrement changed"); }

  if(oldEEPROMData.currentBand != EEPROMData.currentBand) { Serial.print("Loop #: "); Serial.println(loopCount); Serial.println("currentBand changed"); }
  if(oldEEPROMData.currentBandA != EEPROMData.currentBandA) { Serial.print("Loop #: "); Serial.println(loopCount); Serial.println("currentBandA changed"); }
  if(oldEEPROMData.currentBandB != EEPROMData.currentBandB) { Serial.print("Loop #: "); Serial.println(loopCount); Serial.println("currentBandB changed"); }
  if(oldEEPROMData.currentFreqA != EEPROMData.currentFreqA) { Serial.print("Loop #: "); Serial.println(loopCount); Serial.println("currentFreqA changed"); }
  if(oldEEPROMData.currentFreqB != EEPROMData.currentFreqB) { Serial.print("Loop #: "); Serial.println(loopCount); Serial.println("currentFreqB changed"); }
  if(oldEEPROMData.freqCorrectionFactor != EEPROMData.freqCorrectionFactor) { Serial.print("Loop #: "); Serial.println(loopCount); Serial.println("freqCorrectionFactor changed"); }

  for(int i = 0; i < EQUALIZER_CELL_COUNT; i++) {
    if(oldEEPROMData.equalizerRec[i] != EEPROMData.equalizerRec[i]) { Serial.print("Loop #: "); Serial.println(loopCount); Serial.println("equalizerRec changed"); }
    if(oldEEPROMData.equalizerXmt[i] != EEPROMData.equalizerXmt[i]) { Serial.print("Loop #: "); Serial.println(loopCount); Serial.println("equalizerXmt changed"); }
  }

  if(oldEEPROMData.currentMicThreshold != EEPROMData.currentMicThreshold) { Serial.print("Loop #: "); Serial.println(loopCount); Serial.println("currentMicThreshold changed"); }
  if(oldEEPROMData.currentMicCompRatio != EEPROMData.currentMicCompRatio) { Serial.print("Loop #: "); Serial.println(loopCount); Serial.println("currentMicCompRatio changed"); }
  if(oldEEPROMData.currentMicAttack != EEPROMData.currentMicAttack) { Serial.print("Loop #: "); Serial.println(loopCount); Serial.println("currentMicAttack changed"); }
  if(oldEEPROMData.currentMicRelease != EEPROMData.currentMicRelease) { Serial.print("Loop #: "); Serial.println(loopCount); Serial.println("currentMicRelease changed"); }
  if(oldEEPROMData.currentMicGain != EEPROMData.currentMicGain) { Serial.print("Loop #: "); Serial.println(loopCount); Serial.println("currentMicGain changed"); }

  for(int i = 0; i < NUMBER_OF_SWITCHES; i++) {
    if(oldEEPROMData.switchValues[0] != EEPROMData.switchValues[0]) { Serial.print("Loop #: "); Serial.println(loopCount); Serial.println("switchValues changed"); }
  }

  if(oldEEPROMData.LPFcoeff != EEPROMData.LPFcoeff) { Serial.print("Loop #: "); Serial.println(loopCount); Serial.println("LPFcoeff changed"); }
  if(oldEEPROMData.NR_PSI != EEPROMData.NR_PSI) { Serial.print("Loop #: "); Serial.println(loopCount); Serial.println("NR_PSI changed"); }
  if(oldEEPROMData.NR_alpha != EEPROMData.NR_alpha) { Serial.print("Loop #: "); Serial.println(loopCount); Serial.println("NR_alpha changed"); }
  if(oldEEPROMData.NR_beta != EEPROMData.NR_beta) { Serial.print("Loop #: "); Serial.println(loopCount); Serial.println("NR_beta changed"); }
  if(oldEEPROMData.omegaN != EEPROMData.omegaN) { Serial.print("Loop #: "); Serial.println(loopCount); Serial.println("omegaN changed"); }
  if(oldEEPROMData.pll_fmax != EEPROMData.pll_fmax) { Serial.print("Loop #: "); Serial.println(loopCount); Serial.println("pll_fmax changed"); }

  for(int i = 0; i < NUMBER_OF_BANDS; i++) {
    if(oldEEPROMData.powerOutCW[i] != EEPROMData.powerOutCW[i]) { Serial.print("Loop #: "); Serial.println(loopCount); Serial.println("powerOutCW changed"); }
    if(oldEEPROMData.powerOutSSB[i] != EEPROMData.powerOutSSB[i]) { Serial.print("Loop #: "); Serial.println(loopCount); Serial.println("powerOutSSB changed"); }
    if(oldEEPROMData.CWPowerCalibrationFactor[i] != EEPROMData.CWPowerCalibrationFactor[i]) { Serial.print("Loop #: "); Serial.println(loopCount); Serial.println("CWPowerCalibrationFactor changed"); }
    if(oldEEPROMData.SSBPowerCalibrationFactor[i] != EEPROMData.SSBPowerCalibrationFactor[i]) { Serial.print("Loop #: "); Serial.println(loopCount); Serial.println("SSBPowerCalibrationFactor changed"); }
    if(oldEEPROMData.IQAmpCorrectionFactor[i] != EEPROMData.IQAmpCorrectionFactor[i]) { Serial.print("Loop #: "); Serial.println(loopCount); Serial.println("IQAmpCorrectionFactor changed"); }
    if(oldEEPROMData.IQPhaseCorrectionFactor[i] != EEPROMData.IQPhaseCorrectionFactor[i]) { Serial.print("Loop #: "); Serial.println(loopCount); Serial.println("IQPhaseCorrectionFactor changed"); }
    if(oldEEPROMData.IQXAmpCorrectionFactor[i] != EEPROMData.IQXAmpCorrectionFactor[i]) { Serial.print("Loop #: "); Serial.println(loopCount); Serial.println("IQXAmpCorrectionFactor changed"); }
    if(oldEEPROMData.IQXPhaseCorrectionFactor[i] != EEPROMData.IQXPhaseCorrectionFactor[i]) { Serial.print("Loop #: "); Serial.println(loopCount); Serial.println("IQXPhaseCorrectionFactor changed"); }
  }

  for(int i = 0; i < 13; i++) {
    if(oldEEPROMData.favoriteFreqs[i] != EEPROMData.favoriteFreqs[i]) { Serial.print("Loop #: "); Serial.println(loopCount); Serial.println("favoriteFreqs changed"); }
  }
  for(int i = 0; i < NUMBER_OF_BANDS; i++) {
    if(oldEEPROMData.lastFrequencies[i][0] != EEPROMData.lastFrequencies[i][0]) { Serial.print("Loop #: "); Serial.println(loopCount); Serial.println("lastFrequencies_0 changed"); }
    if(oldEEPROMData.lastFrequencies[i][1] != EEPROMData.lastFrequencies[i][1]) { Serial.print("Loop #: "); Serial.println(loopCount); Serial.println("lastFrequencies_1 changed"); }
  }

  if(oldEEPROMData.centerFreq != EEPROMData.centerFreq) { Serial.print("Loop #: "); Serial.println(loopCount); Serial.println("centerFreq changed"); }

  if(strcmp(oldEEPROMData.mapFileName, EEPROMData.mapFileName) != 0) { Serial.print("Loop #: "); Serial.println(loopCount); Serial.println("mapFileName changed"); }
  if(strcmp(oldEEPROMData.myCall, EEPROMData.myCall) != 0) { Serial.print("Loop #: "); Serial.println(loopCount); Serial.println("myCall changed"); }
  if(strcmp(oldEEPROMData.myTimeZone, EEPROMData.myTimeZone) != 0) { Serial.print("Loop #: "); Serial.println(loopCount); Serial.println("myTimeZone changed"); }

  if(oldEEPROMData.paddleFlip != EEPROMData.paddleFlip) { Serial.print("Loop #: "); Serial.println(loopCount); Serial.println("paddleFlip changed"); }
  if(oldEEPROMData.sdCardPresent != EEPROMData.sdCardPresent) { Serial.print("Loop #: "); Serial.println(loopCount); Serial.println("sdCardPresent changed"); }

  if(oldEEPROMData.myLat != EEPROMData.myLat) { Serial.print("Loop #: "); Serial.println(loopCount); Serial.println("myLat changed"); }
  if(oldEEPROMData.myLong != EEPROMData.myLong) { Serial.print("Loop #: "); Serial.println(loopCount); Serial.println("myLong changed"); }
  for(int i = 0; i < NUMBER_OF_BANDS; i++) {
    if(oldEEPROMData.currentNoiseFloor[i] != EEPROMData.currentNoiseFloor[i]) { Serial.print("Loop #: "); Serial.println(loopCount); Serial.println("currentNoiseFloor changed"); }
  }
  if(oldEEPROMData.compressorFlag != EEPROMData.compressorFlag) { Serial.print("Loop #: "); Serial.println(loopCount); Serial.println("compressorFlag changed"); }

  if(oldEEPROMData.buttonThresholdPressed != EEPROMData.buttonThresholdPressed) { Serial.print("Loop #: "); Serial.println(loopCount); Serial.println("buttonThresholdPressed changed"); }
  if(oldEEPROMData.buttonThresholdReleased != EEPROMData.buttonThresholdReleased) { Serial.print("Loop #: "); Serial.println(loopCount); Serial.println("buttonThresholdReleased changed"); }
  if(oldEEPROMData.buttonRepeatDelay != EEPROMData.buttonRepeatDelay) { Serial.print("Loop #: "); Serial.println(loopCount); Serial.println("buttonRepeatDelay changed"); }
}

*/

#define printf Serial.printf
extern "C" uint8_t external_psram_size;
  
void memInfo() {
  constexpr auto RAM_BASE   = 0x2020'0000;
                              constexpr auto RAM_SIZE   = 512 << 10;
                              constexpr auto FLASH_BASE = 0x6000'0000;
  constexpr auto FLASH_SIZE = 8 << 20;

  // note: these values are defined by the linker, they are not valid memory
  // locations in all cases - by defining them as arrays, the C++ compiler
  // will use the address of these definitions - it's a big hack, but there's
  // really no clean way to get at linker-defined symbols from the .ld file

  extern char _stext[], _etext[], _sbss[], _ebss[], _sdata[], _edata[],
         _estack[], _heap_start[], _heap_end[], _itcm_block_count[], *__brkval;

  auto sp = (char*) __builtin_frame_address(0);

  printf("_stext        %08x\n",      _stext);
  printf("_etext        %08x +%db\n", _etext, _etext - _stext);
  printf("_sdata        %08x\n",      _sdata);
  printf("_edata        %08x +%db\n", _edata, _edata - _sdata);
  printf("_sbss         %08x\n",      _sbss);
  printf("_ebss         %08x +%db\n", _ebss, _ebss - _sbss);
  printf("curr stack    %08x +%db\n", sp, sp - _ebss);
  printf("_estack       %08x +%db\n", _estack, _estack - sp);
  printf("_heap_start   %08x\n",      _heap_start);
  printf("__brkval      %08x +%db\n", __brkval, __brkval - _heap_start);
  printf("_heap_end     %08x +%db\n", _heap_end, _heap_end - __brkval);
  extern char _extram_start[], _extram_end[], *__brkval;
  printf("_extram_start %08x\n",      _extram_start);
  printf("_extram_end   %08x +%db\n", _extram_end,
         _extram_end - _extram_start);
  printf("\n");

  printf("<ITCM>  %08x .. %08x\n",
         _stext, _stext + ((int) _itcm_block_count << 15) - 1);
  printf("<DTCM>  %08x .. %08x\n",
         _sdata, _estack - 1);
  printf("<RAM>   %08x .. %08x\n",
         RAM_BASE, RAM_BASE + RAM_SIZE - 1);
  printf("<FLASH> %08x .. %08x\n",
         FLASH_BASE, FLASH_BASE + FLASH_SIZE - 1);
  if (external_psram_size > 0)
    printf("<PSRAM> %08x .. %08x\n",
           _extram_start, _extram_start + (external_psram_size << 20) - 1);
  printf("\n");

  auto stack = sp - _ebss;
  printf("avail STACK %8d b %5d kb\n", stack, stack >> 10);

  auto heap = _heap_end - __brkval;
  printf("avail HEAP  %8d b %5d kb\n", heap, heap >> 10);

  auto psram = _extram_start + (external_psram_size << 20) - _extram_end;
  printf("avail PSRAM %8d b %5d kb\n\n\n\n", psram, psram >> 10);
}


uint32_t *ptrFreeITCM;  // Set to Usable ITCM free RAM
uint32_t  sizeofFreeITCM; // sizeof free RAM in uint32_t units.
uint32_t  SizeLeft_etext;
extern char _stext[], _etext[];
FLASHMEM void   getFreeITCM() { // end of CODE ITCM, skip full 32 bits
  Serial.println("\n\n++++++++++++++++++++++");
  SizeLeft_etext = (32 * 1024) - (((uint32_t)&_etext - (uint32_t)&_stext) % (32 * 1024));
  sizeofFreeITCM = SizeLeft_etext - 4;
  sizeofFreeITCM /= sizeof(ptrFreeITCM[0]);
  ptrFreeITCM = (uint32_t *) ( (uint32_t)&_stext + (uint32_t)&_etext + 4 );
  printf( "Size of Free ITCM in Bytes = %u\n", sizeofFreeITCM * sizeof(ptrFreeITCM[0]) );
  printf( "Start of Free ITCM = %u [%X] \n", ptrFreeITCM, ptrFreeITCM);
  printf( "End of Free ITCM = %u [%X] \n", ptrFreeITCM + sizeofFreeITCM, ptrFreeITCM + sizeofFreeITCM);
  for ( uint32_t ii = 0; ii < sizeofFreeITCM; ii++) ptrFreeITCM[ii] = 1;
  uint32_t jj = 0;
  for ( uint32_t ii = 0; ii < sizeofFreeITCM; ii++) jj += ptrFreeITCM[ii];
  printf( "ITCM DWORD cnt = %u [#bytes=%u] \n", jj, jj*4);
}

