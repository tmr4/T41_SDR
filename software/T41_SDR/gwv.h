
//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

// Configuration working global variables; to be deleted in favor of using the EEPROMData version, but that's a lot of work
// commented lines have no corresponding global extern

extern char versionSettings[];
extern int AGCMode;
extern int audioVolume;
extern int rfGainAllBands;
extern int spectrumNoiseFloor;
extern int tuneIndex;
extern int ftIndex;
extern int transmitPowerLevel;
extern int xmtMode;
extern int nrOptionSelect;
extern int currentScale;
extern long spectrumZoom;
extern float spectrum_display_scale;

extern int CWFilterIndex;
extern int paddleDit;
extern int paddleDah;
extern int decoderFlag;
extern int keyType;
extern int currentWPM;
extern int sidetoneVolume;
extern int cwTransmitDelay;

extern int activeVFO;
extern int freqIncrement;
extern int ftIncrement;

extern int currentBand;
extern int currentBandA;
extern int currentBandB;
extern int currentFreqA;
extern int currentFreqB;
extern int freqCorrectionFactor;

extern int equalizerRec[];
extern int equalizerXmt[];

extern int currentMicThreshold;
extern float currentMicCompRatio;
extern float currentMicAttack;
extern float currentMicRelease;
extern int currentMicGain;

extern int switchValues[];

extern float LPFcoeff;
extern float NR_PSI;
extern float NR_alpha;
extern float NR_beta;
extern float omegaN ;
extern float pll_fmax;

extern float powerOutCW[];
extern float powerOutSSB[];
extern float CWPowerCalibrationFactor[];
extern float SSBPowerCalibrationFactor[];
extern float IQAmpCorrectionFactor[];
extern float IQPhaseCorrectionFactor[];
extern float IQXAmpCorrectionFactor[];
extern float IQXPhaseCorrectionFactor[];

extern long favoriteFreqs[13];
extern int lastFrequencies[][2];

extern long centerFreq;
extern char mapFileName[];
extern char myCall[];
extern char myTimeZone[];
extern int  separationCharacter;

extern int paddleFlip;
extern int sdCardPresent;

extern float myLat;
extern float myLong;
extern int currentNoiseFloor[];
extern int compressorFlag;

extern int buttonThresholdPressed;
extern int buttonThresholdReleased;
extern int buttonRepeatDelay;

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

void LoadOpVars();
