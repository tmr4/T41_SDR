
//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

extern uint32_t m_NumTaps;

extern float32_t audioFIRFilterMask[1024];

extern int nfmFilterBW;
extern int currentFilterLoCut, currentFilterHiCut;

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

void SetIIRCoeffs(float32_t *coefficient_set, float32_t f0, float32_t Q, float32_t sample_rate, uint8_t filter_type);

void DoReceiveEQ();
void DoExciterEQ();
void CalcFilters();

void SetupDemodFilterBW();
