
#include "SDT.h"
#include "Display.h"
#include "FIR.h"
#include "t41Control.h"
#include "Tune.h"
#include "Utility.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

#define USE_LOG10FAST

int zoom_sample_ptr = 0;
float32_t DMAMEM FFT_buffer[1024] __attribute__((aligned(4)));
float32_t DMAMEM buffer_spec_FFT[1024] __attribute__((aligned(4)));
float32_t DMAMEM FFT_spec[1024];
float32_t DMAMEM FFT_data[512];
float32_t DMAMEM FFT_spec_old[1024];
float32_t DMAMEM iFFT_buffer[1024 + 1];

float32_t DMAMEM Fir_Zoom_FFT_Decimate_I_state[4 + 2048 - 1];
float32_t DMAMEM Fir_Zoom_FFT_Decimate_Q_state[4 + 2048 - 1];

arm_fir_decimate_instance_f32 Fir_Zoom_FFT_Decimate_I;
arm_fir_decimate_instance_f32 Fir_Zoom_FFT_Decimate_Q;

float32_t DMAMEM Fir_Zoom_FFT_Decimate_coeffs[4];

const uint32_t IIR_biquad_Zoom_FFT_N_stages = 4;

float32_t IIR_biquad_Zoom_FFT_I_state[IIR_biquad_Zoom_FFT_N_stages * 4];
float32_t IIR_biquad_Zoom_FFT_Q_state[IIR_biquad_Zoom_FFT_N_stages * 4];

arm_biquad_casd_df1_inst_f32 IIR_biquad_Zoom_FFT_I;
arm_biquad_casd_df1_inst_f32 IIR_biquad_Zoom_FFT_Q;

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

/*****
  Purpose: change IIR and decimation filters for altered frequency spectrum badwidth.
*****/
FLASHMEM void ZoomFFTPrep() {
  // take value of spectrumZoom and initialize IIR lowpass and FIR decimation filters for the right values

  float32_t Fstop_Zoom = 0.5 * 192000.0 / (1 << spectrumZoom);
  CalcFIRCoeffs(Fir_Zoom_FFT_Decimate_coeffs, 4, Fstop_Zoom, 60, 0, 0.0, 192000.0);

  if(spectrumZoom < 7) {
    Fir_Zoom_FFT_Decimate_I.M = (1 << spectrumZoom);
    Fir_Zoom_FFT_Decimate_Q.M = (1 << spectrumZoom);
    IIR_biquad_Zoom_FFT_I.pCoeffs = mag_coeffs[spectrumZoom];
    IIR_biquad_Zoom_FFT_Q.pCoeffs = mag_coeffs[spectrumZoom];
  } else { // we have to decimate by 128 for all higher magnifications, arm routine does not allow for higher decimations
    Fir_Zoom_FFT_Decimate_I.M = 128;
    Fir_Zoom_FFT_Decimate_Q.M = 128;
    IIR_biquad_Zoom_FFT_I.pCoeffs = mag_coeffs[7];
    IIR_biquad_Zoom_FFT_Q.pCoeffs = mag_coeffs[7];
  }

  zoom_sample_ptr = 0;
}

