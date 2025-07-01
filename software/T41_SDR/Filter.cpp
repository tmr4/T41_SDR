
#include "SDT.h"

#include "ButtonProc.h"
#include "EEPROM.h"
#include "Filter.h"
#include "FIR.h"
#include "pi.h"
#include "Process.h"
#include "Tune.h"
#include "Utility.h"

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

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

#define IIR_ORDER 8
#define IIR_NUMSTAGES (IIR_ORDER / 2)

int nfmFilterBW = 12000;

float32_t recEQ_LevelScale[14];

// EQ Buffers
float32_t DMAMEM EQ1_float_buffer_L[256];
float32_t DMAMEM EQ2_float_buffer_L[256];
float32_t DMAMEM EQ3_float_buffer_L[256];
float32_t DMAMEM EQ4_float_buffer_L[256];
float32_t DMAMEM EQ5_float_buffer_L[256];
float32_t DMAMEM EQ6_float_buffer_L[256];
float32_t DMAMEM EQ7_float_buffer_L[256];
float32_t DMAMEM EQ8_float_buffer_L[256];
float32_t DMAMEM EQ9_float_buffer_L[256];
float32_t DMAMEM EQ10_float_buffer_L[256];
float32_t DMAMEM EQ11_float_buffer_L[256];
float32_t DMAMEM EQ12_float_buffer_L[256];
float32_t DMAMEM EQ13_float_buffer_L[256];
float32_t DMAMEM EQ14_float_buffer_L[256];

float32_t DMAMEM FIR_filter_mask[1024] __attribute__((aligned(4)));

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

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

/*****
  Purpose: set_IIR_coeffs
*****/
FLASHMEM void SetIIRCoeffs(float32_t *coefficient_set, float32_t f0, float32_t Q, float32_t sample_rate, uint8_t filter_type) {

  /*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    Cascaded biquad (notch, peak, lowShelf, highShelf) [DD4WH, april 2016]
    ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
  // DSP Audio-EQ-cookbook for generating the coeffs of the filters on the fly
  // www.musicdsp.org/files/Audio-EQ-Cookbook.txt  [by Robert Bristow-Johnson]
  // https://www.w3.org/2011/audio/audio-eq-cookbook.html
  // the ARM algorithm assumes the biquad form
  // y[n] = b0 * x[n] + b1 * x[n-1] + b2 * x[n-2] + a1 * y[n-1] + a2 * y[n-2]
  //
  // However, the cookbook formulae by Robert Bristow-Johnson AND the Iowa Hills IIR Filter designer
  // use this formula:
  //
  // y[n] = b0 * x[n] + b1 * x[n-1] + b2 * x[n-2] - a1 * y[n-1] - a2 * y[n-2]
  //
  // Therefore, we have to use negated a1 and a2 for use with the ARM function
  if(f0 > sample_rate / 2.0) f0 = sample_rate / 2.0;
  float32_t w0 = f0 * (TWO_PI / sample_rate);
  float32_t sinW0 = sinf(w0);
  float32_t alpha = sinW0 / (Q * 2.0);
  float32_t cosW0 = cosf(w0);
  float32_t scale = 1.0 / (1.0 + alpha);

  if(filter_type == 0) { // lowpass coeffs

    coefficient_set[0] = ((1.0 - cosW0) / 2.0) * scale;   /* b0 */
    coefficient_set[1] = (1.0 - cosW0) * scale;           /* b1 */
    coefficient_set[2] = coefficient_set[0];              /* b2 */
    coefficient_set[3] = (2.0 * cosW0) * scale;           // negated    a1
    coefficient_set[4] = (-1.0 + alpha) * scale;          // negated    a2
  } else if(filter_type == 2) {
    // ??
  } else if(filter_type == 3) {   // notch
    coefficient_set[0] =  1.0;                            /* b0 */
    coefficient_set[1] =  - 2.0 * cosW0;                  /* b1 */
    coefficient_set[2] =  1.0;                            /* b2 */
    coefficient_set[3] =  2.0 * cosW0 * scale;            // negated    a1
    coefficient_set[4] =  alpha - 1.0;                    // negated    a2
  }
}

