
//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

extern long CWFreqShift, TxRxFreq, NCOFreq;
extern bool splitVFO;

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

void InitSI5351();

void SetSI5351FreqCorFactor(int factor);

void SetFreqCal(long calFreqShift);
void SetCenterTune(long tuneChange);
void SetNCOFreq(long newNCOFreq);
void SetFineTune(long tuneChange);
void ResetTuning();
void SetFreq();
void DoSplitVFO();
void SetTxRxFreq(long freq);
