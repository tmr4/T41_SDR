
//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

extern bool lowerAudioFilterActive;
extern int liveNoiseFloorFlag;

extern bool nfmBWFilterActive;

extern int priorDemodMode;

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

void ButtonMenuDown();
void ButtonMenuUp();
void ButtonBandChange();
void ButtonZoom();
void ButtonFilter();
void ButtonDemodMode();
void ButtonMode();
void ButtonNR();
void ButtonNotchFilter();
void ButtonFrequencyEntry();
void ToggleLiveNoiseFloorFlag();