/*****
  Purpose: DoReceiveEQ
*****/
void DoReceiveEQ() {
  for(int i = 0; i < 14; i++) {
    recEQ_LevelScale[i] = (float)EEPROMData.equalizerRec[i] / 100.0;
  }
  arm_biquad_cascade_df2T_f32(&S1_Rec, float_buffer_L, EQ1_float_buffer_L, 256);
  arm_biquad_cascade_df2T_f32(&S2_Rec, float_buffer_L, EQ2_float_buffer_L, 256);
  arm_biquad_cascade_df2T_f32(&S3_Rec, float_buffer_L, EQ3_float_buffer_L, 256);
  arm_biquad_cascade_df2T_f32(&S4_Rec, float_buffer_L, EQ4_float_buffer_L, 256);
  arm_biquad_cascade_df2T_f32(&S5_Rec, float_buffer_L, EQ5_float_buffer_L, 256);
  arm_biquad_cascade_df2T_f32(&S6_Rec, float_buffer_L, EQ6_float_buffer_L, 256);
  arm_biquad_cascade_df2T_f32(&S7_Rec, float_buffer_L, EQ7_float_buffer_L, 256);
  arm_biquad_cascade_df2T_f32(&S8_Rec, float_buffer_L, EQ8_float_buffer_L, 256);
  arm_biquad_cascade_df2T_f32(&S9_Rec, float_buffer_L, EQ9_float_buffer_L, 256);
  arm_biquad_cascade_df2T_f32(&S10_Rec, float_buffer_L, EQ10_float_buffer_L, 256);
  arm_biquad_cascade_df2T_f32(&S11_Rec, float_buffer_L, EQ11_float_buffer_L, 256);
  arm_biquad_cascade_df2T_f32(&S12_Rec, float_buffer_L, EQ12_float_buffer_L, 256);
  arm_biquad_cascade_df2T_f32(&S13_Rec, float_buffer_L, EQ13_float_buffer_L, 256);
  arm_biquad_cascade_df2T_f32(&S14_Rec, float_buffer_L, EQ14_float_buffer_L, 256);

  arm_scale_f32(EQ1_float_buffer_L, -recEQ_LevelScale[0], EQ1_float_buffer_L, 256);
  arm_scale_f32(EQ2_float_buffer_L, recEQ_LevelScale[1], EQ2_float_buffer_L, 256);
  arm_scale_f32(EQ3_float_buffer_L, -recEQ_LevelScale[2], EQ3_float_buffer_L, 256);
  arm_scale_f32(EQ4_float_buffer_L, recEQ_LevelScale[3], EQ4_float_buffer_L, 256);
  arm_scale_f32(EQ5_float_buffer_L, -recEQ_LevelScale[4], EQ5_float_buffer_L, 256);
  arm_scale_f32(EQ6_float_buffer_L, recEQ_LevelScale[5], EQ6_float_buffer_L, 256);
  arm_scale_f32(EQ7_float_buffer_L, -recEQ_LevelScale[6], EQ7_float_buffer_L, 256);
  arm_scale_f32(EQ8_float_buffer_L, recEQ_LevelScale[7], EQ8_float_buffer_L, 256);
  arm_scale_f32(EQ9_float_buffer_L, -recEQ_LevelScale[8], EQ9_float_buffer_L, 256);
  arm_scale_f32(EQ10_float_buffer_L, recEQ_LevelScale[9], EQ10_float_buffer_L, 256);
  arm_scale_f32(EQ11_float_buffer_L, -recEQ_LevelScale[10], EQ11_float_buffer_L, 256);
  arm_scale_f32(EQ12_float_buffer_L, recEQ_LevelScale[11], EQ12_float_buffer_L, 256);
  arm_scale_f32(EQ13_float_buffer_L, -recEQ_LevelScale[12], EQ13_float_buffer_L, 256);
  arm_scale_f32(EQ14_float_buffer_L, recEQ_LevelScale[13], EQ14_float_buffer_L, 256);

  arm_add_f32(EQ1_float_buffer_L , EQ2_float_buffer_L, float_buffer_L , 256 ) ;

  arm_add_f32(float_buffer_L , EQ3_float_buffer_L, float_buffer_L , 256 ) ;
  arm_add_f32(float_buffer_L , EQ4_float_buffer_L, float_buffer_L , 256 ) ;
  arm_add_f32(float_buffer_L , EQ5_float_buffer_L, float_buffer_L , 256 ) ;
  arm_add_f32(float_buffer_L , EQ6_float_buffer_L, float_buffer_L , 256 ) ;
  arm_add_f32(float_buffer_L , EQ7_float_buffer_L, float_buffer_L , 256 ) ;
  arm_add_f32(float_buffer_L , EQ8_float_buffer_L, float_buffer_L , 256 ) ;
  arm_add_f32(float_buffer_L , EQ9_float_buffer_L, float_buffer_L , 256 ) ;
  arm_add_f32(float_buffer_L , EQ10_float_buffer_L, float_buffer_L , 256 ) ;
  arm_add_f32(float_buffer_L , EQ11_float_buffer_L, float_buffer_L , 256 ) ;
  arm_add_f32(float_buffer_L , EQ12_float_buffer_L, float_buffer_L , 256 ) ;
  arm_add_f32(float_buffer_L , EQ13_float_buffer_L, float_buffer_L , 256 ) ;
  arm_add_f32(float_buffer_L , EQ14_float_buffer_L, float_buffer_L , 256 ) ;
}

