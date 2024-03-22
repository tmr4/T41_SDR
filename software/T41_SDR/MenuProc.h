
//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

extern int calibrateFlag;
extern int IQChoice;

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

int CalibrateOptions(int IQChoice);
void CWOptions();
void SpectrumOptions();
void AGCOptions();
void IQOptions();
void EqualizerRecOptions();
void EqualizerXmtOptions();
void MicGainSet();
void MicOptions();
void RFOptions();
void VFOSelect();
void EEPROMOptions();
int SubmenuSelect(const char *options[], int numberOfChoices, int defaultStart);
