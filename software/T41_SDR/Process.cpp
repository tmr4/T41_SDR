#include "SDT.h"
#include "ButtonProc.h"
#include "CW_Excite.h"
#include "CWProcessing.h"
#include "Demod.h"
#include "Display.h"
#include "DSP_Fn.h"
#include "Encoders.h"
#include "FFT.h"
#include "Filter.h"
#include "FIR.h"
#include "Freq_Shift.h"
#include "ft8.h"
#include "Menu.h"
#include "MenuProc.h"
#include "Noise.h"
#include "Process.h"
#include "Tune.h"
#include "Utility.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

int ft8Loop = 0;

float32_t audioMaxSquaredAve = 0;

//const uint32_t N_B = FFT_LENGTH / 2 / BUFFER_SIZE * (uint32_t)DF;  // 512/2/128 * 8 = 16
//const uint32_t N_B = 16;

int audioYPixel[1024];
float32_t audioSpectBuffer[1024]; // This can't be DMAMEM.  It will break the S-Meter.
float32_t DMAMEM float_buffer_L_AudioCW[256];
//const uint32_t N_DEC_B = N_B / (uint32_t)DF;
//float32_t DMAMEM last_sample_buffer_L[BUFFER_SIZE * N_DEC_B];
//float32_t DMAMEM last_sample_buffer_R[BUFFER_SIZE * N_DEC_B];

uint8_t NB_on = 0; // noise blanker: 0 - off, 1 - on
int16_t *sp_L1;
int16_t *sp_R1;

char atom, currentAtom;
float32_t HP_DC_Butter_state2[2] = { 0, 0 };
arm_biquad_cascade_df2T_instance_f32 s1_Receive2 = { 1, HP_DC_Butter_state2, HP_DC_Filter_Coeffs2 };
uint8_t ANR_notch = 0;
uint8_t ANR_notchOn = 0;

int8_t first_block = 1;
int mute = 0; // 0 - normal volume, 1 - mute (*** this is never changed ***)

float VolumeToAmplification(int volume);

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

/*****
  Purpose: Read audio from Teensy Audio Library
             Calculate FFT for display
             Process audio into SSB signal
             Output audio to amplifier

   Parameter List:
      void

   Return value:
      void

   CAUTION: Assumes a spaces[] array is defined
 *****/