/*****
  Purpose: DoExciterEQ
*****/
void DoExciterEQ() {
  for(int i = 0; i < 14; i++) {
    equalizerXmt[i] = (float)EEPROMData.equalizerXmt[i] / 100.0;
  }
  arm_biquad_cascade_df2T_f32(&S1_Xmt,  float_buffer_L_EX, EQ1_float_buffer_L, 256);
  arm_biquad_cascade_df2T_f32(&S2_Xmt,  float_buffer_L_EX, EQ2_float_buffer_L, 256);
  arm_biquad_cascade_df2T_f32(&S3_Xmt,  float_buffer_L_EX, EQ3_float_buffer_L, 256);
  arm_biquad_cascade_df2T_f32(&S4_Xmt,  float_buffer_L_EX, EQ4_float_buffer_L, 256);
  arm_biquad_cascade_df2T_f32(&S5_Xmt,  float_buffer_L_EX, EQ5_float_buffer_L, 256);
  arm_biquad_cascade_df2T_f32(&S6_Xmt,  float_buffer_L_EX, EQ6_float_buffer_L, 256);
  arm_biquad_cascade_df2T_f32(&S7_Xmt,  float_buffer_L_EX, EQ7_float_buffer_L, 256);
  arm_biquad_cascade_df2T_f32(&S8_Xmt,  float_buffer_L_EX, EQ8_float_buffer_L, 256);
  arm_biquad_cascade_df2T_f32(&S9_Xmt,  float_buffer_L_EX, EQ9_float_buffer_L, 256);
  arm_biquad_cascade_df2T_f32(&S10_Xmt, float_buffer_L_EX, EQ10_float_buffer_L, 256);
  arm_biquad_cascade_df2T_f32(&S11_Xmt, float_buffer_L_EX, EQ11_float_buffer_L, 256);
  arm_biquad_cascade_df2T_f32(&S12_Xmt, float_buffer_L_EX, EQ12_float_buffer_L, 256);
  arm_biquad_cascade_df2T_f32(&S13_Xmt, float_buffer_L_EX, EQ13_float_buffer_L, 256);
  arm_biquad_cascade_df2T_f32(&S14_Xmt, float_buffer_L_EX, EQ14_float_buffer_L, 256);

  arm_scale_f32(EQ1_float_buffer_L,  -equalizerXmt[0],  EQ1_float_buffer_L, 256);
  arm_scale_f32(EQ2_float_buffer_L,   equalizerXmt[1],  EQ2_float_buffer_L, 256);
  arm_scale_f32(EQ3_float_buffer_L,  -equalizerXmt[2],  EQ3_float_buffer_L, 256);
  arm_scale_f32(EQ4_float_buffer_L,   equalizerXmt[3],  EQ4_float_buffer_L, 256);
  arm_scale_f32(EQ5_float_buffer_L,  -equalizerXmt[4],  EQ5_float_buffer_L, 256);
  arm_scale_f32(EQ6_float_buffer_L,   equalizerXmt[5],  EQ6_float_buffer_L, 256);
  arm_scale_f32(EQ7_float_buffer_L,  -equalizerXmt[6],  EQ7_float_buffer_L, 256);
  arm_scale_f32(EQ8_float_buffer_L,   equalizerXmt[7],  EQ8_float_buffer_L, 256);
  arm_scale_f32(EQ9_float_buffer_L,  -equalizerXmt[8],  EQ9_float_buffer_L, 256);
  arm_scale_f32(EQ10_float_buffer_L,  equalizerXmt[9],  EQ10_float_buffer_L, 256);
  arm_scale_f32(EQ11_float_buffer_L, -equalizerXmt[10], EQ11_float_buffer_L, 256);
  arm_scale_f32(EQ12_float_buffer_L,  equalizerXmt[11], EQ12_float_buffer_L, 256);
  arm_scale_f32(EQ13_float_buffer_L, -equalizerXmt[12], EQ13_float_buffer_L, 256);
  arm_scale_f32(EQ14_float_buffer_L,  equalizerXmt[13], EQ14_float_buffer_L, 256);

  arm_add_f32(EQ1_float_buffer_L , EQ2_float_buffer_L, float_buffer_L_EX , 256 ) ;

  arm_add_f32(float_buffer_L_EX , EQ3_float_buffer_L,  float_buffer_L_EX , 256 ) ;
  arm_add_f32(float_buffer_L_EX , EQ4_float_buffer_L,  float_buffer_L_EX , 256 ) ;
  arm_add_f32(float_buffer_L_EX , EQ5_float_buffer_L,  float_buffer_L_EX , 256 ) ;
  arm_add_f32(float_buffer_L_EX , EQ6_float_buffer_L,  float_buffer_L_EX , 256 ) ;
  arm_add_f32(float_buffer_L_EX , EQ7_float_buffer_L,  float_buffer_L_EX , 256 ) ;
  arm_add_f32(float_buffer_L_EX , EQ8_float_buffer_L,  float_buffer_L_EX , 256 ) ;
  arm_add_f32(float_buffer_L_EX , EQ9_float_buffer_L,  float_buffer_L_EX , 256 ) ;
  arm_add_f32(float_buffer_L_EX , EQ10_float_buffer_L, float_buffer_L_EX , 256 ) ;
  arm_add_f32(float_buffer_L_EX , EQ11_float_buffer_L, float_buffer_L_EX , 256 ) ;
  arm_add_f32(float_buffer_L_EX , EQ12_float_buffer_L, float_buffer_L_EX , 256 ) ;
  arm_add_f32(float_buffer_L_EX , EQ13_float_buffer_L, float_buffer_L_EX , 256 ) ;
  arm_add_f32(float_buffer_L_EX , EQ14_float_buffer_L, float_buffer_L_EX , 256 ) ;
}

