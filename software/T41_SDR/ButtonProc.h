
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

void ButtonMenuDown();
void ButtonMenuUp();
void BandChange(int change);
void ButtonFilter();
void ButtonDemodMode();
void ButtonMode();
void ButtonNR();
void ButtonNotchFilter();
void ButtonFrequencyEntry();
void ToggleLiveNoiseFloorFlag();
