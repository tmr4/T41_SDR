
//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

extern uint32_t m_NumTaps;

extern float32_t FIR_Coef_I[(FFT_LENGTH / 2) + 1];
extern float32_t FIR_Coef_Q[(FFT_LENGTH / 2) + 1];
extern float32_t FIR_int1_coeffs[48];
extern float32_t FIR_int2_coeffs[32];
extern float32_t FIR_filter_mask[FFT_LENGTH * 2];

extern int nfmFilterBW;

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

void DoReceiveEQ();
void DoExciterEQ();
void CalcFilters();
void InitFilterMask();
void SetDecIntFilters();

void SetDecIntFilters(int filter_BW);

void UpdateBWFilters();
void SetupMode();