/*****
  Purpose: calculates decimation, interpolation and audio filters
*****/
void CalcFilters() {
  if(bands[currentBand].demod == DEMOD_NFM && nfmBWFilterActive) {

  } else {
    CalcCplxFIRCoeffs(FIR_Coef_I, FIR_Coef_Q, 256 + 1, (float32_t)bands[currentBand].FLoCut, (float32_t)bands[currentBand].FHiCut, 24000.0);
    UpdateFFTFilterMask();

    // and adjust decimation and interpolation filters
    SetDecIntFilters();
  }
}

/*****
  Purpose: UpdateFFTFilterMask()
*****/
void UpdateFFTFilterMask() {
  const arm_cfft_instance_f32* maskS = &arm_cfft_sR_f32_len512;

  /****************************************************************************************
     Calculate the FFT of the FIR filter coefficients once to produce the FIR filter mask
  ****************************************************************************************/
  // the FIR has exactly 256 + 1 taps = coefficients, so we have to add 256 -1 zeros before the FFT
  // in order to produce a 512 point input buffer for the FFT
  // copy coefficients into real values of first part of buffer, rest is zero

  for(unsigned i = 0; i < 256 + 1; i++) {
    // try out a window function to eliminate ringing of the filter at the stop frequency
    //             sd.FFT_Samples[i] = (float32_t)((0.53836 - (0.46164 * arm_cos_f32(PI*2 * (float32_t)i / (float32_t)(FFT_IQ_BUFF_LEN-1)))) * sd.FFT_Samples[i]);
    FIR_filter_mask[i * 2] = FIR_Coef_I[i];
    FIR_filter_mask[i * 2 + 1] = FIR_Coef_Q[i];
  }

  for(unsigned i = 512 + 1; i < 1024; i++) {
    FIR_filter_mask[i] = 0.0;
  }

  // FFT of FIR_filter_mask
  // perform FFT (in-place), needs only to be done once (or every time the filter coeffs change)
  arm_cfft_f32(maskS, FIR_filter_mask, 0, 1);

}

