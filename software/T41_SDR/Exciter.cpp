
#include "SDT.h"
#include "AudioConfig.h"
#include "Exciter.h"
//#include "EEPROM.h"
#include "Filter.h"
#include "FIR.h"
#include "Menu.h"
#include "Utility.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

/*****
  Purpose: Create I and Q signals from Mic input

  Parameter list:

  Return value:
    void
    Notes:
    There are several actions in this function
    1.  Read in the data from the ADC into the Left Channel at 192KHz
    2.  Format the L data and Decimate (downsample and filter)the sampled data by x8
          - the new effective sampling rate is now 24KHz
    3.  Process the L data through the 7 EQ filters and combine to a single data stream
    4.  Copy the L channel to the R channel
    5.  Process the R and L through two Hilbert Transformers - L 0deg phase shift and R 90 deg ph shift
          - This create the I (L) and Q(R) channels
    6.  Interpolate 8x (upsample and filter) the data stream to 192KHz sample rate
    7.  Output the data stream thruogh the DACs at 192KHz
*****/
void ExciterIQData() {
  int16_t *sp_L, *sp_R;

  // process samples from queue buffer if there are at least 16 buffers available
  if((uint32_t) Q_in_L_Ex.available() > 16) {
    // get audio samples from the audio  buffers and convert them to float
    for(unsigned i = 0; i < 16; i++) {
      // read in 16 blocks á 128 samples into the left channel, we'll duplicate this later
      sp_L = Q_in_L_Ex.readBuffer();

      // convert to float one buffer_size, samples are now standardized from > -1.0 to < 1.0
      arm_q15_to_float (sp_L, &float_buffer_L_EX[128 * i], 128);
      Q_in_L_Ex.freeBuffer();
    }

    /**********************************************************************************  AFP 12-31-20
              Decimation is the process of downsampling the data stream and LP filtering
              Decimation is done in two stages to prevent reversal of the spectrum, which occure with each even
              Decimation.  First select every 4th asmple and then every 2nd sample, yielding 8x downsampling
              192KHz/8 = 24KHz, with 8xsmaller sample sizes
     **********************************************************************************/

    // reduce sample rate and size by decimation by 8
    // decimate in two stages to maintain spectrum order
    // 192kHz effective sample rate here
    // decimation-by-4 in-place!
    arm_fir_decimate_f32(&FIR_dec1_EX_I, float_buffer_L_EX, float_buffer_L_EX, 2048);

    // 48KHz effective sample rate here
    // decimation-by-2 in-place
    arm_fir_decimate_f32(&FIR_dec2_EX_I, float_buffer_L_EX, float_buffer_L_EX, 512);

    // perform transmit EQ if activated
    if(xmitEQFlag == ON ) {
      DoExciterEQ();
    }

    arm_copy_f32(float_buffer_L_EX, float_buffer_R_EX, 256);

    //--------------  Hilbert Transformers
    arm_fir_f32(&FIR_Hilbert_L, float_buffer_L_EX, float_buffer_L_EX, 256);
    arm_fir_f32(&FIR_Hilbert_R, float_buffer_R_EX, float_buffer_R_EX, 256);

    // apply IQ calibration factors
    // *** TODO: v49.2k has currentBandA, why? ***
    if(bands[currentBand].demod == DEMOD_LSB) {
      arm_scale_f32(float_buffer_L_EX, + IQXAmpCorrectionFactor[currentBand], float_buffer_L_EX, 256);     // Flip SSB sideband KF5N, minus sign was original
    }
    else if(bands[currentBand].demod == DEMOD_USB) {
      arm_scale_f32(float_buffer_L_EX, - IQXAmpCorrectionFactor[currentBand], float_buffer_L_EX, 256);    // Flip SSB sideband KF5N
    }
    IQPhaseCorrection(float_buffer_L_EX, float_buffer_R_EX, IQXPhaseCorrectionFactor[currentBand], 256);


    // return to 192kHz, interpolate by a factor of 8, once again in two steps to preserve the spectrum order
    //24KHz effective sample rate here
    arm_fir_interpolate_f32(&FIR_int1_EX_I, float_buffer_L_EX, float_buffer_Temp, 256);

    // interpolation-by-4,  48KHz effective sample rate here
    arm_fir_interpolate_f32(&FIR_int2_EX_I, float_buffer_Temp, float_buffer_L_EX, 512);

    // and again for R channel
    arm_fir_interpolate_f32(&FIR_int1_EX_Q, float_buffer_R_EX, float_buffer_Temp, 256);
    arm_fir_interpolate_f32(&FIR_int2_EX_Q, float_buffer_Temp, float_buffer_R_EX, 512);

    //  192KHz effective sample rate here
    // scale to compensate for losses during interpolation
    arm_scale_f32(float_buffer_L_EX, 20, float_buffer_L_EX, 2048);
    arm_scale_f32(float_buffer_R_EX, 20, float_buffer_R_EX, 2048);

    // convert to integer values and output
    for(unsigned  i = 0; i < 16; i++) {
      sp_L = Q_out_L_Ex.getBuffer();
      sp_R = Q_out_R_Ex.getBuffer();
      arm_float_to_q15 (&float_buffer_L_EX[128 * i], sp_L, 128);
      arm_float_to_q15 (&float_buffer_R_EX[128 * i], sp_R, 128);
      Q_out_L_Ex.playBuffer();
      Q_out_R_Ex.playBuffer();
    }
  }
}

/*****
  Purpose: Set the current band relay ON or OFF

  Parameter list:
    int state             OFF = 0, ON = 1

  Return value:
    void
*****/
void SetBandRelay(int state) {
  // There are 4 physical relays.  Turn all of them off.
  for(int i = 0; i < 4; i = i + 1) {
  digitalWrite(bandswitchPins[i], LOW); // Set ALL band relays low.  KF5N July 21, 2023
  }
// Set current band relay "on".  Ignore 12M and 10M.  15M and 17M use the same relay.  KF5N September 27, 2023.
  if(currentBand < BAND_12M) digitalWrite(bandswitchPins[currentBand], state);
}
