
//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

#undef  round
#undef  PI
#undef  HALF_PI
#undef  TWO_PI
#define PI                          3.1415926535897932384626433832795f
#define HALF_PI                     1.5707963267948966192313216916398f
#define TWO_PI                      6.283185307179586476925286766559f
#define TPI                         TWO_PI
#define PIH                         HALF_PI
#define FOURPI                      (2.0f * TPI)
#define SIXPI                       (3.0f * TPI)

extern float32_t coeffs192K_10K_LPF_FIR[];
extern float32_t coeffs48K_8K_LPF_FIR[];

extern float32_t EQ_Band1Coeffs[];
extern float32_t EQ_Band2Coeffs[];
extern float32_t EQ_Band3Coeffs[];
extern float32_t EQ_Band4Coeffs[];
extern float32_t EQ_Band5Coeffs[];
extern float32_t EQ_Band6Coeffs[];
extern float32_t EQ_Band7Coeffs[];
extern float32_t EQ_Band8Coeffs[];
extern float32_t EQ_Band9Coeffs[];
extern float32_t EQ_Band10Coeffs[];
extern float32_t EQ_Band11Coeffs[];
extern float32_t EQ_Band12Coeffs[];
extern float32_t EQ_Band13Coeffs[];
extern float32_t EQ_Band14Coeffs[];

extern float32_t FIR_Hilbert_coeffs_45[];
extern float32_t FIR_Hilbert_coeffs_neg45[];
extern float32_t CW_Filter_Coeffs2[];
extern float32_t coefficient_set[];
extern float32_t *mag_coeffs[];

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

void CalcFIRCoeffs(float *coeffs_I, int numCoeffs, float32_t fc, float32_t Astop, int type, float dfc, float Fsamprate);
void CalcCplxFIRCoeffs(float * coeffs_I, float * coeffs_Q, int numCoeffs, float32_t FLoCut, float32_t FHiCut, float SampleRate);
void SetIIRCoeffs(float32_t f0, float32_t Q, float32_t sample_rate, uint8_t filter_type);
