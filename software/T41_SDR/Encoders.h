
//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

#define MAX_AUDIO_VOLUME        100
#define MIN_AUDIO_VOLUME         16 // adjust to where the band noise disappears

#define ENCODER_DELAY             100L        // Menu options scroll too fast!
#define ENCODER_FACTOR            0.25F       // use 0.25f with cheap encoders that have 4 detents per step, 
                                              // for other encoders or libs we use 1.0f

extern bool volumeChangeFlag;
extern bool fineTuneFlag;
extern bool resetTuningFlag;  // Experimental flag for ResetTuning() due to possible timing issues.  KF5N July 31, 2023

extern long posFilterEncoder;
extern long lastFilterEncoder;
extern long filter_pos_BW;
extern long last_filter_pos_BW;

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

void SetBWFilters();
void EncoderCenterTune();
void EncoderVolume();
float GetEncoderValueLive(float minValue, float maxValue, float startValue, float increment, char prompt[]);
int GetEncoderValue(int minValue, int maxValue, int startValue, int increment, char prompt[]);
int SetWPM();
long SetTransmitDelay();
void EncoderFineTune();
void EncoderMenuChangeFilter();