FLASHMEM void InitFFTFilter() {
  float32_t Fstop_Zoom = 0.5 * 192000.0 / (1 << spectrumZoom);

  CalcFIRCoeffs(Fir_Zoom_FFT_Decimate_coeffs, 4, Fstop_Zoom, 60, 0, 0.0, 192000.0);
  arm_fir_decimate_init_f32(&Fir_Zoom_FFT_Decimate_I, 4, 128, Fir_Zoom_FFT_Decimate_coeffs, Fir_Zoom_FFT_Decimate_I_state, 2048);
  arm_fir_decimate_init_f32(&Fir_Zoom_FFT_Decimate_Q, 4, 128, Fir_Zoom_FFT_Decimate_coeffs, Fir_Zoom_FFT_Decimate_Q_state, 2048);

  IIR_biquad_Zoom_FFT_I.numStages = IIR_biquad_Zoom_FFT_N_stages;  // set number of stages
  IIR_biquad_Zoom_FFT_Q.numStages = IIR_biquad_Zoom_FFT_N_stages;  // set number of stages
  for(unsigned i = 0; i < 4 * IIR_biquad_Zoom_FFT_N_stages; i++) {
    IIR_biquad_Zoom_FFT_I_state[i] = 0.0;  // set state variables to zero
    IIR_biquad_Zoom_FFT_Q_state[i] = 0.0;  // set state variables to zero
  }
  IIR_biquad_Zoom_FFT_I.pState = IIR_biquad_Zoom_FFT_I_state;  // set pointer to the state variables
  IIR_biquad_Zoom_FFT_Q.pState = IIR_biquad_Zoom_FFT_Q_state;  // set pointer to the state variables

  // this sets the coefficients for the ZoomFFT decimation filter
  // according to the desired magnification mode
  // for 0 the mag_coeffs will a NULL  ptr, since the filter is not going to be used in this  mode!
  IIR_biquad_Zoom_FFT_I.pCoeffs = mag_coeffs[spectrumZoom];
  IIR_biquad_Zoom_FFT_Q.pCoeffs = mag_coeffs[spectrumZoom];

  ZoomFFTPrep();
}

