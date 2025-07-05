
//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

//---- Teensy 4.1 Pin assignments
#ifdef FOURSQRP
    #define VOLUME_ENCODER_A         2
    #define VOLUME_ENCODER_B         3
    #define FILTER_ENCODER_A        16
    #define FILTER_ENCODER_B        15
    #define FINETUNE_ENCODER_A       4
    #ifdef PROJECTSYSTEM
    #define FINETUNE_ENCODER_B      24 // pin 5 is TFT_CS on Project System (the pin assigned here is only meaningful when testing fine tune encoder on non-front panel systems)
    #else
    #define FINETUNE_ENCODER_B       5
    #endif
    #define TUNE_ENCODER_A          14
    #define TUNE_ENCODER_B          17
#else
    #define VOLUME_ENCODER_A         2
    #define VOLUME_ENCODER_B         3
    #define FILTER_ENCODER_A        15
    #define FILTER_ENCODER_B        14
    #define FINETUNE_ENCODER_A       4
    #define FINETUNE_ENCODER_B       5
    #define TUNE_ENCODER_A          16
    #define TUNE_ENCODER_B          17
#endif

#define MAX_AUDIO_VOLUME        100
#define MIN_AUDIO_VOLUME         16 // adjust to where the band noise disappears

#define ENCODER_DELAY             100L        // Menu options scroll too fast!
#define ENCODER_FACTOR            0.25F       // use 0.25f with cheap encoders that have 4 detents per step,
                                              // for other encoders or libs we use 1.0f

extern bool volumeChangeFlag;
extern bool fineTuneFlag;
extern bool resetTuningFlag;  // Experimental flag for ResetTuning() due to possible timing issues.  KF5N July 31, 2023
extern bool getEncoderValueFlag;

extern int posFilterEncoder, lastFilterEncoder;
extern long filter_pos_BW;
extern long last_filter_pos_BW;

extern volatile int menuEncoderMove;
extern volatile long fineTuneEncoderMove;

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

void EncodersInit();

void SetBWFilters();
void EncoderCenterTune();
void EncoderVolumeISR();
float GetEncoderValueLive(float minValue, float maxValue, float startValue, float increment, char prompt[]);
//int GetEncoderValue(int minValue, int maxValue, int startValue, int increment, char prompt[]);
void EncoderFineTuneISR();
void EncoderMenuChangeFilterISR();
