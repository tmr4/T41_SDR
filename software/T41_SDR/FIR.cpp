
#include "SDT.h"
#include "CWProcessing.h"
#include "FIR.h"
#include "pi.h"
#include "Utility.h"

#include "FIRcoef.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

uint8_t FIR_filter_window = 1; // 1 - 4-term Blackman-Harris, 2 - sine, 3 - cosine, 4 - Hann, other - Blackman-Nuttall

// receiver decimation/interpolation state and instance arrays
float32_t DMAMEM FIR_dec1_I_state[2074]; // numtaps+blocksize-1 = 27+2048-1 = 2074
float32_t DMAMEM FIR_dec1_Q_state[2074];
float32_t DMAMEM FIR_dec2_I_state[544]; // numtaps+blocksize-1 = 33+512-1 = 544
float32_t DMAMEM FIR_dec2_Q_state[544];

float32_t DMAMEM FIR_int1_I_state[279]; // (numTaps/L)+blockSize-1 = 48/2+256-1 = 279
float32_t DMAMEM FIR_int1_Q_state[279];
float32_t DMAMEM FIR_int2_I_state[519]; // (numTaps/L)+blockSize-1 = 32/4+512-1 = 519
float32_t DMAMEM FIR_int2_Q_state[519];

arm_fir_decimate_instance_f32 FIR_dec1_I;
arm_fir_decimate_instance_f32 FIR_dec1_Q;
arm_fir_decimate_instance_f32 FIR_dec2_I;
arm_fir_decimate_instance_f32 FIR_dec2_Q;

arm_fir_interpolate_instance_f32 FIR_int1_I;
arm_fir_interpolate_instance_f32 FIR_int1_Q;
arm_fir_interpolate_instance_f32 FIR_int2_I;
arm_fir_interpolate_instance_f32 FIR_int2_Q;

float32_t DMAMEM FIR_dec1_coeffs[27];
float32_t DMAMEM FIR_dec2_coeffs[33];
float32_t DMAMEM FIR_int1_coeffs[48];
float32_t DMAMEM FIR_int2_coeffs[32];

// exciter decimation/interpolation state and instance arrays
float32_t DMAMEM FIR_dec1_EX_I_state[2095]; // numtaps+blocksize-1 = 48+2048-1 = 2095
float32_t DMAMEM FIR_dec1_EX_Q_state[2095];
float32_t DMAMEM FIR_dec2_EX_I_state[559]; // numtaps+blocksize-1 = 48+512-1 = 559
float32_t DMAMEM FIR_dec2_EX_Q_state[559];

float32_t DMAMEM FIR_int2_EX_I_state[523]; // (numTaps/L)+blockSize-1 = 48/4+512-1 = 523
float32_t DMAMEM FIR_int2_EX_Q_state[523];
float32_t DMAMEM FIR_int1_EX_I_state[279]; // (numTaps/L)+blockSize-1 = 48/2+256-1 = 279
float32_t DMAMEM FIR_int1_EX_Q_state[279];

arm_fir_decimate_instance_f32 FIR_dec1_EX_I;
arm_fir_decimate_instance_f32 FIR_dec1_EX_Q;
arm_fir_decimate_instance_f32 FIR_dec2_EX_I;
arm_fir_decimate_instance_f32 FIR_dec2_EX_Q;

arm_fir_interpolate_instance_f32 FIR_int1_EX_I;
arm_fir_interpolate_instance_f32 FIR_int1_EX_Q;
arm_fir_interpolate_instance_f32 FIR_int2_EX_I;
arm_fir_interpolate_instance_f32 FIR_int2_EX_Q;

//Hilbert FIR Filters
float32_t FIR_Hilbert_state_L[100 + 256 - 1];
float32_t FIR_Hilbert_state_R[100 + 256 - 1];

arm_fir_instance_f32 FIR_Hilbert_L;
arm_fir_instance_f32 FIR_Hilbert_R;

// CW decode Filters
float32_t FIR_CW_DecodeL_state[64 + 256 - 1];
float32_t FIR_CW_DecodeR_state[64 + 256 - 1];

