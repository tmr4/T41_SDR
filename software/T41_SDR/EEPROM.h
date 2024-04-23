
//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

#define NUMBER_OF_BANDS           7
#define BAND_80M                  0
#define BAND_40M                  1
#define BAND_20M                  2
#define BAND_17M                  3
#define BAND_15M                  4
#define BAND_12M                  5
#define BAND_10M                  6

#define EQUALIZER_CELL_COUNT     14
#define NUMBER_OF_SWITCHES       18 // Number of push button switches

#define EEPROM_BASE_ADDRESS      0U

typedef struct {
  char versionSettings[10];
  int AGCMode;
  int audioVolume;
  int rfGainAllBands;
  int spectrumNoiseFloor;
  int tuneIndex;
  int ftIndex;
  float32_t transmitPowerLevel;
  int xmtMode;
  int nrOptionSelect;
  int currentScale;
  long spectrumZoom;
  float spectrum_display_scale;

  int CWFilterIndex;
  int paddleDit;
  int paddleDah;
  int decoderFlag;
  int keyType;
  int currentWPM;
  float32_t sidetoneVolume;
  uint32_t cwTransmitDelay;

  int activeVFO;
  int freqIncrement;

  int currentBand;
  int currentBandA;
  int currentBandB;
  int currentFreqA;
  int currentFreqB;
  int freqCorrectionFactor;

  int equalizerRec[EQUALIZER_CELL_COUNT];
  int equalizerXmt[EQUALIZER_CELL_COUNT];

  int currentMicThreshold;
  float currentMicCompRatio;
  float currentMicAttack;
  float currentMicRelease;
  int currentMicGain;

  int switchValues[NUMBER_OF_SWITCHES];

  float LPFcoeff;
  float NR_PSI;
  float NR_alpha;
  float NR_beta;
  float omegaN;
  float pll_fmax;

  float powerOutCW[NUMBER_OF_BANDS];
  float powerOutSSB[NUMBER_OF_BANDS];
  float CWPowerCalibrationFactor[NUMBER_OF_BANDS];
  float SSBPowerCalibrationFactor[NUMBER_OF_BANDS];
  float IQAmpCorrectionFactor[NUMBER_OF_BANDS];
  float IQPhaseCorrectionFactor[NUMBER_OF_BANDS];
  float IQXAmpCorrectionFactor[NUMBER_OF_BANDS];
  float IQXPhaseCorrectionFactor[NUMBER_OF_BANDS];

  long favoriteFreqs[13];
  int lastFrequencies[NUMBER_OF_BANDS][2];

  long centerFreq;

  char mapFileName[50];
  char myCall[10];
  char myTimeZone[10];
  int  separationCharacter;
  
  int paddleFlip;
  int sdCardPresent;

  float myLong;
  float myLat;
  int currentNoiseFloor[NUMBER_OF_BANDS];
  int compressorFlag;

  int buttonThresholdPressed;
  int buttonThresholdReleased;
  int buttonRepeatDelay;
} config_t;

extern config_t EEPROMData;

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

void EEPROMWrite();
void EEPROMRead();
void EEPROMShow();
void EEPROMStuffFavorites(unsigned long current[]);
void SetFavoriteFrequency();
void GetFavoriteFrequency();
void EEPROMSaveDefaults2();
int CopySDToEEPROM();
int CopyEEPROMToSD();
void SDEEPROMDump();
void EEPROMStartup();
