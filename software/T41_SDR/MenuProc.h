
//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

extern int calibrateFlag;
extern int IQChoice;

extern bool getMenuValueActive;
extern bool getMenuValueSelected;
extern void (*getMenuValue)();
extern void (*getMenuValueFollowup)();

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

void CWOptions();
void RFOptions();
void VFOSelect(int32_t index);
void VFOSelect();
void EEPROMOptions();
void SpectrumOptions();
void AGCOptions();
void EqualizerRecOptions();
void EqualizerXmtOptions();
void MicGainSet();
void MicOptions();
void IQOptions();
int CalibrateOptions(int IQChoice);

