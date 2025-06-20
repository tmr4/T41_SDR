
#include <arm_math.h>
#include <arm_const_structs.h>

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

extern arm_fir_decimate_instance_f32 FIR_dec1_I;
extern arm_fir_decimate_instance_f32 FIR_dec1_Q;
extern arm_fir_decimate_instance_f32 FIR_dec2_I;
extern arm_fir_decimate_instance_f32 FIR_dec2_Q;
extern arm_fir_interpolate_instance_f32 FIR_int1_I;
extern arm_fir_interpolate_instance_f32 FIR_int1_Q;
extern arm_fir_interpolate_instance_f32 FIR_int2_I;
extern arm_fir_interpolate_instance_f32 FIR_int2_Q;

extern arm_fir_decimate_instance_f32 FIR_dec1_EX_I;
extern arm_fir_decimate_instance_f32 FIR_dec1_EX_Q;
extern arm_fir_decimate_instance_f32 FIR_dec2_EX_I;
extern arm_fir_decimate_instance_f32 FIR_dec2_EX_Q;
extern arm_fir_interpolate_instance_f32 FIR_int1_EX_I;
extern arm_fir_interpolate_instance_f32 FIR_int1_EX_Q;
extern arm_fir_interpolate_instance_f32 FIR_int2_EX_I;
extern arm_fir_interpolate_instance_f32 FIR_int2_EX_Q;

extern arm_fir_instance_f32 FIR_Hilbert_L;
extern arm_fir_instance_f32 FIR_Hilbert_R;

extern arm_fir_instance_f32 FIR_CW_DecodeL;
extern arm_fir_instance_f32 FIR_CW_DecodeR;

extern float32_t *mag_coeffs[];

extern float32_t FIR_Coef_I[256 + 1];
extern float32_t FIR_Coef_Q[256 + 1];

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

void InitFIRFilter();

void CalcFIRCoeffs(float *coeffs_I, int numCoeffs, float32_t fc, float32_t Astop, int type, float dfc, float Fsamprate);
void CalcCplxFIRCoeffs(float * coeffs_I, float * coeffs_Q, int numCoeffs, float32_t fLoCut, float32_t fHiCut, float sampleRate);

void SetDecIntFilters();
void SetDecIntFilters(int filter_BW);
