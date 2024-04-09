#include "SDT.h"
#include "Display.h"
#include "FIR.h"
#include "Utility.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

#define USE_LOG10FAST

int zoom_sample_ptr = 0;
float32_t DMAMEM FFT_buffer[FFT_LENGTH * 2] __attribute__((aligned(4)));
float32_t DMAMEM buffer_spec_FFT[1024] __attribute__((aligned(4)));
float32_t DMAMEM FFT_spec[1024];
float32_t DMAMEM FFT_spec_old[1024];

float32_t DMAMEM Fir_Zoom_FFT_Decimate_coeffs[4];

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

/*****
  Purpose: change IIR and decimation filters for altered frequency spectrum badwidth.
  
  Parameter list:
    void
    
  Return value;
    void
*****/
void ZoomFFTPrep() {
  // take value of spectrum_zoom and initialize IIR lowpass and FIR decimation filters for the right values

  float32_t Fstop_Zoom = 0.5 * (float32_t) SampleRate / (1 << spectrum_zoom);
  CalcFIRCoeffs(Fir_Zoom_FFT_Decimate_coeffs, 4, Fstop_Zoom, 60, 0, 0.0, (float32_t)SampleRate);

  if (spectrum_zoom < 7)
  {
    Fir_Zoom_FFT_Decimate_I.M = (1 << spectrum_zoom);
    Fir_Zoom_FFT_Decimate_Q.M = (1 << spectrum_zoom);
    IIR_biquad_Zoom_FFT_I.pCoeffs = mag_coeffs[spectrum_zoom];
    IIR_biquad_Zoom_FFT_Q.pCoeffs = mag_coeffs[spectrum_zoom];
  } else { // we have to decimate by 128 for all higher magnifications, arm routine does not allow for higher decimations
    Fir_Zoom_FFT_Decimate_I.M = 128;
    Fir_Zoom_FFT_Decimate_Q.M = 128;
    IIR_biquad_Zoom_FFT_I.pCoeffs = mag_coeffs[7];
    IIR_biquad_Zoom_FFT_Q.pCoeffs = mag_coeffs[7];
  }

  zoom_sample_ptr = 0;
}