/*****
  Purpose: Display FFT routine
           Intended for spectrumZoom > 1
*****/
void ZoomFFTExe(uint32_t blockSize) {
  float32_t LPFcoeff;
  float32_t onem_LPFcoeff;
  float32_t x_buffer[blockSize]; // can be 4096 [FFT length == 1024] or even 8192 [FFT length == 2048]
  float32_t y_buffer[blockSize];
  static float32_t FFT_ring_buffer_x[SPECTRUM_RES*2];
  static float32_t FFT_ring_buffer_y[SPECTRUM_RES*2];
  int sample_no = SPECTRUM_RES; // sample_no is 256, in high magnify modes it is smaller but it must never be > SPECTRUM_RES
  float32_t multiplier;
  const arm_cfft_instance_f32* spec_FFT = &arm_cfft_sR_f32_len512;

  sample_no = 2048 / (1 << spectrumZoom);
  if(sample_no > SPECTRUM_RES) {
    sample_no = SPECTRUM_RES;
  }

  arm_biquad_cascade_df1_f32(&IIR_biquad_Zoom_FFT_I, float_buffer_L, x_buffer, blockSize);
  arm_biquad_cascade_df1_f32(&IIR_biquad_Zoom_FFT_Q, float_buffer_R, y_buffer, blockSize);

  // decimation
  arm_fir_decimate_f32(&Fir_Zoom_FFT_Decimate_I, x_buffer, x_buffer, blockSize);
  arm_fir_decimate_f32(&Fir_Zoom_FFT_Decimate_Q, y_buffer, y_buffer, blockSize);

  // This puts the sample_no samples into the ringbuffer -->
  // the right order has to be thought about!
  // we take all the samples from zoom_sample_ptr to 256 and
  // then all samples from 0 to zoom_sampl_ptr - 1
  // fill into ringbuffer

  // interleave real and imaginary input values [real, imag, real, imag . . .]
  for(int i = 0; i < sample_no; i++) {
    FFT_ring_buffer_x[zoom_sample_ptr] = x_buffer[i];
    FFT_ring_buffer_y[zoom_sample_ptr] = y_buffer[i];
    zoom_sample_ptr++;
    if(zoom_sample_ptr >= SPECTRUM_RES)
      zoom_sample_ptr = 0;
  }

  multiplier = (float32_t)spectrumZoom;
  if(spectrumZoom > 3) { // SPECTRUM_ZOOM_8
    multiplier = (float32_t)(1 << spectrumZoom);
  }
  for(int idx = 0; idx < SPECTRUM_RES; idx++) {
    buffer_spec_FFT[idx * 2 + 0] =  multiplier * FFT_ring_buffer_x[zoom_sample_ptr] * (0.5 - 0.5 * cos(6.28 * idx / SPECTRUM_RES)); //Hanning Window AFP 03-12-21
    buffer_spec_FFT[idx * 2 + 1] =  multiplier * FFT_ring_buffer_y[zoom_sample_ptr] * (0.5 - 0.5 * cos(6.28 * idx / SPECTRUM_RES));
    zoom_sample_ptr++;
    if(zoom_sample_ptr >= SPECTRUM_RES) {
      zoom_sample_ptr = 0;
    }
  }
  //***************
  // adjust lowpass filter coefficient, so that
  // "spectrum display smoothness" is the same across the different sample rates
  // and the same across different magnify modes . . .
  LPFcoeff = 0.7;

  //if(LPFcoeff > 1.0) {
  //  LPFcoeff = 1.0;
  //}
  //if(LPFcoeff < 0.001) {
  //  LPFcoeff = 0.001;
  //}

  onem_LPFcoeff = 1.0 - LPFcoeff;

  // perform complex FFT
  // calculation is performed in-place the FFT_buffer [re, im, re, im, re, im . . .]
  arm_cfft_f32(spec_FFT, buffer_spec_FFT, 0, 1);

  for(int i = 0; i < SPECTRUM_RES / 2; i++) {
    FFT_spec[i + SPECTRUM_RES / 2] = (buffer_spec_FFT[i * 2] * buffer_spec_FFT[i * 2] + buffer_spec_FFT[i * 2 + 1] * buffer_spec_FFT[i * 2 + 1]); // Last half of spectrum
    FFT_spec[i] = (buffer_spec_FFT[(i + SPECTRUM_RES / 2) * 2] * buffer_spec_FFT[(i + SPECTRUM_RES / 2)  * 2] + buffer_spec_FFT[(i + SPECTRUM_RES / 2)  * 2 + 1] * buffer_spec_FFT[(i + SPECTRUM_RES / 2)  * 2 + 1]);
  }

  // apply low pass filter and scale the magnitude values and convert to int for spectrum display
  // apply spectrum AGC
  int16_t min = 0;
  int16_t max = 0;
  int16_t data[SPECTRUM_RES];
  for(int i = 0; i < SPECTRUM_RES; i++) {
    FFT_spec[i] = LPFcoeff * FFT_spec[i] + onem_LPFcoeff * FFT_spec_old[i];
    FFT_spec_old[i] = FFT_spec[i];

    pixelnew[i] = displayScale[currentScale].baseOffset + bands[currentBand].pixel_offset + (int16_t)(displayScale[currentScale].dBScale * log10f_fast(FFT_spec[i]));

    if(controlDataFlag) {
      // T41 spectrum equation: spectrumNoiseFloor - pixelnew[i] - currentNF;
      //data[i] = spectrumNoiseFloor - pixelnew[i] - currentNF;
      data[i] = pixelnew[i] + nf2PC;
      if(data[i] < min) {
        min = data[i];
      }
      if(data[i] > max) {
        max = data[i];
      }
    }
  }

  // set up specData for frequency spectrum command
  // FDxxx[512]; where xxx = 255 - max and [512] = 512 bytes spectrum data
  sprintf((char*)specData, "FD%03d", 255 - max);
  specData[517] = ';';

  // shift spectrum data and send it to PC if applicable
  // we have to scale and apply noise floor in the control app
  if(controlDataFlag) {
    int tmp = 0;
    for(int i = 0; i < SPECTRUM_RES; i++) {
      // shift data so max = 255
      // *** TODO: consider scaling here fits data into a 0-255 range ***
      tmp = data[i] + 255 - max;
      // though unlikely, data can still be negative, limit it
      if(tmp < 0) {
        tmp = 0;
      }
      //if(tmp > 255) {
      //  tmp = 255;
      //}
      specData[i + 5] = (uint8_t)tmp;
    }

    T41ControlSendData(specData, SPECTRUM_RES + 6);
  }
  //if(connected) {
  //  int tmp = 0;
  //  for(int i = 0; i < SPECTRUM_RES; i++) {
  //    // shift data so max = 255
  //    // *** TODO: consider scaling here fits data into a 0-255 range ***
  //    tmp = spectrumNoiseFloor - pixelnew[i] - currentNF;
  //    // though unlikely, data can still be negative, limit it
  //    if(tmp < 0) {
  //      tmp = SPECTRUM_BOTTOM;
  //    }
  //    if(tmp > 255) {
  //      tmp = SPECTRUM_BOTTOM;
  //    }
  //    freqData[i] = tmp;
  //  }
  //}
}

