
//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

extern int calibrateFlag;
extern int IQChoice;

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

int CalibrateOptions(int IQChoice);
int CWOptions();
int SpectrumOptions();
int AGCOptions();
int IQOptions();
int EqualizerRecOptions();
int EqualizerXmtOptions();
int MicGainSet();
int MicOptions();
int RFOptions();
int VFOSelect();
int EEPROMOptions();
int SubmenuSelect(const char *options[], int numberOfChoices, int defaultStart);