void ProcessIQData() {
  static float32_t audiotmp = 0.0f;
  float32_t w;
  static float32_t wold = 0.0f;
  static int leftover = 0;
  q15_t q15_buffer_LTemp[2048];
  float32_t audioMaxSquared;
  uint32_t AudioMaxIndex;
  static uint32_t result = 0;

  if (keyPressedOn == 1) {
    return;
  }

  if(bands[currentBand].mode == DEMOD_FT8_WAV) {
  // are there at least N_BLOCKS buffers in each channel available ?
  if ( (uint32_t) Q_in_L.available() > N_BLOCKS + 0 && (uint32_t) Q_in_R.available() > N_BLOCKS + 0 ) {
    usec = 0;
    // get samples from wave file
    // let's pull the data at the same rate as the T41 pulls from the I/Q stream
    // 2048 samples / 8 = 256
    // Or we can look to fill the audio out buffer at the same rate, which is 2048 samples per loop

    // process wave file data
    // get next chunk of wave file
    //readWave(float_buffer_L, 1920);
    // wav file sample rate is 12 kHz, T41 audio is 24 kHz
    // get a half sample size that we'll interpolate to the proper rate
    if(readWave(float_buffer_R, FFT_length / 2 / 2)) {

    // prepare audio stream (mostly just to verify proper wav file transfer)
    // interpolate by 2 to 24 kHz
    float_buffer_L[0] = float_buffer_R[0];
    for (unsigned i = 1; i < FFT_length / 2; i++) {
      float_buffer_L[2*i-1] = (float_buffer_R[i-1] + float_buffer_R[i]) / 2;
      float_buffer_L[2*i] = float_buffer_R[i];
    }
    
    // convert floats to the q15 required by FT8 routines
    arm_float_to_q15(float_buffer_R, q15_buffer_LTemp, FFT_length / 2 / 2);

    // decimate by 1.875
    for (unsigned i = 0; i < 68; i++) {
      // roll ft8 dsp buffer
      ft8_dsp_buffer[i + ft8Loop * 68] = ft8_dsp_buffer[i + 1024 + ft8Loop * 68];
      ft8_dsp_buffer[1024 + i + ft8Loop * 68] = ft8_dsp_buffer[i + 2048 + ft8Loop * 68];

      // transfer audio data to ft8_dsp_buffer
      // decimate by 1.875 which brings us to a 6.4 ksps rate
      ft8_dsp_buffer[2048 + i + ft8Loop * 68] = q15_buffer_LTemp[(int)(((float)i) * 120.0 / 64.0)]; // i * 1.875
    }

    if(++ft8Loop == 15) {
      ft8Loop = 0;
      process_FT8_FFT();
      if(ft8_decode_flag == 1) {
        num_decoded_msg = ft8_decode();
        if(num_decoded_msg > 0) {
          display_details(num_decoded_msg, kMax_decoded_messages);
        }
        ft8_decode_flag = 0;
        FT_8_counter = 0;
      }
    }
    
      // Prepare the audio signal buffers
      for (unsigned i = 0; i < BUFFER_SIZE * N_BLOCKS / (uint32_t)(DF); i++) {
        // fill first half of buffer with last sample
        FFT_buffer[i * 2] = last_sample_buffer_L[i]; // real
        FFT_buffer[i * 2 + 1] = 0; // there is no imaginary component

        // copy recent sample to last_sample_buffer for next time
        last_sample_buffer_L [i] = float_buffer_L[i];

        // fill recent audio samples into FFT_buffer
        FFT_buffer[FFT_length + i * 2] = float_buffer_L[i]; // real
        FFT_buffer[FFT_length + i * 2 + 1] = 0; // there is no imaginary component
      }

      /**********************************************************************************
        Perform complex FFT on the audio time signals
        calculation is performed in-place the FFT_buffer [re, im, re, im, re, im . . .]
      **********************************************************************************/

      // prepare audio box spectrum and filter per the audio cutoff frequency
      arm_cfft_f32(S, FFT_buffer, 0, 1);
      arm_cmplx_mult_cmplx_f32 (FFT_buffer, FIR_filter_mask, iFFT_buffer, FFT_length);

      // process audio frequency spectrum only at the beginning of the show spectrum process
      if (updateDisplayFlag == 1) {
        for (int k = 0; k < 1024; k++) {
          audioSpectBuffer[1023 - k] = (iFFT_buffer[k] * iFFT_buffer[k]);
        }
        //for (int k = 0; k < 256; k++) {
        for (int k = 0; k < AUDIO_SPEC_BOX_W - 2; k++) {
          // a spectrum offset of 20 give about the same magnitude signal peak as seen in the AM modes
          audioYPixel[k] = 20 +  map(15 * log10f((audioSpectBuffer[1021 - k] + audioSpectBuffer[1022 - k] + audioSpectBuffer[1023 - k]) / 3), 0, 100, 0, 120);
          if (audioYPixel[k] < 0) {
            audioYPixel[k] = 0;
          }
        }
        arm_max_f32 (audioSpectBuffer, 1024, &audioMaxSquared, &AudioMaxIndex);  // Max value of squared bin magnitued in audio
        audioMaxSquaredAve = .5 * audioMaxSquared + .5 * audioMaxSquaredAve;  // Running averaged values
      }

    // ======================================Interpolation  ================
    // interpolation-by-2
    arm_fir_interpolate_f32(&FIR_int1_I, float_buffer_L, iFFT_buffer, BUFFER_SIZE * N_BLOCKS / (uint32_t)(DF));

    // interpolation-by-4
    arm_fir_interpolate_f32(&FIR_int2_I, iFFT_buffer, float_buffer_L, BUFFER_SIZE * N_BLOCKS / (uint32_t)(DF1));

    /**********************************************************************************
      Digital Volume Control
    **********************************************************************************/
    if (mute == 1) {
      arm_scale_f32(float_buffer_L, 0.0, float_buffer_L, BUFFER_SIZE * N_BLOCKS);
    } else {
      if (mute == 0) {
        arm_scale_f32(float_buffer_L, DF * VolumeToAmplification(audioVolume), float_buffer_L, BUFFER_SIZE * N_BLOCKS);
      }
    }

    /**********************************************************************************
      CONVERT TO INTEGER AND PLAY AUDIO
    **********************************************************************************/
    arm_float_to_q15 (float_buffer_L, q15_buffer_LTemp, 2048);

    result = Q_out_L.play(q15_buffer_LTemp, 2048);
    ft8Delay = result / 10;
    Codec_gain();
    }
    else {
      ButtonDemodMode(); // switch back to ft8 mode
      /*
      // keep decoding w/o new audio
      for (unsigned i = 0; i < 68; i++) {
        // roll ft8 dsp buffer
        ft8_dsp_buffer[i + ft8Loop * 68] = ft8_dsp_buffer[i + 1024 + ft8Loop * 68];
        ft8_dsp_buffer[1024 + i + ft8Loop * 68] = ft8_dsp_buffer[i + 2048 + ft8Loop * 68];

        ft8_dsp_buffer[2048 + i + ft8Loop * 68] = 0;
      }

      if(++ft8Loop == 15) {
        //Serial.println(ft8Loop);
        ft8Loop = 0;
        process_FT8_FFT();
        if(ft8_decode_flag == 1) {
          num_decoded_msg = ft8_decode();
          if(num_decoded_msg > 0) {
            display_details(num_decoded_msg, kMax_decoded_messages);
          }
          ft8_decode_flag = 0;
          FT_8_counter = 0;
        }
      }
      */
    }
    elapsed_micros_sum = elapsed_micros_sum + usec;
    elapsed_micros_idx_t++;

    //delay(10); // need some delay to slow down processing
    Q_in_L.clear();
    Q_in_R.clear();

  }

  } else {

  /**********************************************************************************
        Get samples from queue buffers
        Teensy Audio Library stores ADC data in two buffers, Q_in_L and Q_in_R as initiated from the audio lib.
        Then the buffers are read into two arrays sp_L and sp_R in blocks of 128 up to N_BLOCKS.  The arrarys are
        of size BUFFER_SIZE * N_BLOCKS.  BUFFER_SIZE is 128.
        N_BLOCKS = FFT_LENGTH / 2 / BUFFER_SIZE * (uint32_t)DF; // should be 16 with DF == 8 and FFT_LENGTH = 512
        BUFFER_SIZE*N_BLOCKS = 2048 samples
     **********************************************************************************/
  float rfGainValue;

  // are there at least N_BLOCKS buffers in each channel available ?
  if ( (uint32_t) Q_in_L.available() > N_BLOCKS + 0 && (uint32_t) Q_in_R.available() > N_BLOCKS + 0 ) {
    usec = 0;

    // get audio samples from the audio  buffers and convert them to float
    // read in 32 blocks á 128 samples in I and Q
    for (unsigned i = 0; i < N_BLOCKS; i++) {
      sp_L1 = Q_in_R.readBuffer();
      sp_R1 = Q_in_L.readBuffer();

      /**********************************************************************************
          Using arm_Math library, convert to float one buffer_size.
          Float_buffer samples are now standardized from > -1.0 to < 1.0
      **********************************************************************************/
      arm_q15_to_float (sp_L1, &float_buffer_L[BUFFER_SIZE * i], BUFFER_SIZE); // convert int_buffer to float 32bit
      arm_q15_to_float (sp_R1, &float_buffer_R[BUFFER_SIZE * i], BUFFER_SIZE); // convert int_buffer to float 32bit
      Q_in_L.freeBuffer();
      Q_in_R.freeBuffer();
    }

    if (keyPressedOn == 1) {
      return;
    }

    /*******************************
            Set RFGain - for all bands
    */
    rfGainValue = pow(10, (float)rfGainAllBands / 20);
    arm_scale_f32 (float_buffer_L, rfGainValue, float_buffer_L, BUFFER_SIZE * N_BLOCKS);
    arm_scale_f32 (float_buffer_R, rfGainValue, float_buffer_R, BUFFER_SIZE * N_BLOCKS);
    /**********************************************************************************
        Remove DC offset to reduce centeral spike.  First read the Mean value of
        left and right channels.  Then fill L and R correction arrays with those Means
        and subtract the Means from the float L and R buffer data arrays.  Again use Arm_Math functions
        to manipulate the arrays.  Arrays are all BUFFER_SIZE * N_BLOCKS long
    **********************************************************************************/
    //
    //================

    arm_biquad_cascade_df2T_f32(&s1_Receive2, float_buffer_L, float_buffer_L, 2048);
    arm_biquad_cascade_df2T_f32(&s1_Receive2, float_buffer_R, float_buffer_R, 2048);
    
    //===========================

    /********************************************************************************** 
        Scale the data buffers by the RFgain value defined in bands[currentBand] structure
    **********************************************************************************/
    arm_scale_f32 (float_buffer_L, bands[currentBand].RFgain, float_buffer_L, BUFFER_SIZE * N_BLOCKS);
    arm_scale_f32 (float_buffer_R, bands[currentBand].RFgain, float_buffer_R, BUFFER_SIZE * N_BLOCKS);

    /**********************************************************************************
      Clear Buffers
      This is to prevent overfilled queue buffers during each switching event
      (band change, mode change, frequency change, the audio chain runs and fills the buffers
      if the buffers are full, the Teensy needs much more time
      in that case, we clear the buffers to keep the whole audio chain running smoothly
      **********************************************************************************/
    if (Q_in_L.available() > 25) {
      Q_in_L.clear();
      AudioInterrupts();
    }
    if (Q_in_R.available() > 25) {
      Q_in_R.clear();
      AudioInterrupts();
    }
    /**********************************************************************************
      IQ amplitude and phase correction.  For this scaled down version the I an Q channels are
      equalized and phase corrected manually. This is done by applying a correction, which is the difference, to
      the L channel only.  The phase is corrected in the IQPhaseCorrection() function.

      IQ amplitude and phase correction
    ***********************************************************************************************/

    // Manual IQ amplitude correction
    // to be honest: we only correct the amplitude of the I channel ;-)
    if (bands[currentBand].mode == DEMOD_LSB || bands[currentBand].mode == DEMOD_AM || bands[currentBand].mode == DEMOD_SAM) {
      arm_scale_f32 (float_buffer_L, -IQAmpCorrectionFactor[currentBand], float_buffer_L, BUFFER_SIZE * N_BLOCKS);
      IQPhaseCorrection(float_buffer_L, float_buffer_R, IQPhaseCorrectionFactor[currentBand], BUFFER_SIZE * N_BLOCKS);
    } else {
      if (bands[currentBand].mode == DEMOD_USB || bands[currentBand].mode == DEMOD_AM || bands[currentBand].mode == DEMOD_SAM) {
      //if (bands[currentBand].mode == DEMOD_USB || bands[currentBand].mode == DEMOD_FT8 || bands[currentBand].mode == DEMOD_AM || bands[currentBand].mode == DEMOD_SAM) {
        arm_scale_f32 (float_buffer_L, -IQAmpCorrectionFactor[currentBand], float_buffer_L, BUFFER_SIZE * N_BLOCKS);
        IQPhaseCorrection(float_buffer_L, float_buffer_R, IQPhaseCorrectionFactor[currentBand], BUFFER_SIZE * N_BLOCKS);
      }
    }
    // IQ phase correction

    /**********************************************************************************
        Perform a 256 point FFT for the spectrum display on the basis of the first 256 complex values
        of the raw IQ input data this saves about 3% of processor power compared to calculating
        the magnitudes and means of the 4096 point FFT for the display

        Only go there from here, if magnification == 1
    ***********************************************************************************************/

    if (spectrum_zoom == 0) { // && display_S_meter_or_spectrum_state == 1)
      CalcZoom1Magn();  // Moved to display function
    }

    //display_S_meter_or_spectrum_state++;
    if ( keyPressedOn == 1) {
      return;
    }

    /**********************************************************************************
        Frequency translation by Fs/4 without multiplication from Lyons (2011): chapter 13.1.2 page 646
        together with the savings of not having to shift/rotate the FFT_buffer, this saves
        about 1% of processor use

        This is for +Fs/4 [moves receive frequency to the left in the spectrum display]
          float_buffer_L contains I = real values
          float_buffer_R contains Q = imaginary values
          xnew(0) =  xreal(0) + jximag(0)
              leave first value (DC component) as it is!
          xnew(1) =  - ximag(1) + jxreal(1)
    **********************************************************************************/
    FreqShift1();

    /**********************************************************************************
        SPECTRUM_ZOOM_2 and larger here after frequency conversion!
        Spectrum zoom displays a magnified display of the data around the translated receive frequency.
        Processing is done in the ZoomFFTExe(BUFFER_SIZE * N_BLOCKS) function.  For magnifications of 2x to 8X
        Larger magnification are not needed in practice.

        Spectrum Zoom uses the shifted spectrum, so the center "hump" around DC is shifted by fs/4
    **********************************************************************************/
    // Run display FFT routine only once for each Audio process FFT
    if(spectrum_zoom != 0 && updateDisplayFlag == 1) {
      ZoomFFTExe(BUFFER_SIZE * N_BLOCKS); // there seems to be a BUG here, because the blocksize has to be adjusted according to magnification,
      // does not work for magnifications > 8
    }

    /**********************************************************************************  AFP 12-31-20
        S-Meter & dBm-display ?? not usually called
    **********************************************************************************/
    //============================== AFP 10-22-22  Begin new
    if (calibrateFlag == 1) {
      CalibrateOptions(IQChoice);
    }

    /*************************************************************************************************
        freq_conv2()

        FREQUENCY CONVERSION USING A SOFTWARE QUADRATURE OSCILLATOR
        Creates a new IF frequency to allow the tuning window to be moved anywhere in the current display.
        THIS VERSION calculates the COS AND SIN WAVE on the fly - uses double precision float

        MAJOR ADVANTAGE: frequency conversion can be done for any frequency !

        large parts of the code taken from the mcHF code by Clint, KA7OEI, thank you!
          see here for more info on quadrature oscillators:
        Wheatley, M. (2011): CuteSDR Technical Manual Ver. 1.01. - http://sourceforge.net/projects/cutesdr/
        Lyons, R.G. (2011): Understanding Digital Processing. – Pearson, 3rd edition.
    *************************************************************************************************/

    FreqShift2();

    /**********************************************************************************
        Decimation
        Resample (Decimate) the shifted time signal, first by 4, then by 2.  Each time the
        signal is decimated by an even number, the spectrum is reversed.  Resampling twice
        returns the spectrum to the correct orientation.
        Signal has now been shifted to base band, leaving aliases at higher frequencies,
        which are removed at each decimation step using the Arm combined decimate/filter function.
        If the starting sample rate is 192K SPS,   after the combined decimation, the sample rate is
        now 192K/8 = 24K SPS.  The array size is also reduced by 8, making FFT calculations much faster.
        The effective bandwidth (up to Nyquist frequency) is 12KHz.
    **********************************************************************************/

    if(bands[currentBand].mode == DEMOD_NFM) {
      // a NFM signal needs to be demodulated prior to audio processing so all we do here
      // is decimate and prepare the audio signal buffers

      // set NFM filter to decimate
      // fixed at 6 kHz for now
      // make variable (including separate visual BW on frequency spectrum and audio filter on audio spectrum)
      SetDecIntFilters(nfmFilterBW);

      // decimation-by-4 in-place!
      arm_fir_decimate_f32(&FIR_dec1_I, float_buffer_L, float_buffer_L, BUFFER_SIZE * N_BLOCKS);
      arm_fir_decimate_f32(&FIR_dec1_Q, float_buffer_R, float_buffer_R, BUFFER_SIZE * N_BLOCKS);

      // decimation-by-2 in-place
      arm_fir_decimate_f32(&FIR_dec2_I, float_buffer_L, float_buffer_L, BUFFER_SIZE * N_BLOCKS / (uint32_t)DF1);
      arm_fir_decimate_f32(&FIR_dec2_Q, float_buffer_R, float_buffer_R, BUFFER_SIZE * N_BLOCKS / (uint32_t)DF1);

      // Prepare the audio signal buffers
      // fill recent audio samples into FFT_buffer (left channel: re, right channel: im)
      // we'll use this to demodulate the NFM signal
      for (unsigned i = 0; i < BUFFER_SIZE * N_BLOCKS / (uint32_t)(DF); i++) {
        FFT_buffer[FFT_length + i * 2] = float_buffer_L[i]; // real
        FFT_buffer[FFT_length + i * 2 + 1] = float_buffer_R[i]; // imaginary
      }
    } else {
      // prepare signals for all other modes

      // decimation-by-4 in-place!
      arm_fir_decimate_f32(&FIR_dec1_I, float_buffer_L, float_buffer_L, BUFFER_SIZE * N_BLOCKS);
      arm_fir_decimate_f32(&FIR_dec1_Q, float_buffer_R, float_buffer_R, BUFFER_SIZE * N_BLOCKS);

      // decimation-by-2 in-place
      arm_fir_decimate_f32(&FIR_dec2_I, float_buffer_L, float_buffer_L, BUFFER_SIZE * N_BLOCKS / (uint32_t)DF1);
      arm_fir_decimate_f32(&FIR_dec2_Q, float_buffer_R, float_buffer_R, BUFFER_SIZE * N_BLOCKS / (uint32_t)DF1);

      // =================  Level Adjust ===========
      float freqKHzFcut;
      float volScaleFactor;
      if (bands[currentBand].mode == DEMOD_LSB) {
        freqKHzFcut = -(float32_t)bands[currentBand].FLoCut * 0.001;
      } else {
        freqKHzFcut = (float32_t)bands[currentBand].FHiCut * 0.001;
      }

      volScaleFactor = 7.0874 * pow(freqKHzFcut, -1.232);
      arm_scale_f32(float_buffer_L, volScaleFactor, float_buffer_L, FFT_length / 2);
      arm_scale_f32(float_buffer_R, volScaleFactor, float_buffer_R, FFT_length / 2);
      
      // Prepare the audio signal buffers
      //  First, Create Complex time signal for CFFT routine.
      //  Fill first block with Zeros
      //  Then interleave RE and IM parts to create signal for FFT
      if (first_block) { 
        // fill real & imaginaries with zeros for the first BLOCKSIZE samples
        // ONLY FOR the VERY FIRST FFT: fill first samples with zeros
        for (unsigned i = 0; i < BUFFER_SIZE * N_BLOCKS / (uint32_t)(DF / 2.0); i++) {
          FFT_buffer[i] = 0.0;
        }
        first_block = 0;
      } else {
        // All other FFTs
        // fill FFT_buffer with last events audio samples for all other FFT instances
        for (unsigned i = 0; i < BUFFER_SIZE * N_BLOCKS / (uint32_t)(DF); i++) {
          FFT_buffer[i * 2] = last_sample_buffer_L[i]; // real
          FFT_buffer[i * 2 + 1] = last_sample_buffer_R[i]; // imaginary
        }
      }

      for (unsigned i = 0; i < BUFFER_SIZE * N_BLOCKS / (uint32_t)(DF); i++) {
        // copy recent samples to last_sample_buffer for next time!
        last_sample_buffer_L [i] = float_buffer_L[i];
        last_sample_buffer_R [i] = float_buffer_R[i];

        // fill recent audio samples into FFT_buffer (left channel: re, right channel: im)
        FFT_buffer[FFT_length + i * 2] = float_buffer_L[i]; // real
        FFT_buffer[FFT_length + i * 2 + 1] = float_buffer_R[i]; // imaginary
      }

      /**********************************************************************************
          Digital FFT convolution
          Filtering is accomplished by combining (multiplying) spectra in the frequency domain.
          basis for this was Lyons, R. (2011): Understanding Digital Processing.
          "Fast FIR Filtering using the FFT", pages 688 - 694.
          Method used here: overlap-and-save.
      **********************************************************************************/
      /**********************************************************************************
        Perform complex FFT on the audio time signals
        calculation is performed in-place the FFT_buffer [re, im, re, im, re, im . . .]
      **********************************************************************************/
      arm_cfft_f32(S, FFT_buffer, 0, 1);

      /**********************************************************************************
        Continuing FFT Convolution
            Next, prepare the filter mask (done in the Filter.cpp file).  Only need to do this once for each filter setting.
            Allows efficient real-time variable LP and HP audio filters, without the overhead of time-domain convolution filtering.

            After the Filter mask in the frequency domain is created, complex multiply  filter mask with the frequency domain audio data.
            Filter mask previously calculated in setup Array of filter mask coefficients:
            FIR_filter_mask[]
      **********************************************************************************/

      arm_cmplx_mult_cmplx_f32 (FFT_buffer, FIR_filter_mask, iFFT_buffer, FFT_length);

      // process audio frequency spectrum only at the beginning of the show spectrum process
      if (updateDisplayFlag == 1) {
        for (int k = 0; k < 1024; k++) {
          audioSpectBuffer[1023 - k] = (iFFT_buffer[k] * iFFT_buffer[k]);
        }
        //for (int k = 0; k < 256; k++) {
        for (int k = 0; k < AUDIO_SPEC_BOX_W - 2; k++) {
          if (bands[currentBand].mode == DEMOD_USB || bands[currentBand].mode == DEMOD_FT8 || bands[currentBand].mode == DEMOD_AM || bands[currentBand].mode == DEMOD_SAM) {
            audioYPixel[k] = 50 +  map(15 * log10f((audioSpectBuffer[1021 - k] + audioSpectBuffer[1022 - k] + audioSpectBuffer[1023 - k]) / 3), 0, 100, 0, 120);
          }
          else {
            if (bands[currentBand].mode == 1) {
              audioYPixel[k] = 50 +   map(15 * log10f((audioSpectBuffer[k] + audioSpectBuffer[k + 1] + audioSpectBuffer[k + 2]) / 3), 0, 100, 0, 120);
            }
          }
          if (audioYPixel[k] < 0) {
            audioYPixel[k] = 0;
          }
        }
        arm_max_f32 (audioSpectBuffer, 1024, &audioMaxSquared, &AudioMaxIndex);  // Max value of squared bin magnitued in audio
        audioMaxSquaredAve = .5 * audioMaxSquared + .5 * audioMaxSquaredAve;  // Running averaged values
      }

      /**********************************************************************************
          Additional Convolution Processes:
          // filter by just deleting bins - principle of Linrad
            only works properly when we have the right window function!

          (automatic) notch filter = Tone killer --> the name is stolen from SNR ;-)
          first test, we set a notch filter at 1kHz
          which bin is that?
          positive & negative frequency -1kHz and +1kHz --> delete 2 bins
          we are not deleting one bin, but five bins for the test
          1024 bins in 12ksps = 11.71Hz per bin
          SampleRate / 8.0 / 1024 = bin BW
          1000Hz / 11.71Hz = bin 85.333

      **********************************************************************************/

      /**********************************************************************************
        After the frequency domain filter mask and other processes are complete, do a
        complex inverse FFT to return to the time domain
          (if sample rate = 192kHz, we are in 24ksps now, because we decimated by 8)
          perform iFFT (in-place)  IFFT is selected by the IFFT flag=1 in the Arm CFFT function.
      **********************************************************************************/

      arm_cfft_f32(iS, iFFT_buffer, 1, 1);

      // Adjust for level alteration because of filters

      /**********************************************************************************
          AGC - automatic gain control

          we´re back in time domain
          AGC acts upon I & Q before demodulation on the decimated audio data in iFFT_buffer
      **********************************************************************************/
      AGC();  //AGC function works with time domain I and Q data buffers created in the last step
    }

    /**********************************************************************************
      Demodulation
        our time domain output is a combination of the real part (left channel) AND the imaginary part (right channel) of the second half of the FFT_buffer
        The demod mode is accomplished by selecting/combining the real and imaginary parts of the output of the IFFT process.
    **********************************************************************************/

    switch (bands[currentBand].mode) {
      case DEMOD_USB:
      case DEMOD_FT8: // demodulate FT8 signals via antenna input as USB for audio
        for (unsigned i = 0; i < FFT_length / 2; i++) {
          // if (bands[currentBand].mode == DEMOD_USB || bands[currentBand].mode == DEMOD_LSB ) {  // for SSB copy real part in both outputs
          float_buffer_L[i] = iFFT_buffer[FFT_length + (i * 2)];

          float_buffer_R[i] = float_buffer_L[i];
          audiotmp = AlphaBetaMag(iFFT_buffer[FFT_length + (i * 2)], iFFT_buffer[FFT_length + (i * 2) + 1]);
        }

          // save audio signal to FT8 bubber
        if(bands[currentBand].mode == DEMOD_FT8) {
          if(ft8_decode_flag == 0) {
            // Pocket_FT8 process_data()
            // convert floats to the q15 required by FT8 routines
            arm_float_to_q15(float_buffer_L, q15_buffer_LTemp, 256);

            for (unsigned i = 0; i < 68; i++) {
              // roll ft8 dsp buffer
              ft8_dsp_buffer[i + ft8Loop * 68] = ft8_dsp_buffer[i + 1024 + ft8Loop * 68];
              ft8_dsp_buffer[1024 + i + ft8Loop * 68] = ft8_dsp_buffer[i + 2048 + ft8Loop * 68];

              // transfer audio data to ft8_dsp_buffer
              // decimate by 3.75 which brings us to a 6.4 ksps rate
              //ft8_dsp_buffer[2048 + i + ft8Loop * 68] = float_buffer_L[(int)(i * 3.75)]; 
              ft8_dsp_buffer[2048 + i + ft8Loop * 68 + leftover] = q15_buffer_LTemp[(int)(i * 15 / 4)]; // i * 3.75
            }

            ++ft8Loop;
            if(ft8Loop == 15) {
              // fill last cell
              ft8_dsp_buffer[3071] = q15_buffer_LTemp[255];

              ft8Loop = 0;
              leftover = 0;

              DSP_Flag = 1;

            } else {
              // take 3 more samples over the range (every 4th loop) to completely fill the buffer
              if(ft8Loop == 4 || ft8Loop == 8 || ft8Loop == 12) {
                ft8_dsp_buffer[2048 + ft8Loop * 68 + leftover] = q15_buffer_LTemp[255];
                leftover++;
              }
            }
          }

          if(DSP_Flag == 1 && ft8_flag == 1) {
            // *** investigate threads to handle FT8 processing while we continue to collect audio
            process_FT8_FFT();
            DSP_Flag = 0;
          }

          if(ft8_decode_flag == 1) {
            num_decoded_msg = ft8_decode();
            if(num_decoded_msg > 0) {
              display_details(num_decoded_msg, 5);
            }

            ft8_decode_flag = 0;  
          }

          if(ft8_flag == 0) update_synchronization();
        }
        break;

      case DEMOD_LSB:
        for (unsigned i = 0; i < FFT_length / 2; i++) {
          //if (bands[currentBand].mode == DEMOD_USB || bands[currentBand].mode == DEMOD_LSB ) {  // for SSB copy real part in both outputs
          float_buffer_L[i] = iFFT_buffer[FFT_length + (i * 2)];

          float_buffer_R[i] = float_buffer_L[i];
        }
        break;

      case DEMOD_AM:
        for (unsigned i = 0; i < FFT_length / 2; i++) {     // Magnitude estimation Lyons (2011): page 652 / libcsdr
          audiotmp = AlphaBetaMag(iFFT_buffer[FFT_length + (i * 2)], iFFT_buffer[FFT_length + (i * 2) + 1]);
          // DC removal filter -----------------------
          w = audiotmp + wold * 0.99f; // Response to below 200Hz
          float_buffer_L[i] = w - wold;
          wold = w;
        }
        arm_biquad_cascade_df1_f32(&biquad_lowpass1, float_buffer_L, float_buffer_R, FFT_length / 2);
        arm_copy_f32(float_buffer_R, float_buffer_L, FFT_length / 2);
        break;

      case DEMOD_NFM:
        // three versions to select from:
        //  (1) - fmdemod_quadri_novect_cf
        //  (2) - nfmdemod
        //  (3) - arm_max_f32
        // the first two have about same performance, the third didn't perform well on my test signal

        nfmdemod(&FFT_buffer[FFT_length], float_buffer_L, FFT_length / 2);
        
        // limit the demodulated signal
        for (unsigned i = 1; i < FFT_length / 2; i++) {
          float32_t tmp = float_buffer_L[i];
        
          // limit it to -1 <= tmp <= 1
          // modified from limit_ff in libcsdr.c from https://github.com/ha7ilm/csdr
          tmp = (1 < tmp) ? 1 : tmp;
          tmp = (-1 > tmp) ? -1 : tmp;
          float_buffer_L[i] = tmp;
        }

        // no difference in audio with this
        //arm_biquad_cascade_df1_f32(&biquad_lowpass1, float_buffer_L, float_buffer_R, FFT_length / 2);
        //arm_copy_f32(float_buffer_R, float_buffer_L, FFT_length / 2);

        // buzz and muffled sound with this deemphasis filter
        //deemphasis_nfm_ff(float_buffer_L, float_buffer_R, FFT_length / 2, 24000);
        //arm_copy_f32(float_buffer_R, float_buffer_L, FFT_length / 2);

        //deemphasis_nfm_ff(float_buffer_L, float_buffer_R, FFT_length / 2, 24000);
        //arm_biquad_cascade_df1_f32(&biquad_lowpass1, float_buffer_R, float_buffer_L, FFT_length / 2);

        //arm_biquad_cascade_df1_f32(&biquad_lowpass1, float_buffer_L, float_buffer_R, FFT_length / 2);
        //deemphasis_nfm_ff(float_buffer_R, float_buffer_L, FFT_length / 2, 24000);
        break;

      case DEMOD_SAM:
        AMDecodeSAM();
        break;
    }

    // update audio spectrum for NFM
    if(bands[currentBand].mode == DEMOD_NFM) {

      // Prepare the audio signal buffers
      for (unsigned i = 0; i < BUFFER_SIZE * N_BLOCKS / (uint32_t)(DF); i++) {
        // fill first half of buffer with last sample
        FFT_buffer[i * 2] = last_sample_buffer_L[i]; // real
        FFT_buffer[i * 2 + 1] = 0; // there is no imaginary component

        // copy recent sample to last_sample_buffer for next time
        last_sample_buffer_L [i] = float_buffer_L[i];

        // fill recent audio samples into FFT_buffer
        FFT_buffer[FFT_length + i * 2] = float_buffer_L[i]; // real
        FFT_buffer[FFT_length + i * 2 + 1] = 0; // there is no imaginary component
      }

      /**********************************************************************************
        Perform complex FFT on the audio time signals
        calculation is performed in-place the FFT_buffer [re, im, re, im, re, im . . .]
      **********************************************************************************/

      // prepare audio box spectrum and filter per the audio cutoff frequency
      arm_cfft_f32(S, FFT_buffer, 0, 1);
      arm_cmplx_mult_cmplx_f32 (FFT_buffer, FIR_filter_mask, iFFT_buffer, FFT_length);

      // process audio frequency spectrum only at the beginning of the show spectrum process
      if (updateDisplayFlag == 1) {
        for (int k = 0; k < 1024; k++) {
          audioSpectBuffer[1023 - k] = (iFFT_buffer[k] * iFFT_buffer[k]);
        }
        //for (int k = 0; k < 256; k++) {
        for (int k = 0; k < AUDIO_SPEC_BOX_W - 2; k++) {
          // a spectrum offset of 20 give about the same magnitude signal peak as seen in the AM modes
          audioYPixel[k] = 20 +  map(15 * log10f((audioSpectBuffer[1021 - k] + audioSpectBuffer[1022 - k] + audioSpectBuffer[1023 - k]) / 3), 0, 100, 0, 120);
          if (audioYPixel[k] < 0) {
            audioYPixel[k] = 0;
          }
        }
        arm_max_f32 (audioSpectBuffer, 1024, &audioMaxSquared, &AudioMaxIndex);  // Max value of squared bin magnitued in audio
        audioMaxSquaredAve = .5 * audioMaxSquared + .5 * audioMaxSquaredAve;  // Running averaged values
      }

      // convert back to time domain and fill buffer
      arm_cfft_f32(iS, iFFT_buffer, 1, 1);

      AGC();  // we can perform AGC on the NFM signal now

      // transfer audio signal back to buffer
      for (unsigned i = 0; i < FFT_length / 2; i++) {
        float_buffer_L[i] = iFFT_buffer[FFT_length + (i * 2)];
      }
    }


    //============================  Receive EQ  ========================
    if (receiveEQFlag == ON ) {
      DoReceiveEQ();
      //arm_copy_f32(float_buffer_L, float_buffer_R, FFT_length / 2);
    }
    //============================ End Receive EQ

    /**********************************************************************************
      Noise Reduction
      3 algorithms working 3-15-22
      NR_Kim
      Spectral NR
      LMS variable leak NR
    **********************************************************************************/
    switch (nrOptionSelect) {
      case 0:                               // NR Off
        break;
      case 1:                               // Kim NR
        Kim1_NR();
        arm_scale_f32 (float_buffer_L, 30, float_buffer_L, FFT_length / 2);
        //arm_scale_f32 (float_buffer_R, 30, float_buffer_R, FFT_length / 2);
        break;
      case 2:                               // Spectral NR
        SpectralNoiseReduction();
        break;
      case 3:                               // LMS NR
        ANR_notch = 0;
        Xanr();
        arm_scale_f32 (float_buffer_L, 1.5, float_buffer_L, FFT_length / 2);
        //arm_scale_f32 (float_buffer_R, 2, float_buffer_R, FFT_length / 2);
        break;
    }
    //==================  End NR ============================

    // ===========================Automatic Notch ==================
    if (ANR_notchOn == 1) {
      ANR_notch = 1;
      Xanr();
      arm_copy_f32(float_buffer_R, float_buffer_L, FFT_length / 2);
    }
    // ====================End notch =================================

    /**********************************************************************************
      EXPERIMENTAL: noise blanker
      by Michael Wild
    **********************************************************************************/
    if (NB_on != 0) {
      NoiseBlanker(float_buffer_L, float_buffer_R);
      arm_copy_f32(float_buffer_R, float_buffer_L, FFT_length / 2);
    }

    if (T41State == CW_RECEIVE) {
      DoCWReceiveProcessing();

      // ----------------------  CW Narrow band filters -------------------------
      if (CWFilterIndex != 5) {
        switch (CWFilterIndex) {
          case 0:  // 0.8 KHz
            arm_biquad_cascade_df2T_f32(&S1_CW_AudioFilter1, float_buffer_L, float_buffer_L_AudioCW, 256);
            arm_copy_f32(float_buffer_L_AudioCW, float_buffer_L, FFT_length / 2);
            //arm_copy_f32(float_buffer_L_AudioCW, float_buffer_R, FFT_length / 2);
            break;

          case 1: // 1.0 KHz
            arm_biquad_cascade_df2T_f32(&S1_CW_AudioFilter2, float_buffer_L, float_buffer_L_AudioCW, 256);
            arm_copy_f32(float_buffer_L_AudioCW, float_buffer_L, FFT_length / 2);
            //arm_copy_f32(float_buffer_L_AudioCW, float_buffer_R, FFT_length / 2);
            break;

          case 2: // 1.3 KHz
            arm_biquad_cascade_df2T_f32(&S1_CW_AudioFilter3, float_buffer_L, float_buffer_L_AudioCW, 256);
            arm_copy_f32(float_buffer_L_AudioCW, float_buffer_L, FFT_length / 2);
            //arm_copy_f32(float_buffer_L_AudioCW, float_buffer_R, FFT_length / 2);
            break;

          case 3: // 1.8 KHz
            arm_biquad_cascade_df2T_f32(&S1_CW_AudioFilter4, float_buffer_L, float_buffer_L_AudioCW, 256);
            arm_copy_f32(float_buffer_L_AudioCW, float_buffer_L, FFT_length / 2);
            //arm_copy_f32(float_buffer_L_AudioCW, float_buffer_R, FFT_length / 2);
            break;

          case 4:  // 2.0 KHz
            arm_biquad_cascade_df2T_f32(&S1_CW_AudioFilter5, float_buffer_L, float_buffer_L_AudioCW, 256);
            arm_copy_f32(float_buffer_L_AudioCW, float_buffer_L, FFT_length / 2);
            //arm_copy_f32(float_buffer_L_AudioCW, float_buffer_R, FFT_length / 2);
            break;

          case 5:  //Off
            break;
        }
      }
    }

    // ======================================Interpolation  ================
    // interpolation-by-2
    arm_fir_interpolate_f32(&FIR_int1_I, float_buffer_L, iFFT_buffer, BUFFER_SIZE * N_BLOCKS / (uint32_t)(DF));

    // interpolation-by-4
    arm_fir_interpolate_f32(&FIR_int2_I, iFFT_buffer, float_buffer_L, BUFFER_SIZE * N_BLOCKS / (uint32_t)(DF1));

    /**********************************************************************************
      Digital Volume Control
    **********************************************************************************/
    if (mute == 1) {
      arm_scale_f32(float_buffer_L, 0.0, float_buffer_L, BUFFER_SIZE * N_BLOCKS);
    } else {
      if (mute == 0) {
        arm_scale_f32(float_buffer_L, DF * VolumeToAmplification(audioVolume), float_buffer_L, BUFFER_SIZE * N_BLOCKS);
      }
    }

    /**********************************************************************************
      CONVERT TO INTEGER AND PLAY AUDIO
    **********************************************************************************/
    arm_float_to_q15 (float_buffer_L, q15_buffer_LTemp, 2048);
    Q_out_L.play(q15_buffer_LTemp, 2048);

    Codec_gain();

    elapsed_micros_sum = elapsed_micros_sum + usec;
    elapsed_micros_idx_t++;
  } // end of if(audio blocks available)
  }
}