/*****
  Purpose: changes audio filters appropriate for the current demod mode and calculates new filters based on BW
           *** evaluate using just high/low audio filters, without changing back and forth; lilely big code change ***
*****/
FLASHMEM void SetupDemodFilterBW() {
  //float temp;

  switch(bands[currentBand].demod) {
    case DEMOD_USB:
    case DEMOD_PSK31_WAV:
    case DEMOD_PSK31:
    case DEMOD_FT8:
    case DEMOD_FT8_WAV:
      //temp = bands[currentBand].FHiCut;
      //bands[currentBand].FHiCut = -bands[currentBand].FLoCut;
      //bands[currentBand].FLoCut = -temp;
      bands[currentBand].FHiCut =  3000;
      bands[currentBand].FLoCut = 200;
      break;

    case DEMOD_LSB:
      //temp = bands[currentBand].FHiCut;
      //bands[currentBand].FHiCut = -bands[currentBand].FLoCut;
      //bands[currentBand].FLoCut = -temp;
      bands[currentBand].FHiCut =  -200;
      bands[currentBand].FLoCut = -3000;
      break;

    case DEMOD_AM:
    case DEMOD_SAM:
      //bands[currentBand].FHiCut =  -bands[currentBand].FLoCut;
      bands[currentBand].FHiCut =  3000;
      bands[currentBand].FLoCut = -3000;
      break;

    case DEMOD_NFM:
      //temp = min(abs(bands[currentBand].FHiCut), abs(bands[currentBand].FLoCut));
      //bands[currentBand].FHiCut = max(abs(bands[currentBand].FHiCut), abs(bands[currentBand].FLoCut));
      bands[currentBand].FHiCut =  3000;
      bands[currentBand].FLoCut = 200;
      break;

    default:
      bands[currentBand].FHiCut =  3000;
      bands[currentBand].FLoCut = 200;
      break;
  }

  CalcFilters();
}
