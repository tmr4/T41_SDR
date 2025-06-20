
//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

extern uint32_t m_NumTaps;

extern float32_t FIR_filter_mask[1024];

extern int nfmFilterBW;

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

void SetIIRCoeffs(float32_t *coefficient_set, float32_t f0, float32_t Q, float32_t sample_rate, uint8_t filter_type);

void DoReceiveEQ();
void DoExciterEQ();
void CalcFilters();
void UpdateFFTFilterMask();

void SetupDemodFilterBW();
