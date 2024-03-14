#include "SDT.h"
#include "Display.h"
#include "EEPROM.h"
#include "Filter.h"
#include "FIR.h"
#include "Utility.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

#define IIR_ORDER 8
#define IIR_NUMSTAGES (IIR_ORDER / 2)

uint32_t m_NumTaps = (FFT_LENGTH / 2) + 1;
float32_t recEQ_LevelScale[14];

float32_t rec_EQ_Band1_state[IIR_NUMSTAGES * 2] = { 0, 0, 0, 0, 0, 0, 0, 0 };  //declare and zero biquad state variables
float32_t rec_EQ_Band2_state[IIR_NUMSTAGES * 2] = { 0, 0, 0, 0, 0, 0, 0, 0 };
float32_t rec_EQ_Band3_state[IIR_NUMSTAGES * 2] = { 0, 0, 0, 0, 0, 0, 0, 0 };
float32_t rec_EQ_Band4_state[IIR_NUMSTAGES * 2] = { 0, 0, 0, 0, 0, 0, 0, 0 };
float32_t rec_EQ_Band5_state[IIR_NUMSTAGES * 2] = { 0, 0, 0, 0, 0, 0, 0, 0 };
float32_t rec_EQ_Band6_state[IIR_NUMSTAGES * 2] = { 0, 0, 0, 0, 0, 0, 0, 0 };
float32_t rec_EQ_Band7_state[IIR_NUMSTAGES * 2] = { 0, 0, 0, 0, 0, 0, 0, 0 };
float32_t rec_EQ_Band8_state[IIR_NUMSTAGES * 2] = { 0, 0, 0, 0, 0, 0, 0, 0 };  //declare and zero biquad state variables
float32_t rec_EQ_Band9_state[IIR_NUMSTAGES * 2] = { 0, 0, 0, 0, 0, 0, 0, 0 };
float32_t rec_EQ_Band10_state[IIR_NUMSTAGES * 2] = { 0, 0, 0, 0, 0, 0, 0, 0 };
float32_t rec_EQ_Band11_state[IIR_NUMSTAGES * 2] = { 0, 0, 0, 0, 0, 0, 0, 0 };
float32_t rec_EQ_Band12_state[IIR_NUMSTAGES * 2] = { 0, 0, 0, 0, 0, 0, 0, 0 };
float32_t rec_EQ_Band13_state[IIR_NUMSTAGES * 2] = { 0, 0, 0, 0, 0, 0, 0, 0 };
float32_t rec_EQ_Band14_state[IIR_NUMSTAGES * 2] = { 0, 0, 0, 0, 0, 0, 0, 0 };

//EQ filter instances
arm_biquad_cascade_df2T_instance_f32 S1_Rec = { IIR_NUMSTAGES, rec_EQ_Band1_state, EQ_Band1Coeffs };
arm_biquad_cascade_df2T_instance_f32 S2_Rec = { IIR_NUMSTAGES, rec_EQ_Band2_state, EQ_Band2Coeffs };
arm_biquad_cascade_df2T_instance_f32 S3_Rec = { IIR_NUMSTAGES, rec_EQ_Band3_state, EQ_Band3Coeffs };
arm_biquad_cascade_df2T_instance_f32 S4_Rec = { IIR_NUMSTAGES, rec_EQ_Band4_state, EQ_Band4Coeffs };
arm_biquad_cascade_df2T_instance_f32 S5_Rec = { IIR_NUMSTAGES, rec_EQ_Band5_state, EQ_Band5Coeffs };
arm_biquad_cascade_df2T_instance_f32 S6_Rec = { IIR_NUMSTAGES, rec_EQ_Band6_state, EQ_Band6Coeffs };
arm_biquad_cascade_df2T_instance_f32 S7_Rec = { IIR_NUMSTAGES, rec_EQ_Band7_state, EQ_Band7Coeffs };
arm_biquad_cascade_df2T_instance_f32 S8_Rec = { IIR_NUMSTAGES, rec_EQ_Band8_state, EQ_Band8Coeffs };
arm_biquad_cascade_df2T_instance_f32 S9_Rec = { IIR_NUMSTAGES, rec_EQ_Band9_state, EQ_Band9Coeffs };
arm_biquad_cascade_df2T_instance_f32 S10_Rec = { IIR_NUMSTAGES, rec_EQ_Band10_state, EQ_Band10Coeffs };
arm_biquad_cascade_df2T_instance_f32 S11_Rec = { IIR_NUMSTAGES, rec_EQ_Band11_state, EQ_Band11Coeffs };
arm_biquad_cascade_df2T_instance_f32 S12_Rec = { IIR_NUMSTAGES, rec_EQ_Band12_state, EQ_Band12Coeffs };
arm_biquad_cascade_df2T_instance_f32 S13_Rec = { IIR_NUMSTAGES, rec_EQ_Band13_state, EQ_Band13Coeffs };
arm_biquad_cascade_df2T_instance_f32 S14_Rec = { IIR_NUMSTAGES, rec_EQ_Band14_state, EQ_Band14Coeffs };