arm_fir_instance_f32 FIR_CW_DecodeL;
arm_fir_instance_f32 FIR_CW_DecodeR;

float32_t DMAMEM FIR_Coef_I[256 + 1];
float32_t DMAMEM FIR_Coef_Q[256 + 1];

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

void InitFIRFilter() {
  /****************************************************************************************
     Receive decimation and interpolation FIR filters design

      Two-stage decimation and interpolation
        1st stage decimation factor = 4
        2nd stage decimation factor = 2
        desired stopband attenuation = 90
        desired max BW of the filters = 9k
        samplerate before decimation = 192k (original code has 176k which is from Teensy Convolution SDR, why use it here?)
      see Lyons 2011 Two-Stage Decimation chapter 10.2 for the theory (equation 10.3)

      const float32_t n_fpass1 = 9.0 / n_samplerate;
      const float32_t n_fpass2 = 9.0 / (n_samplerate / 4.0);
      const float32_t n_fstop1 = ((n_samplerate / 4.0) - 9.0) / n_samplerate;
      const float32_t n_fstop2 = ((n_samplerate / 8.0) - 9.0) / (n_samplerate / 4.0);

      const uint16_t n_dec1_taps = (1 + (uint16_t)(90.0 / (22.0 * (n_fstop1 - n_fpass1))));
      const uint16_t n_dec2_taps = (1 + (uint16_t)(90.0 / (22.0 * (n_fstop2 - n_fpass2))));

      gives: n_dec1_taps = 27, n_dec2_taps = 33 (see decFilterTaps.xlsx)
  ****************************************************************************************/
  // Decimation filter 1, M1 = 4
  CalcFIRCoeffs(FIR_dec1_coeffs, 27, 9000.0, 90.0, 0, 0.0, 192000.0);
  arm_fir_decimate_init_f32(&FIR_dec1_I, 27, 4.0, FIR_dec1_coeffs, FIR_dec1_I_state, 2048);
  arm_fir_decimate_init_f32(&FIR_dec1_Q, 27, 4.0, FIR_dec1_coeffs, FIR_dec1_Q_state, 2048);

  // Decimation filter 2, M2 = 2
  CalcFIRCoeffs(FIR_dec2_coeffs, 33, 9000.0, 90.0, 0, 0.0, 48000.0);
  arm_fir_decimate_init_f32(&FIR_dec2_I, 33, 2, FIR_dec2_coeffs, FIR_dec2_I_state, 512);
  arm_fir_decimate_init_f32(&FIR_dec2_Q, 33, 2, FIR_dec2_coeffs, FIR_dec2_Q_state, 512);

  // Interpolation filter 1, L1 = 2
  CalcFIRCoeffs(FIR_int1_coeffs, 48, 9000.0, 90.0, 0, 0.0, 48000.0);
  arm_fir_interpolate_init_f32(&FIR_int1_I, 2, 48, FIR_int1_coeffs, FIR_int1_I_state, 256);
  arm_fir_interpolate_init_f32(&FIR_int1_Q, 2, 48, FIR_int1_coeffs, FIR_int1_Q_state, 256);

  // Interpolation filter 2, L2 = 4
  CalcFIRCoeffs(FIR_int2_coeffs, 32, 9000.0, 90.0, 0, 0.0, 192000.0);
  arm_fir_interpolate_init_f32(&FIR_int2_I, 4.0, 32, FIR_int2_coeffs, FIR_int2_I_state, 512);
  arm_fir_interpolate_init_f32(&FIR_int2_Q, 4.0, 32, FIR_int2_coeffs, FIR_int2_Q_state, 512);

  // excite decimate/interpolate filter initialization
  arm_fir_decimate_init_f32(&FIR_dec1_EX_I, 48, 4, coeffs192K_10K_LPF_FIR, FIR_dec1_EX_I_state, 2048);
  arm_fir_decimate_init_f32(&FIR_dec1_EX_Q, 48, 4, coeffs192K_10K_LPF_FIR, FIR_dec1_EX_Q_state, 2048);
  arm_fir_decimate_init_f32(&FIR_dec2_EX_I, 48, 2, coeffs48K_8K_LPF_FIR, FIR_dec2_EX_I_state, 512);
  arm_fir_decimate_init_f32(&FIR_dec2_EX_Q, 48, 2, coeffs48K_8K_LPF_FIR, FIR_dec2_EX_Q_state, 512);

  arm_fir_interpolate_init_f32(&FIR_int1_EX_I, 2, 48, coeffs48K_8K_LPF_FIR, FIR_int1_EX_I_state, 256);
  arm_fir_interpolate_init_f32(&FIR_int1_EX_Q, 2, 48, coeffs48K_8K_LPF_FIR, FIR_int1_EX_Q_state, 256);
  arm_fir_interpolate_init_f32(&FIR_int2_EX_I, 4, 48, coeffs192K_10K_LPF_FIR, FIR_int2_EX_I_state, 512);
  arm_fir_interpolate_init_f32(&FIR_int2_EX_Q, 4, 48, coeffs192K_10K_LPF_FIR, FIR_int2_EX_Q_state, 512);

  // Hilbert filters
  arm_fir_init_f32(&FIR_Hilbert_L, 100, FIR_Hilbert_coeffs_45, FIR_Hilbert_state_L, 256);
  arm_fir_init_f32(&FIR_Hilbert_R, 100, FIR_Hilbert_coeffs_neg45, FIR_Hilbert_state_R, 256);

  // CW filters
  arm_fir_init_f32(&FIR_CW_DecodeL, 64, CW_Filter_Coeffs2, FIR_CW_DecodeL_state, 256);
  arm_fir_init_f32(&FIR_CW_DecodeR, 64, CW_Filter_Coeffs2, FIR_CW_DecodeR_state, 256);

  // set filter BW based on current band filter cutoffs
  CalcCplxFIRCoeffs(FIR_Coef_I, FIR_Coef_Q, 256 + 1, (float32_t)bands[currentBand].FLoCut, (float32_t)bands[currentBand].FHiCut, 24000.0);
  SetDecIntFilters();
}

