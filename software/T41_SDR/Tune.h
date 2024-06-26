
//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

extern long TxRxFreq;
extern bool splitVFO;

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

void SetFreqCal(void);
void SetCenterTune(long tuneChange);
void SetNCOFreq(long newNCOFreq);
void SetFineTune(long tuneChange);
void ResetTuning();
void SetFreq();
void DoSplitVFO();
void SetTxRxFreq(long freq);