float32_t rec_EQ1_float_buffer_L[256];
float32_t rec_EQ2_float_buffer_L[256];
float32_t rec_EQ3_float_buffer_L[256];
float32_t rec_EQ4_float_buffer_L[256];
float32_t rec_EQ5_float_buffer_L[256];
float32_t rec_EQ6_float_buffer_L[256];
float32_t rec_EQ7_float_buffer_L[256];
float32_t rec_EQ8_float_buffer_L[256];
float32_t rec_EQ9_float_buffer_L[256];
float32_t rec_EQ10_float_buffer_L[256];
float32_t rec_EQ11_float_buffer_L[256];
float32_t rec_EQ12_float_buffer_L[256];
float32_t rec_EQ13_float_buffer_L[256];
float32_t rec_EQ14_float_buffer_L[256];

float32_t xmt_EQ_Band1_state[IIR_NUMSTAGES * 2] = { 0, 0, 0, 0, 0, 0, 0, 0 };  //declare and zero biquad state variables
float32_t xmt_EQ_Band2_state[IIR_NUMSTAGES * 2] = { 0, 0, 0, 0, 0, 0, 0, 0 };
float32_t xmt_EQ_Band3_state[IIR_NUMSTAGES * 2] = { 0, 0, 0, 0, 0, 0, 0, 0 };
float32_t xmt_EQ_Band4_state[IIR_NUMSTAGES * 2] = { 0, 0, 0, 0, 0, 0, 0, 0 };
float32_t xmt_EQ_Band5_state[IIR_NUMSTAGES * 2] = { 0, 0, 0, 0, 0, 0, 0, 0 };
float32_t xmt_EQ_Band6_state[IIR_NUMSTAGES * 2] = { 0, 0, 0, 0, 0, 0, 0, 0 };
float32_t xmt_EQ_Band7_state[IIR_NUMSTAGES * 2] = { 0, 0, 0, 0, 0, 0, 0, 0 };
float32_t xmt_EQ_Band8_state[IIR_NUMSTAGES * 2] = { 0, 0, 0, 0, 0, 0, 0, 0 };
float32_t xmt_EQ_Band9_state[IIR_NUMSTAGES * 2] = { 0, 0, 0, 0, 0, 0, 0, 0 };
float32_t xmt_EQ_Band10_state[IIR_NUMSTAGES * 2] = { 0, 0, 0, 0, 0, 0, 0, 0 };
float32_t xmt_EQ_Band11_state[IIR_NUMSTAGES * 2] = { 0, 0, 0, 0, 0, 0, 0, 0 };
float32_t xmt_EQ_Band12_state[IIR_NUMSTAGES * 2] = { 0, 0, 0, 0, 0, 0, 0, 0 };
float32_t xmt_EQ_Band13_state[IIR_NUMSTAGES * 2] = { 0, 0, 0, 0, 0, 0, 0, 0 };
float32_t xmt_EQ_Band14_state[IIR_NUMSTAGES * 2] = { 0, 0, 0, 0, 0, 0, 0, 0 };