/*****
  Purpose: calc_FIR_coeffs
    // pointer to coefficients variable, no. of coefficients to calculate, frequency where it happens, stopband attenuation in dB,
    // filter type, half-filter bandwidth (only for bandpass and notch)

  Parameter list:
    float * coeffs_I
    int numCoeffs
    float32_t fc
    float32_t Astop
    int type
    float dfc
    float Fsamprate

  Return value:
    void
*****/
void CalcFIRCoeffs(float *coeffs_I, int numCoeffs, float32_t fc, float32_t Astop, int type, float dfc, float Fsamprate) {
  // modified by WMXZ and DD4WH after
  // Wheatley, M. (2011): CuteSDR Technical Manual. www.metronix.com, pages 118 - 120, FIR with Kaiser-Bessel Window
  // assess required number of coefficients by
  //     numCoeffs = (Astop - 8.0) / (2.285 * TWO_PI * normFtrans);
  // selecting high-pass, numCoeffs is forced to an even number for better frequency response

  int nc    = numCoeffs;
  float32_t Beta;
  float32_t izb;
  float fcf = fc;
  float x, w;
  fc        = fc / Fsamprate;
  dfc       = dfc / Fsamprate;

  // calculate Kaiser-Bessel window shape factor beta from stop-band attenuation
  if(Astop < 20.96) {
    Beta = 0.0;
  } else {
    if(Astop >= 50.0) {
      Beta = 0.1102 * (Astop - 8.71);
    } else {
      Beta = 0.5842 * powf((Astop - 20.96), 0.4) + 0.07886 * (Astop - 20.96);
    }
  }
  memset(coeffs_I, 0.0, numCoeffs * sizeof(float));    //zero entire buffer, important for variables from DMAMEM

  izb = Izero(Beta);
  if(type == 0) { // low pass filter
    fcf = fc * 2.0;
    nc  =  numCoeffs;
  } else if(type == 1) { // high-pass filter
    fcf = -fc;
    nc  =  2 * (numCoeffs / 2);
  } else if((type == 2) || (type == 3)) { // band-pass filter
    fcf = dfc;
    nc  =  2 * (numCoeffs / 2); // maybe not needed
  } else if(type == 4) { // Hilbert transform
    nc  =  2 * (numCoeffs / 2);
    // clear coefficients
    for(int ii = 0; ii < 2 * (nc - 1); ii++) {
      coeffs_I[ii] = 0;
    }
    coeffs_I[nc] = 1;                                   // set real delay
    for(int ii = 1; ii < (nc + 1); ii += 2) {          // set imaginary Hilbert coefficients
      if(2 * ii == nc)
        continue;
      x = (float)(2 * ii - nc) / (float)nc;
      w = Izero(Beta * sqrtf(1.0f - x * x)) / izb; // Kaiser window
      coeffs_I[2 * ii + 1] = 1.0f / (HALF_PI * (float)(ii - nc / 2)) * w ;
    }
    return;
  }

  for(int ii = - nc, jj = 0; ii < nc; ii += 2, jj++) {
    x = (float)ii / (float)nc;
    w = Izero(Beta * sqrtf(1.0f - x * x)) / izb; // Kaiser window
    coeffs_I[jj] = fcf * MSinc(ii, fcf) * w;
  }

  if(type == 1) {
    coeffs_I[nc / 2] += 1;
  } else if(type == 2) {
    for(int jj = 0; jj < nc + 1; jj++)
      coeffs_I[jj] *= 2.0f * cosf(HALF_PI * (2 * jj - nc) * fc);
  } else if(type == 3) {
    for(int jj = 0; jj < nc + 1; jj++)
      coeffs_I[jj] *= -2.0f * cosf(HALF_PI * (2 * jj - nc) * fc);
    coeffs_I[nc / 2] += 1;
  }

}