/*****
  Purpose: Display FFT routine
           Should only be called when spectrum_zoom > 1 and updateDisplayFlag == 1
  
  Parameter list:
    void

  Return value;
    void
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


  sample_no = BUFFER_SIZE * N_BLOCKS / (1 << spectrum_zoom);
  if (sample_no > SPECTRUM_RES) {
    sample_no = SPECTRUM_RES;
  }

  arm_biquad_cascade_df1_f32 (&IIR_biquad_Zoom_FFT_I, float_buffer_L, x_buffer, blockSize);
  arm_biquad_cascade_df1_f32 (&IIR_biquad_Zoom_FFT_Q, float_buffer_R, y_buffer, blockSize);

  // decimation
  arm_fir_decimate_f32(&Fir_Zoom_FFT_Decimate_I, x_buffer, x_buffer, blockSize);
  arm_fir_decimate_f32(&Fir_Zoom_FFT_Decimate_Q, y_buffer, y_buffer, blockSize);

  // This puts the sample_no samples into the ringbuffer -->
  // the right order has to be thought about!
  // we take all the samples from zoom_sample_ptr to 256 and
  // then all samples from 0 to zoom_sampl_ptr - 1
  // fill into ringbuffer
  
  // interleave real and imaginary input values [real, imag, real, imag . . .]
  for (int i = 0; i < sample_no; i++) { 
    FFT_ring_buffer_x[zoom_sample_ptr] = x_buffer[i];
    FFT_ring_buffer_y[zoom_sample_ptr] = y_buffer[i];
    zoom_sample_ptr++;
    if (zoom_sample_ptr >= SPECTRUM_RES)
      zoom_sample_ptr = 0;
  }
  
  multiplier = (float32_t)spectrum_zoom;
  if (spectrum_zoom > 3) { // SPECTRUM_ZOOM_8
    multiplier = (float32_t)(1 << spectrum_zoom);
  }
  for (int idx = 0; idx < SPECTRUM_RES; idx++) {
    buffer_spec_FFT[idx * 2 + 0] =  multiplier * FFT_ring_buffer_x[zoom_sample_ptr] * (0.5 - 0.5 * cos(6.28 * idx / SPECTRUM_RES)); //Hanning Window AFP 03-12-21
    buffer_spec_FFT[idx * 2 + 1] =  multiplier * FFT_ring_buffer_y[zoom_sample_ptr] * (0.5 - 0.5 * cos(6.28 * idx / SPECTRUM_RES));
    zoom_sample_ptr++;
    if (zoom_sample_ptr >= SPECTRUM_RES) {
      zoom_sample_ptr = 0;
    }
  }
  //***************
  // adjust lowpass filter coefficient, so that
  // "spectrum display smoothness" is the same across the different sample rates
  // and the same across different magnify modes . . .
  LPFcoeff = 0.7;
  
  //if (LPFcoeff > 1.0) {
  //  LPFcoeff = 1.0;
  //}
  //if (LPFcoeff < 0.001) {
  //  LPFcoeff = 0.001;
  //}

  onem_LPFcoeff = 1.0 - LPFcoeff;

  // perform complex FFT
  // calculation is performed in-place the FFT_buffer [re, im, re, im, re, im . . .]
  arm_cfft_f32(spec_FFT, buffer_spec_FFT, 0, 1);

  for (int i = 0; i < SPECTRUM_RES / 2; i++) {
    FFT_spec[i + SPECTRUM_RES / 2] = (buffer_spec_FFT[i * 2] * buffer_spec_FFT[i * 2] + buffer_spec_FFT[i * 2 + 1] * buffer_spec_FFT[i * 2 + 1]); // Last half of spectrum
    FFT_spec[i] = (buffer_spec_FFT[(i + SPECTRUM_RES / 2) * 2] * buffer_spec_FFT[(i + SPECTRUM_RES / 2)  * 2] + buffer_spec_FFT[(i + SPECTRUM_RES / 2)  * 2 + 1] * buffer_spec_FFT[(i + SPECTRUM_RES / 2)  * 2 + 1]);
  }

  // apply low pass filter and scale the magnitude values and convert to int for spectrum display
  // apply spectrum AGC
  for (int i = 0; i < SPECTRUM_RES; i++) {
    // save old pixels for lowpass filter This is also used to erase the old spectrum
    //pixelold[i] = pixelnew[i];
    pixelold[i] = pixelCurrent[i];

    FFT_spec[i] = LPFcoeff * FFT_spec[i] + onem_LPFcoeff * FFT_spec_old[i];
    FFT_spec_old[i] = FFT_spec[i];

    pixelnew[i] = displayScale[currentScale].baseOffset + bands[currentBand].pixel_offset + (int16_t)(displayScale[currentScale].dBScale * log10f_fast(FFT_spec[i]));
    if (pixelnew[i] > 220) {
      pixelnew[i] = 220;
    }
  }
}

/*****
  Purpose: CalcZoom1Magn()
  Parameter list:
    void
  Return value;
    void
    Used when Spectrum Zoom =1
*****/
void CalcZoom1Magn() {
 if (updateDisplayFlag == 1) {
  float32_t spec_help = 0.0;
  float32_t LPFcoeff = 0.7;
  if (LPFcoeff > 1.0) {
    LPFcoeff = 1.0;
  }
  for (int i = 0; i < SPECTRUM_RES; i++) {
    pixelold[i] = pixelnew[i];
  }


  for (int i = 0; i < SPECTRUM_RES; i++) { // interleave real and imaginary input values [real, imag, real, imag . . .]
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

  for (int i = 0; i < SPECTRUM_RES/2; i++) {
      FFT_spec[i + SPECTRUM_RES/2] = (buffer_spec_FFT[i * 2] * buffer_spec_FFT[i * 2] + buffer_spec_FFT[i * 2 + 1] * buffer_spec_FFT[i * 2 + 1]);
      FFT_spec[i]                  = (buffer_spec_FFT[(i + SPECTRUM_RES/2) * 2] * buffer_spec_FFT[(i + SPECTRUM_RES/2)  * 2] + buffer_spec_FFT[(i + SPECTRUM_RES/2)  * 2 + 1] * buffer_spec_FFT[(i + SPECTRUM_RES/2)  * 2 + 1]);
    }
  // apply low pass filter and scale the magnitude values and convert to int for spectrum display

  for (int16_t x = 0; x < SPECTRUM_RES; x++) {
    spec_help = LPFcoeff * FFT_spec[x] + (1.0 - LPFcoeff) * FFT_spec_old[x];
    FFT_spec_old[x] = spec_help;

#ifdef USE_LOG10FAST
    pixelnew[x] = displayScale[currentScale].baseOffset + bands[currentBand].pixel_offset + (int16_t) (displayScale[currentScale].dBScale * log10f_fast(FFT_spec[x]));
#else
    pixelnew[x] = displayScale[currentScale].baseOffset + bands[currentBand].pixel_offset + (int16_t) (displayScale[currentScale].dBScale * log10f(spec_help));
#endif
  }
 }
} // end calc_256_magn