arm_biquad_cascade_df2T_instance_f32 S1_Xmt = { IIR_NUMSTAGES, xmt_EQ_Band1_state, EQ_Band1Coeffs };
arm_biquad_cascade_df2T_instance_f32 S2_Xmt = { IIR_NUMSTAGES, xmt_EQ_Band2_state, EQ_Band2Coeffs };
arm_biquad_cascade_df2T_instance_f32 S3_Xmt = { IIR_NUMSTAGES, xmt_EQ_Band3_state, EQ_Band3Coeffs };
arm_biquad_cascade_df2T_instance_f32 S4_Xmt = { IIR_NUMSTAGES, xmt_EQ_Band4_state, EQ_Band4Coeffs };
arm_biquad_cascade_df2T_instance_f32 S5_Xmt = { IIR_NUMSTAGES, xmt_EQ_Band5_state, EQ_Band5Coeffs };
arm_biquad_cascade_df2T_instance_f32 S6_Xmt = { IIR_NUMSTAGES, xmt_EQ_Band6_state, EQ_Band6Coeffs };
arm_biquad_cascade_df2T_instance_f32 S7_Xmt = { IIR_NUMSTAGES, xmt_EQ_Band7_state, EQ_Band7Coeffs };
arm_biquad_cascade_df2T_instance_f32 S8_Xmt = { IIR_NUMSTAGES, xmt_EQ_Band8_state, EQ_Band8Coeffs };
arm_biquad_cascade_df2T_instance_f32 S9_Xmt = { IIR_NUMSTAGES, xmt_EQ_Band9_state, EQ_Band9Coeffs };
arm_biquad_cascade_df2T_instance_f32 S10_Xmt = { IIR_NUMSTAGES, xmt_EQ_Band10_state, EQ_Band10Coeffs };
arm_biquad_cascade_df2T_instance_f32 S11_Xmt = { IIR_NUMSTAGES, xmt_EQ_Band11_state, EQ_Band11Coeffs };
arm_biquad_cascade_df2T_instance_f32 S12_Xmt = { IIR_NUMSTAGES, xmt_EQ_Band12_state, EQ_Band12Coeffs };
arm_biquad_cascade_df2T_instance_f32 S13_Xmt = { IIR_NUMSTAGES, xmt_EQ_Band13_state, EQ_Band13Coeffs };
arm_biquad_cascade_df2T_instance_f32 S14_Xmt = { IIR_NUMSTAGES, xmt_EQ_Band14_state, EQ_Band14Coeffs };

//EQBuffers
float32_t DMAMEM xmt_EQ1_float_buffer_L[256];
float32_t DMAMEM xmt_EQ2_float_buffer_L[256];
float32_t DMAMEM xmt_EQ3_float_buffer_L[256];
float32_t DMAMEM xmt_EQ4_float_buffer_L[256];
float32_t DMAMEM xmt_EQ5_float_buffer_L[256];
float32_t DMAMEM xmt_EQ6_float_buffer_L[256];
float32_t DMAMEM xmt_EQ7_float_buffer_L[256];
float32_t DMAMEM xmt_EQ8_float_buffer_L[256];
float32_t DMAMEM xmt_EQ9_float_buffer_L[256];
float32_t DMAMEM xmt_EQ10_float_buffer_L[256];
float32_t DMAMEM xmt_EQ11_float_buffer_L[256];
float32_t DMAMEM xmt_EQ12_float_buffer_L[256];
float32_t DMAMEM xmt_EQ13_float_buffer_L[256];
float32_t DMAMEM xmt_EQ14_float_buffer_L[256];

float32_t DMAMEM FIR_Coef_I[(FFT_LENGTH / 2) + 1];
float32_t DMAMEM FIR_Coef_Q[(FFT_LENGTH / 2) + 1];
float32_t DMAMEM FIR_int1_coeffs[48];
float32_t DMAMEM FIR_int2_coeffs[32];
float32_t DMAMEM FIR_filter_mask[FFT_LENGTH * 2] __attribute__((aligned(4)));

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