//////////////////////////////////////////////////////////////////////
//  Call to setup filter parameters
// sampleRate in Hz
// fLowcut is low cutoff frequency of filter in Hz
// fHicut is high cutoff frequency of filter in Hz
// Offset is the CW tone offset frequency
// cutoff frequencies range from -sampleRate/2 to +sampleRate/2
//  HiCut must be greater than LowCut
//    example to make 2700Hz USB filter:
//  SetupParameters( 100, 2800, 0, 48000);
//////////////////////////////////////////////////////////////////////

/*****
  Purpose: calc_cplx_FIR_coeffs

  Parameter list:
    float *coeffs_I
    float *coeffs_Q
    int numCoeffs
    float32_t fLoCut
    float32_t FHiCut
    float sampleRate

  Return value:
    void
*****/
void CalcCplxFIRCoeffs(float * coeffs_I, float * coeffs_Q, int numCoeffs, float32_t fLoCut, float32_t fHiCut, float sampleRate) {
  //calculate some normalized filter parameters
  float32_t nFL = fLoCut / sampleRate;
  float32_t nFH = fHiCut / sampleRate;
  float32_t nFc = (nFH - nFL) / 2.0; // prototype LP filter cutoff
  float32_t nFs = PI * (nFH + nFL); // 2 PI times required frequency shift (fHiCut+fLoCut)/2
  float32_t fCenter = 0.5 * (float32_t)(numCoeffs - 1); //floating point center index of FIR filter
  float32_t x;
  float32_t z;

  memset(coeffs_I, 0.0, numCoeffs * sizeof(float));    //zero entire buffer, important for variables from DMAMEM
  memset(coeffs_Q, 0.0, numCoeffs * sizeof(float));

  //create LP FIR windowed sinc, sin(x)/x complex LP filter coefficients
  for(int i = 0; i < numCoeffs; i++)  {
    x = (float32_t)i - fCenter;
    if( abs((float)i - fCenter) < 0.01) //deal with odd size filter singularity where sin(0)/0==1
      z = 2.0 * nFc;
    else
      switch(FIR_filter_window) {
        case 1:    // 4-term Blackman-Harris --> this is what Power SDR uses
          z = (float32_t)sinf(TWO_PI * x * nFc) / (PI * x) *
              (0.35875 - 0.48829 * cosf( (TWO_PI * i) / (numCoeffs - 1) )
               + 0.14128 * cosf( (FOURPI * i) / (numCoeffs - 1) )
               - 0.01168 * cosf( (SIXPI * i) / (numCoeffs - 1) ) );
          break;

        case 2: // sine
          z = (float32_t)sinf(TWO_PI * x * nFc) / (PI * x) *
              (0.355768 - 0.487396 * cosf( (TWO_PI * i) / (numCoeffs - 1) )
               + 0.144232 * cosf( (FOURPI * i) / (numCoeffs - 1) )
               - 0.012604 * cosf( (SIXPI * i) / (numCoeffs - 1) ) );
          break;

        case 3: // cosine
          z = (float32_t)sinf(TWO_PI * x * nFc) / (PI * x) *
              cosf((PI * (float32_t)i) / (numCoeffs - 1));
          break;

        case 4: // Hann
          z = (float32_t)sinf(TWO_PI * x * nFc) / (PI * x) *
              0.5 * (float32_t)(1.0 - (cosf(PI * 2 * (float32_t)i / (float32_t)(numCoeffs - 1))));
          break;
        default: // Blackman-Nuttall window
          z = (float32_t)sinf(TWO_PI * x * nFc) / (PI * x) *
              (0.3635819
               - 0.4891775 * cosf( (TWO_PI * i) / (numCoeffs - 1) )
               + 0.1365995 * cosf( (FOURPI * i) / (numCoeffs - 1) )
               - 0.0106411 * cosf( (SIXPI * i) / (numCoeffs - 1) ) );
          break;
      }
    //shift lowpass filter coefficients in frequency by (hicut+lowcut)/2 to form bandpass filter anywhere in range
    coeffs_I[i]   = z * cosf(nFs * x);
    coeffs_Q[i]   = z * sinf(nFs * x);
  }
}