/*****
  Purpose: scale volume from 0 to 100

  Parameter list:
    int volume        the current reading

  Return value;
    void
*****/
float VolumeToAmplification(int volume) {
  float x = volume / 100.0f;  //"volume" Range 0..100
                              //#if 0
                              //  float a = 3.1623e-4;
                              //  float b = 8.059f;
                              //  float ampl = a * expf( b * x );
                              //  if (x < 0.1f) ampl *= x * 10.0f;
                              //#else
  //Approximation:
  float ampl = 5 * x * x * x * x * x;  //70dB
                                       //#endif
  return ampl;
}

/*****
  Purpose: To set the codec gain

  Parameter list:
    void

  Return value:
    void

*****/
void Codec_gain() {
  static uint32_t timer = 0;
  static uint8_t half_clip = 0;
  static uint8_t quarter_clip = 0;

  timer++;
  if (timer > 10000) timer = 10000;
  if (half_clip == 1)  // did clipping almost occur?
  {
    if (timer >= 20)  // 100  // has enough time passed since the last gain decrease?
    {
      if (bands[currentBand].RFgain != 0)  // yes - is this NOT zero?
      {
        bands[currentBand].RFgain -= 1;  // decrease gain one step, 1.5dB
        if (bands[currentBand].RFgain < 0) {
          bands[currentBand].RFgain = 0;
        }
        timer = 0;  // reset the adjustment timer
        AudioNoInterrupts();
        AudioInterrupts();
      }
    }
  } else if (quarter_clip == 0)  // no clipping occurred
  {
    if (timer >= 50)  // 500   // has it been long enough since the last increase?
    {
      bands[currentBand].RFgain += 1;  // increase gain by one step, 1.5dB
      timer = 0;                       // reset the timer to prevent this from executing too often
      if (bands[currentBand].RFgain > 15) {
        bands[currentBand].RFgain = 15;
      }
      AudioNoInterrupts();
      AudioInterrupts();
    }
  }
  half_clip = 0;     // clear "half clip" indicator that tells us that we should decrease gain
  quarter_clip = 0;  // clear indicator that, if not triggered, indicates that we can increase gain
}