/*****
  Purpose: void DoReceiveEQ  Parameter list:
    void
  Return value;
    void
*****/
void DoReceiveEQ() {
  for (int i = 0; i < 14; i++) {
    recEQ_LevelScale[i] = (float)EEPROMData.equalizerRec[i] / 100.0;
  }
  arm_biquad_cascade_df2T_f32(&S1_Rec, float_buffer_L, rec_EQ1_float_buffer_L, 256);
  arm_biquad_cascade_df2T_f32(&S2_Rec, float_buffer_L, rec_EQ2_float_buffer_L, 256);
  arm_biquad_cascade_df2T_f32(&S3_Rec, float_buffer_L, rec_EQ3_float_buffer_L, 256);
  arm_biquad_cascade_df2T_f32(&S4_Rec, float_buffer_L, rec_EQ4_float_buffer_L, 256);
  arm_biquad_cascade_df2T_f32(&S5_Rec, float_buffer_L, rec_EQ5_float_buffer_L, 256);
  arm_biquad_cascade_df2T_f32(&S6_Rec, float_buffer_L, rec_EQ6_float_buffer_L, 256);
  arm_biquad_cascade_df2T_f32(&S7_Rec, float_buffer_L, rec_EQ7_float_buffer_L, 256);
  arm_biquad_cascade_df2T_f32(&S8_Rec, float_buffer_L, rec_EQ8_float_buffer_L, 256);
  arm_biquad_cascade_df2T_f32(&S9_Rec, float_buffer_L, rec_EQ9_float_buffer_L, 256);
  arm_biquad_cascade_df2T_f32(&S10_Rec, float_buffer_L, rec_EQ10_float_buffer_L, 256);
  arm_biquad_cascade_df2T_f32(&S11_Rec, float_buffer_L, rec_EQ11_float_buffer_L, 256);
  arm_biquad_cascade_df2T_f32(&S12_Rec, float_buffer_L, rec_EQ12_float_buffer_L, 256);
  arm_biquad_cascade_df2T_f32(&S13_Rec, float_buffer_L, rec_EQ13_float_buffer_L, 256);
  arm_biquad_cascade_df2T_f32(&S14_Rec, float_buffer_L, rec_EQ14_float_buffer_L, 256);

  arm_scale_f32(rec_EQ1_float_buffer_L, -recEQ_LevelScale[0], rec_EQ1_float_buffer_L, 256);
  arm_scale_f32(rec_EQ2_float_buffer_L, recEQ_LevelScale[1], rec_EQ2_float_buffer_L, 256);
  arm_scale_f32(rec_EQ3_float_buffer_L, -recEQ_LevelScale[2], rec_EQ3_float_buffer_L, 256);
  arm_scale_f32(rec_EQ4_float_buffer_L, recEQ_LevelScale[3], rec_EQ4_float_buffer_L, 256);
  arm_scale_f32(rec_EQ5_float_buffer_L, -recEQ_LevelScale[4], rec_EQ5_float_buffer_L, 256);
  arm_scale_f32(rec_EQ6_float_buffer_L, recEQ_LevelScale[5], rec_EQ6_float_buffer_L, 256);
  arm_scale_f32(rec_EQ7_float_buffer_L, -recEQ_LevelScale[6], rec_EQ7_float_buffer_L, 256);
  arm_scale_f32(rec_EQ8_float_buffer_L, recEQ_LevelScale[7], rec_EQ8_float_buffer_L, 256);
  arm_scale_f32(rec_EQ9_float_buffer_L, -recEQ_LevelScale[8], rec_EQ9_float_buffer_L, 256);
  arm_scale_f32(rec_EQ10_float_buffer_L, recEQ_LevelScale[9], rec_EQ10_float_buffer_L, 256);
  arm_scale_f32(rec_EQ11_float_buffer_L, -recEQ_LevelScale[10], rec_EQ11_float_buffer_L, 256);
  arm_scale_f32(rec_EQ12_float_buffer_L, recEQ_LevelScale[11], rec_EQ12_float_buffer_L, 256);
  arm_scale_f32(rec_EQ13_float_buffer_L, -recEQ_LevelScale[12], rec_EQ13_float_buffer_L, 256);
  arm_scale_f32(rec_EQ14_float_buffer_L, recEQ_LevelScale[13], rec_EQ14_float_buffer_L, 256);

  arm_add_f32(rec_EQ1_float_buffer_L , rec_EQ2_float_buffer_L, float_buffer_L , 256 ) ;

  arm_add_f32(float_buffer_L , rec_EQ3_float_buffer_L, float_buffer_L , 256 ) ;
  arm_add_f32(float_buffer_L , rec_EQ4_float_buffer_L, float_buffer_L , 256 ) ;
  arm_add_f32(float_buffer_L , rec_EQ5_float_buffer_L, float_buffer_L , 256 ) ;
  arm_add_f32(float_buffer_L , rec_EQ6_float_buffer_L, float_buffer_L , 256 ) ;
  arm_add_f32(float_buffer_L , rec_EQ7_float_buffer_L, float_buffer_L , 256 ) ;
  arm_add_f32(float_buffer_L , rec_EQ8_float_buffer_L, float_buffer_L , 256 ) ;
  arm_add_f32(float_buffer_L , rec_EQ9_float_buffer_L, float_buffer_L , 256 ) ;
  arm_add_f32(float_buffer_L , rec_EQ10_float_buffer_L, float_buffer_L , 256 ) ;
  arm_add_f32(float_buffer_L , rec_EQ11_float_buffer_L, float_buffer_L , 256 ) ;
  arm_add_f32(float_buffer_L , rec_EQ12_float_buffer_L, float_buffer_L , 256 ) ;
  arm_add_f32(float_buffer_L , rec_EQ13_float_buffer_L, float_buffer_L , 256 ) ;
  arm_add_f32(float_buffer_L , rec_EQ14_float_buffer_L, float_buffer_L , 256 ) ;
}