/*****
  Purpose: SetDecIntFilters()

  Parameter list:
    void

  Return value:
    void
*****/
void SetDecIntFilters() {
  /****************************************************************************************
     Recalculate decimation and interpolation FIR filters
  ****************************************************************************************/
  int filter_BW_highest = bands[currentBand].FHiCut;
  int LP_F_help;

  if(filter_BW_highest < -bands[currentBand].FLoCut) {
    filter_BW_highest = -bands[currentBand].FLoCut;
  }
  LP_F_help = filter_BW_highest;

  if(LP_F_help > 10000) {
    LP_F_help = 10000;
  }

  CalcFIRCoeffs(FIR_dec1_coeffs, 27, (float32_t)LP_F_help, 90.0, 0, 0.0, 192000.0);
  CalcFIRCoeffs(FIR_dec2_coeffs, 33, (float32_t)LP_F_help, 90.0, 0, 0.0, 48000.0);

  CalcFIRCoeffs(FIR_int1_coeffs, 48, (float32_t)(LP_F_help), 90.0, 0, 0.0, 48000.0);
  CalcFIRCoeffs(FIR_int2_coeffs, 32, (float32_t)(LP_F_help), 90.0, 0, 0.0, 192000.0);
}

/*****
  Purpose: Set the decimate coefs for the specified BW

  Parameter list:
    int filter_BW - desired bandwidth

  Return value:
    void
*****/
void SetDecIntFilters(int filter_BW) {
  int LP_F_help = filter_BW;

  //if(LP_F_help > 10000) {
  //  LP_F_help = 10000;
  //}

  CalcFIRCoeffs(FIR_dec1_coeffs, 27, (float32_t)(LP_F_help), 90.0, 0, 0.0, 192000.0);
  CalcFIRCoeffs(FIR_dec2_coeffs, 33, (float32_t)(LP_F_help), 90.0, 0, 0.0, 48000.0);
}
