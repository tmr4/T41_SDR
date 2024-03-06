#include "SDT.h"
#include "EEPROM.h"
#include "debug.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

config_t oldEEPROMData;
int loopCounter = 0;

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
  oldEEPROMData.stepFineTune = stepFineTune;
  oldEEPROMData.transmitPowerLevel = transmitPowerLevel;
  oldEEPROMData.xmtMode = xmtMode;
  oldEEPROMData.nrOptionSelect = nrOptionSelect;
  oldEEPROMData.currentScale = currentScale;
  oldEEPROMData.spectrum_zoom = spectrum_zoom;
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
  if(oldEEPROMData.stepFineTune != stepFineTune) { Serial.println("  stepFineTune changed"); }
  if(oldEEPROMData.transmitPowerLevel != transmitPowerLevel) { Serial.println("  transmitPowerLevel changed"); }
  if(oldEEPROMData.xmtMode != xmtMode) { Serial.println("  xmtMode changed"); }
  if(oldEEPROMData.nrOptionSelect != nrOptionSelect) { Serial.println("  nrOptionSelect changed"); }
  if(oldEEPROMData.currentScale != currentScale) { Serial.println("  currentScale changed"); }
  if(oldEEPROMData.spectrum_zoom != spectrum_zoom) { Serial.println("  spectrum_zoom changed"); }
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
  oldEEPROMData.stepFineTune = EEPROMData.stepFineTune;
  oldEEPROMData.transmitPowerLevel = EEPROMData.transmitPowerLevel;
  oldEEPROMData.xmtMode = EEPROMData.xmtMode;
  oldEEPROMData.nrOptionSelect = EEPROMData.nrOptionSelect;
  oldEEPROMData.currentScale = EEPROMData.currentScale;
  oldEEPROMData.spectrum_zoom = EEPROMData.spectrum_zoom;
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
  if(oldEEPROMData.stepFineTune != EEPROMData.stepFineTune) { Serial.print("Loop #: "); Serial.println(loopCount); Serial.println("stepFineTune changed"); }
  if(oldEEPROMData.transmitPowerLevel != EEPROMData.transmitPowerLevel) { Serial.print("Loop #: "); Serial.println(loopCount); Serial.println("transmitPowerLevel changed"); }
  if(oldEEPROMData.xmtMode != EEPROMData.xmtMode) { Serial.print("Loop #: "); Serial.println(loopCount); Serial.println("xmtMode changed"); }
  if(oldEEPROMData.nrOptionSelect != EEPROMData.nrOptionSelect) { Serial.print("Loop #: "); Serial.println(loopCount); Serial.println("nrOptionSelect changed"); }
  if(oldEEPROMData.currentScale != EEPROMData.currentScale) { Serial.print("Loop #: "); Serial.println(loopCount); Serial.println("currentScale changed"); }
  if(oldEEPROMData.spectrum_zoom != EEPROMData.spectrum_zoom) { Serial.print("Loop #: "); Serial.println(loopCount); Serial.println("spectrum_zoom changed"); }
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
