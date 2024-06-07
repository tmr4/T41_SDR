
//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

#define MAX_ZOOM_ENTRIES      5

extern bool lowerAudioFilterActive;
extern int liveNoiseFloorFlag;

extern bool nfmBWFilterActive;
extern bool ft8MsgSelectActive;

extern int priorDemodMode;
extern int currentDataMode;

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

void ChangeBand(int change);
void ChangeBand(long newFreq);
void ButtonFilter();
void ChangeDemodMode(int mode);
void ChangeMode(int mode);
void ButtonNR();
void ButtonNotchFilter();
void ButtonFrequencyEntry();
void ToggleLiveNoiseFloorFlag();

void ChangeFreqIncrement(int change);
void ChangeFtIncrement(int change);
