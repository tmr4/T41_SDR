
//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

extern int CWFreqShift, TxRxFreq, NCOFreq;
extern bool splitVFO;

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

void InitSI5351();

void SetSI5351FreqCorFactor(int factor);

void SetFreqCal(long calFreqShift);

void SetCenterTune(int tuneChange);
void SetNCOFreq(int newNCOFreq);
void SetFineTune(int tuneChange);
void SetTxRxFreq(int freq);

void ResetTuning();
void SetFreq();
void DoSplitVFO();
