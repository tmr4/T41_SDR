
#include "SDT.h"
#include "AudioConfig.h"
#include "Exciter.h"
#include "FIR.h"
#include "keyer.h"
#include "Utility.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

// keyPressedOn set to 1 in ISR indicating a dit or dah key press in CW mode.
// This should be reset to 0 after processing the key click.
// I've removed check for this being set in ShowSpectrum and ProcessIQData.
// This means there will be an increased lag from when the CW key is pressed
// until it is recognized in the next processing loop after the current loop completes.
// I haven't noticed this, perhaps because a more efficient processing loop.
// *** TODO: consider reintroducing an early return from longer running process loop
//     code if lag becomes noticable. ***
uint8_t keyPressedOn = 0;

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

/*****
  Purpose: CW key interrupt

  Parameter list:
    void

  Return value:
    void
*****/
void KeyTipOn() {
  if(digitalRead(KEYER_DIT_INPUT_TIP) == LOW && xmtMode == CW_MODE ) {
    keyPressedOn = 1;
  }
}

/*****
  Purpose: CW key interrupt

  Parameter list:
    void

  Return value:
    void
*****/
void KeyRingOn() {
  if(keyType == 1) {
    if(digitalRead(KEYER_DAH_INPUT_RING) == LOW && xmtMode == CW_MODE ) {
      keyPressedOn = 1;
    }
  }
}