/*****
  Purpose: void DoExciterEQ

  Parameter list:
    void
  Return value;
    void
*****/
void DoExciterEQ() {
  for (int i = 0; i < 14; i++) {
    equalizerXmt[i] = (float)EEPROMData.equalizerXmt[i] / 100.0;
  }
  arm_biquad_cascade_df2T_f32(&S1_Xmt,  float_buffer_L_EX, xmt_EQ1_float_buffer_L, 256);
  arm_biquad_cascade_df2T_f32(&S2_Xmt,  float_buffer_L_EX, xmt_EQ2_float_buffer_L, 256);
  arm_biquad_cascade_df2T_f32(&S3_Xmt,  float_buffer_L_EX, xmt_EQ3_float_buffer_L, 256);
  arm_biquad_cascade_df2T_f32(&S4_Xmt,  float_buffer_L_EX, xmt_EQ4_float_buffer_L, 256);
  arm_biquad_cascade_df2T_f32(&S5_Xmt,  float_buffer_L_EX, xmt_EQ5_float_buffer_L, 256);
  arm_biquad_cascade_df2T_f32(&S6_Xmt,  float_buffer_L_EX, xmt_EQ6_float_buffer_L, 256);
  arm_biquad_cascade_df2T_f32(&S7_Xmt,  float_buffer_L_EX, xmt_EQ7_float_buffer_L, 256);
  arm_biquad_cascade_df2T_f32(&S8_Xmt,  float_buffer_L_EX, xmt_EQ8_float_buffer_L, 256);
  arm_biquad_cascade_df2T_f32(&S9_Xmt,  float_buffer_L_EX, xmt_EQ9_float_buffer_L, 256);
  arm_biquad_cascade_df2T_f32(&S10_Xmt, float_buffer_L_EX, xmt_EQ10_float_buffer_L, 256);
  arm_biquad_cascade_df2T_f32(&S11_Xmt, float_buffer_L_EX, xmt_EQ11_float_buffer_L, 256);
  arm_biquad_cascade_df2T_f32(&S12_Xmt, float_buffer_L_EX, xmt_EQ12_float_buffer_L, 256);
  arm_biquad_cascade_df2T_f32(&S13_Xmt, float_buffer_L_EX, xmt_EQ13_float_buffer_L, 256);
  arm_biquad_cascade_df2T_f32(&S14_Xmt, float_buffer_L_EX, xmt_EQ14_float_buffer_L, 256);

  arm_scale_f32(xmt_EQ1_float_buffer_L,  -equalizerXmt[0],  xmt_EQ1_float_buffer_L, 256);
  arm_scale_f32(xmt_EQ2_float_buffer_L,   equalizerXmt[1],  xmt_EQ2_float_buffer_L, 256);
  arm_scale_f32(xmt_EQ3_float_buffer_L,  -equalizerXmt[2],  xmt_EQ3_float_buffer_L, 256);
  arm_scale_f32(xmt_EQ4_float_buffer_L,   equalizerXmt[3],  xmt_EQ4_float_buffer_L, 256);
  arm_scale_f32(xmt_EQ5_float_buffer_L,  -equalizerXmt[4],  xmt_EQ5_float_buffer_L, 256);
  arm_scale_f32(xmt_EQ6_float_buffer_L,   equalizerXmt[5],  xmt_EQ6_float_buffer_L, 256);
  arm_scale_f32(xmt_EQ7_float_buffer_L,  -equalizerXmt[6],  xmt_EQ7_float_buffer_L, 256);
  arm_scale_f32(xmt_EQ8_float_buffer_L,   equalizerXmt[7],  xmt_EQ8_float_buffer_L, 256);
  arm_scale_f32(xmt_EQ9_float_buffer_L,  -equalizerXmt[8],  xmt_EQ9_float_buffer_L, 256);
  arm_scale_f32(xmt_EQ10_float_buffer_L,  equalizerXmt[9],  xmt_EQ10_float_buffer_L, 256);
  arm_scale_f32(xmt_EQ11_float_buffer_L, -equalizerXmt[10], xmt_EQ11_float_buffer_L, 256);
  arm_scale_f32(xmt_EQ12_float_buffer_L,  equalizerXmt[11], xmt_EQ12_float_buffer_L, 256);
  arm_scale_f32(xmt_EQ13_float_buffer_L, -equalizerXmt[12], xmt_EQ13_float_buffer_L, 256);
  arm_scale_f32(xmt_EQ14_float_buffer_L,  equalizerXmt[13], xmt_EQ14_float_buffer_L, 256);

  arm_add_f32(xmt_EQ1_float_buffer_L , xmt_EQ2_float_buffer_L, float_buffer_L_EX , 256 ) ;

  arm_add_f32(float_buffer_L_EX , xmt_EQ3_float_buffer_L,  float_buffer_L_EX , 256 ) ;
  arm_add_f32(float_buffer_L_EX , xmt_EQ4_float_buffer_L,  float_buffer_L_EX , 256 ) ;
  arm_add_f32(float_buffer_L_EX , xmt_EQ5_float_buffer_L,  float_buffer_L_EX , 256 ) ;
  arm_add_f32(float_buffer_L_EX , xmt_EQ6_float_buffer_L,  float_buffer_L_EX , 256 ) ;
  arm_add_f32(float_buffer_L_EX , xmt_EQ7_float_buffer_L,  float_buffer_L_EX , 256 ) ;
  arm_add_f32(float_buffer_L_EX , xmt_EQ8_float_buffer_L,  float_buffer_L_EX , 256 ) ;
  arm_add_f32(float_buffer_L_EX , xmt_EQ9_float_buffer_L,  float_buffer_L_EX , 256 ) ;
  arm_add_f32(float_buffer_L_EX , xmt_EQ10_float_buffer_L, float_buffer_L_EX , 256 ) ;
  arm_add_f32(float_buffer_L_EX , xmt_EQ11_float_buffer_L, float_buffer_L_EX , 256 ) ;
  arm_add_f32(float_buffer_L_EX , xmt_EQ12_float_buffer_L, float_buffer_L_EX , 256 ) ;
  arm_add_f32(float_buffer_L_EX , xmt_EQ13_float_buffer_L, float_buffer_L_EX , 256 ) ;
  arm_add_f32(float_buffer_L_EX , xmt_EQ14_float_buffer_L, float_buffer_L_EX , 256 ) ;
}