/*****
  Purpose: Calcculate zoom magnification when Spectrum Zoom = 1
*****/
void CalcZoom1Magn() {
  const arm_cfft_instance_f32* spec_FFT = &arm_cfft_sR_f32_len512;

  float32_t spec_help = 0.0;
  float32_t LPFcoeff = 0.7;
  if(LPFcoeff > 1.0) {
    LPFcoeff = 1.0;
  }

  for(int i = 0; i < SPECTRUM_RES; i++) { // interleave real and imaginary input values [real, imag, real, imag . . .]
    buffer_spec_FFT[i * 2] =      float_buffer_L[i] * (0.5 - 0.5 * cos(6.28 * i / SPECTRUM_RES)); //Hanning
    buffer_spec_FFT[i * 2 + 1] =  float_buffer_R[i] * (0.5 - 0.5 * cos(6.28 * i / SPECTRUM_RES));
  }
  // perform complex FFT
  // calculation is performed in-place the FFT_buffer [re, im, re, im, re, im . . .]
  arm_cfft_f32(spec_FFT, buffer_spec_FFT, 0, 1);

  // calculate magnitudes and put into FFT_spec
  // we do not need to calculate magnitudes with square roots, it would seem to be sufficient to
  // calculate mag = I*I + Q*Q, because we are doing a log10-transformation later anyway
  // and simultaneously put them into the right order
  // 38.50%, saves 0.05% of processor power and 1kbyte RAM ;-)

  for(int i = 0; i < SPECTRUM_RES/2; i++) {
    FFT_spec[i + SPECTRUM_RES/2] = (buffer_spec_FFT[i * 2] * buffer_spec_FFT[i * 2] + buffer_spec_FFT[i * 2 + 1] * buffer_spec_FFT[i * 2 + 1]);
    FFT_spec[i]                  = (buffer_spec_FFT[(i + SPECTRUM_RES/2) * 2] * buffer_spec_FFT[(i + SPECTRUM_RES/2)  * 2] + buffer_spec_FFT[(i + SPECTRUM_RES/2)  * 2 + 1] * buffer_spec_FFT[(i + SPECTRUM_RES/2)  * 2 + 1]);
  }
  // apply low pass filter and scale the magnitude values and convert to int for spectrum display

  for(int16_t x = 0; x < SPECTRUM_RES; x++) {
    spec_help = LPFcoeff * FFT_spec[x] + (1.0 - LPFcoeff) * FFT_spec_old[x];
    FFT_spec_old[x] = spec_help;

#ifdef USE_LOG10FAST
    pixelnew[x] = displayScale[currentScale].baseOffset + bands[currentBand].pixel_offset + (int16_t) (displayScale[currentScale].dBScale * log10f_fast(FFT_spec[x]));
#else
    pixelnew[x] = displayScale[currentScale].baseOffset + bands[currentBand].pixel_offset + (int16_t) (displayScale[currentScale].dBScale * log10f(spec_help));
#endif
  }
}