/*****
  Purpose: Create and play I and Q sample for CW signal
           This creates a 10ms, 750 Hz sample at 192 kHz sample rate to the Teensy Audio Adapter line-out to
           the exciter board.  Function must be called again within that time for a continuous signal.  Prior to call
           Q_out_L_Ex, Q_out_R_Ex and Q_out_L (for sidetone) must be properly routed.  Sidetone signal adjusted for a gain of 1
           at a volume of 30.  Signal level is controlled by powerOutCW[currentBand] and volumeLog[sidetoneVolume] / 0.000100.
           This gives a reasonable volume with power level of 1-20 W.  This should be done prior to calling this function or CreateCWSignal.

  Parameter list:
    int state       turn signal ON or OFF
    bool ramp       add a ramp (upwards for on, downwards for off)
    bool pause      pause for 10ms while signal plays
    int timeAdjust  shorten the ramp block by timeAdjust ms
    bool pwrScale   scale signal for loses during interpolation

  Return value:
    void
*****/
void CW_ExciterIQData(int state = ON, bool ramp = false, bool pause = true, float timeAdjust = 0.0, bool pwrScale = true) {
  float cwPwr = (-.0133 * transmitPowerLevel * transmitPowerLevel + .7884 * transmitPowerLevel + 4.5146) * CWPowerCalibrationFactor[currentBand];
  float fac;

  // create I/Q from precalculated buffers (750 Hz signal at a 24kHz sample rate)
  arm_scale_f32(cosBuffer, 0.2, float_buffer_L_EX, 256);
  arm_scale_f32(sinBuffer, 0.2, float_buffer_R_EX, 256);

  /**********************************************************************************
      Additional scaling, if nesessary to compensate for down-stream gain variations
   **********************************************************************************/

  if(bands[currentBand].demod == DEMOD_LSB) {
    //arm_scale_f32(float_buffer_L_EX, IQXAmpCorrectionFactor[currentBandA], float_buffer_L_EX, 256);  //Adjust level of L buffer
    arm_scale_f32(float_buffer_L_EX, -IQXAmpCorrectionFactor[currentBand], float_buffer_L_EX, 256);       //Adjust level of L buffer KF5N flipped sign, original was +
    IQPhaseCorrection(float_buffer_L_EX, float_buffer_R_EX, IQXPhaseCorrectionFactor[currentBand], 256);  // Adjust phase
  } else {
    if(bands[currentBand].demod == DEMOD_USB) {
      //arm_scale_f32(float_buffer_L_EX, -IQXAmpCorrectionFactor[currentBandA], float_buffer_L_EX, 256);
      arm_scale_f32(float_buffer_L_EX, + IQXAmpCorrectionFactor[currentBand], float_buffer_L_EX, 256);   // KF5N flipped sign, original was minus
      IQPhaseCorrection(float_buffer_L_EX, float_buffer_R_EX, IQXPhaseCorrectionFactor[currentBand], 256);
    }
  }

  // ramp signal if requested
  if(ramp) {
    // adjust start or end 10 ms block for a variable time and a 5 ms raised cosine ramp
    // (see https://www.w8ji.com/keyclicks.htm for good discussion on shaping CW signals).
    for(int i = 0; i < 256; i++) {
      if(state == ON) {
        // signal turning on
        int begin = (int)(timeAdjust * 25.5);
        //int end = (int)((5 + timeAdjust) * 25.5);
        int end;

        if(begin > 128) begin = 128;
        end = begin + 128;

        if(i < begin) {
          fac = 0.0;
        } else {
          if(i < end) {
            fac = cwRampUp[i - begin];
          } else {
            fac = 1.0;
          }
        }
      } else {
        // signal turning off
        int end = (int)((10.0 - timeAdjust) * 25.5);
        int begin;

        if(end < 128) end = 128;
        begin = end - 128;

        if(i < begin) {
          fac = 1.0;
        } else {
          if(i < end) {
            fac = cwRampDown[i - begin];
          } else {
            fac = 0.0;
          }
        }
      }
      float_buffer_L_EX[i] *= fac;
      float_buffer_R_EX[i] *= fac;
    }
  } else if(state == OFF) {
    // signal off, scale to 0
    arm_scale_f32(float_buffer_L_EX, 0.0, float_buffer_L_EX, 256);
    arm_scale_f32(float_buffer_R_EX, 0.0, float_buffer_R_EX, 256);
  }

  /**********************************************************************************
    Interpolate (upsample the data streams by 8X to create the 192 kHz sample rate for output
    Requires a LPF FIR 48 tap 10KHz and 8KHz
    **********************************************************************************/
  // interpolation I channel by 2 to 48kHz
  arm_fir_interpolate_f32(&FIR_int1_EX_I, float_buffer_L_EX, float_buffer_Temp, 256);

  // interpolation I channel by 4 to 192 kHz
  arm_fir_interpolate_f32(&FIR_int2_EX_I, float_buffer_Temp, float_buffer_L_EX, 512);

  // interpolate 2x and 4x again with Q channel
  arm_fir_interpolate_f32(&FIR_int1_EX_Q, float_buffer_R_EX, float_buffer_Temp, 256);
  arm_fir_interpolate_f32(&FIR_int2_EX_Q, float_buffer_Temp, float_buffer_R_EX, 512);

  // scale to compensate for losses in interpolation
  if(pwrScale) {
    arm_scale_f32(float_buffer_L_EX, 20 * cwPwr, float_buffer_L_EX, 2048);
    arm_scale_f32(float_buffer_R_EX, 20 * cwPwr, float_buffer_R_EX, 2048);
  } else {
    arm_scale_f32(float_buffer_L_EX, 20, float_buffer_L_EX, 2048);
    arm_scale_f32(float_buffer_R_EX, 20, float_buffer_R_EX, 2048);
  }

  /**********************************************************************************
    CONVERT TO INTEGER AND PLAY AUDIO
  **********************************************************************************/

  q15_t q15_buffer_LTemp[2048];
  q15_t q15_buffer_RTemp[2048];

  arm_float_to_q15(float_buffer_L_EX, q15_buffer_LTemp, 2048);
  arm_float_to_q15(float_buffer_R_EX, q15_buffer_RTemp, 2048);

  // reset CW signal timing if we've started a new signal
  // *** TODO: this will have problems if we don't ramp ***
  if(state == ON && ramp) {
    cwAtomTimer = 0;
  }

  Q_out_L_Ex.play(q15_buffer_LTemp, 2048);
  Q_out_R_Ex.play(q15_buffer_RTemp, 2048);
  Q_out_L.play(q15_buffer_LTemp, 2048);

  if(state == ON && ramp) {
    while(cwAtomTimer < timeAdjust) {
      ;
    }

    // reset cwAtomTimer here at end of on ramp
    // to get most accurate signal timing from CreateCWSignal
    cwAtomTimer = 0;
  }

  if(pause) {
    // CW_ExciterIQData produces ~10ms of data
    // pause while signal plays to prevent needless churn
    CWPause(10);
  }
}

/*****
  Purpose: Create I and Q signal of given length for CW
           Signal consists of a shaped starting and ending blocks and enough 10 ms blocks
           to create a signal of specified length.

  Parameter list:
    unsigned long signalLength

  Return value:
    void
*****/
void CreateCWSignal(unsigned long signalLength) {
  // # of full 10ms blocks (less initial on and final off 10 ms blocks) required to create signal
  int blocks = (signalLength - 20) / 10;
  float timeAdjust = 2.5 * (signalLength / transmitDitLength); // required time adjustment for signal length

  // maintain at least a 5 ms ramp
  // *** this could cause timing to differ slightly from signalLength ***
  if(timeAdjust > 5.0) {
    timeAdjust = 2.5;
    blocks -= 1;
  }

  // queue blocks required for signalLength
  // we won't pause for each call, but pause for the
  // overall signal length after queueing these
  // ramp up
  CW_ExciterIQData(ON, true, false, timeAdjust);

  // body
  for(int i = 0; i < blocks; i++) {
    CW_ExciterIQData(ON, false, false);
  }

  // ramp down
  CW_ExciterIQData(OFF, true, false, timeAdjust);

  // pause while signal plays
  // cwAtomTimer is reset at end of CW_ExciterIQData ramp up
  // for most accurate timing
  while(cwAtomTimer < signalLength) {
    ;
  }
}