/*****
  Purpose: void FilterBandwidth()  Parameter list:
    void
  Return value;
    void
*****/
void FilterBandwidth() {
  AudioNoInterrupts();

  CalcCplxFIRCoeffs(FIR_Coef_I, FIR_Coef_Q, m_NumTaps, (float32_t)bands[currentBand].FLoCut, (float32_t)bands[currentBand].FHiCut, (float)SampleRate / DF);
  InitFilterMask();

  for (int i = 0; i < 5; i++) {
    biquad_lowpass1_coeffs[i] = coefficient_set[i];
  }

  // and adjust decimation and interpolation filters
  SetDecIntFilters();
  ShowBandwidth();
  MyDelay(1L);
  AudioInterrupts();
}

/*****
  Purpose: InitFilterMask()

  Parameter list:
    void

  Return value;
    void
*****/
void InitFilterMask() {

  /****************************************************************************************
     Calculate the FFT of the FIR filter coefficients once to produce the FIR filter mask
  ****************************************************************************************/
  // the FIR has exactly m_NumTaps and a maximum of (FFT_length / 2) + 1 taps = coefficients, so we have to add (FFT_length / 2) -1 zeros before the FFT
  // in order to produce a FFT_length point input buffer for the FFT
  // copy coefficients into real values of first part of buffer, rest is zero

  for (unsigned i = 0; i < m_NumTaps; i++) {
    // try out a window function to eliminate ringing of the filter at the stop frequency
    //             sd.FFT_Samples[i] = (float32_t)((0.53836 - (0.46164 * arm_cos_f32(PI*2 * (float32_t)i / (float32_t)(FFT_IQ_BUFF_LEN-1)))) * sd.FFT_Samples[i]);
    FIR_filter_mask[i * 2] = FIR_Coef_I [i];
    FIR_filter_mask[i * 2 + 1] = FIR_Coef_Q [i];
  }

  for (unsigned i = FFT_length + 1; i < FFT_length * 2; i++) {
    FIR_filter_mask[i] = 0.0;
  }

  // FFT of FIR_filter_mask
  // perform FFT (in-place), needs only to be done once (or every time the filter coeffs change)
  arm_cfft_f32(maskS, FIR_filter_mask, 0, 1);

} // end init_filter_mask

/*****
  Purpose: void control_filter_f()
  Parameter list:
    void
  Return value;
    void
*****/
void ControlFilterF() {
  // low Fcut must never be larger than high Fcut and vice versa

  switch (bands[currentBand].mode) {
    case DEMOD_USB:
      if (bands[currentBand].FLoCut < 0) bands[currentBand].FLoCut = 100;
      break;

    case DEMOD_LSB:
      if (bands[currentBand].FHiCut > 0) bands[currentBand].FHiCut = -100;
      break;

    case DEMOD_AM:
    case DEMOD_NFM:
    case DEMOD_SAM:
      bands[currentBand].FLoCut = - bands[currentBand].FHiCut;
      break;

    case DEMOD_IQ:
      bands[currentBand].FLoCut = - bands[currentBand].FHiCut;
      break;
  }
}

/*****
  Purpose: void SetDecIntFilters()
  Parameter list:
    void
  Return value;
    void
*****/
void SetDecIntFilters() {
  /****************************************************************************************
     Recalculate decimation and interpolation FIR filters
  ****************************************************************************************/
  int filter_BW_highest = bands[currentBand].FHiCut;
  int LP_F_help;

  if (filter_BW_highest < - bands[currentBand].FLoCut) {
    filter_BW_highest = - bands[currentBand].FLoCut;
  }
  LP_F_help = filter_BW_highest;

  if (LP_F_help > 10000) {
    LP_F_help = 10000;
  }
  CalcFIRCoeffs(FIR_dec1_coeffs, n_dec1_taps, (float32_t)(LP_F_help), n_att, 0, 0.0, (float32_t)(SampleRate));
  CalcFIRCoeffs(FIR_dec2_coeffs, n_dec2_taps, (float32_t)(LP_F_help), n_att, 0, 0.0, (float32_t)(SampleRate / DF1));

  CalcFIRCoeffs(FIR_int1_coeffs, 48, (float32_t)(LP_F_help), n_att, 0, 0.0, (float32_t)(SampleRate / DF1));
  CalcFIRCoeffs(FIR_int2_coeffs, 32, (float32_t)(LP_F_help), n_att, 0, 0.0, (float32_t)SampleRate);
  bin_BW = 1.0 / (DF * FFT_length) * (float32_t)SampleRate;
}
