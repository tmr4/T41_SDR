#pragma once

//======================================== User section that might need to be changed ===================================
#include "MyConfigurationFile.h"  // This file name should remain unchanged
#define VERSION "sdr_dev.1"       // Change this for updates. If you make this longer than 9 characters, brace yourself for surprises

//======================================== Library include files ========================================================
// need to verify that all these libraries are still used
#include <Adafruit_GFX.h>
//#include "Fonts/FreeMonoBold24pt7b.h"
//#include "Fonts/FreeMonoBold18pt7b.h"
//#include "Fonts/FreeMono24pt7b.h"
//#include "Fonts/FreeMono9pt7b.h"
#include <Audio.h>                     //https://github.com/chipaudette/OpenAudio_ArduinoLibrary
#include <OpenAudio_ArduinoLibrary.h>  // AFP 11-01-22
#include <TimeLib.h>                   // Part of Teensy Time library
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <Metro.h>
#include <Bounce.h>
#include <arm_math.h>
#include <arm_const_structs.h>
#include <si5351.h>                    // https://github.com/etherkit/Si5351Arduino
#include <RA8875.h>                    // https://github.com/mjs513/RA8875/tree/RA8875_t4
#include <Rotary.h>                    // https://github.com/brianlow/Rotary
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <util/crc16.h>                // mdrhere
#include <utility/imxrt_hw.h>          // for setting I2S freq
#include <EEPROM.h>

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

#define BOGUS_PIN_READ             -1 // If no push button read
#define FFT_LENGTH                512
#define SSB_MODE                    0
#define CW_MODE                     1
#define OFF                         0
#define ON                          1

// demodulation modes
#define DEMOD_MIN                   0
#define DEMOD_USB                   0
#define DEMOD_LSB                   1
#define DEMOD_AM                    2
#define DEMOD_NFM                   3
#define DEMOD_FT8_WAV               4
#define DEMOD_FT8                   5
#define DEMOD_SAM                   6
#define DEMOD_MAX                   6

#define BUFFER_SIZE                 128

//---- Global Teensy 4.1 Pin assignments
#define RXTX                        22    // Transmit/Receive
#define KEYER_DAH_INPUT_RING        35    // Ring connection for keyer  -- default for righthanded user
#define KEYER_DIT_INPUT_TIP         36    // Tip connection for keyer
#define MUTE                        38    // Mute Audio,  HIGH = "On" Audio available from Audio PA, LOW = Mute audio
#define BUSY_ANALOG_PIN             39    // This is the analog pin that controls the 18 switches
//---- End Global Teensy 4.1 Pin assignments

//************************************* End Global Defines ************************

//************************************* Clean up stuff to fix ************************

// delete once we get rid of global working variables
#include "gwv.h"

// Find a home for these or eliminate them

// eliminate any overlap with radioState and T41State and the below 
#define SSB_RECEIVE                 0
#define CW_RECEIVE                  2
#define SSB_RECEIVE_STATE 0
#define SSB_TRANSMIT_STATE 1
#define CW_RECEIVE_STATE 2
#define CW_TRANSMIT_STRAIGHT_STATE 3
#define CW_TRANSMIT_KEYER_STATE 4
#define RECEIVE_STATE         1
#define TRANSMIT_STATE        0

//************************************* End: Clean up stuff to fix ************************

extern byte sharedRAM1[1024 * 8];
extern byte /*DMAMEM*/ sharedRAM2[2048 * 13] __attribute__ ((aligned (4)));

extern int radioState, lastState;  // Used by the loop to monitor current state.
extern long CWFreqShift;
extern long calFreqShift;

extern long NCOFreq;

extern arm_fir_interpolate_instance_f32 FIR_int1_EX_I;
extern arm_fir_interpolate_instance_f32 FIR_int1_EX_Q;
extern arm_fir_interpolate_instance_f32 FIR_int2_EX_I;
extern arm_fir_interpolate_instance_f32 FIR_int2_EX_Q;

extern float32_t  FIR_int1_EX_I_state[];
extern float32_t  FIR_int1_EX_Q_state[];

extern float32_t  float_buffer_L_EX[];
extern float32_t  float_buffer_R_EX[];
extern float32_t  float_buffer_Temp[];

extern AudioMixer4 modeSelectInR;
extern AudioMixer4 modeSelectInL;
extern AudioMixer4 modeSelectInExR;
extern AudioMixer4 modeSelectInExL;

extern AudioMixer4 modeSelectOutL;
extern AudioMixer4 modeSelectOutR;
extern AudioMixer4 modeSelectOutExL;
extern AudioMixer4 modeSelectOutExR;

extern AudioRecordQueue Q_in_L;
extern AudioRecordQueue Q_in_R;
extern AudioRecordQueue Q_in_L_Ex;
extern AudioRecordQueue Q_in_R_Ex;

extern AudioPlayQueue Q_out_L;
extern AudioPlayQueue Q_out_R;
extern AudioPlayQueue Q_out_L_Ex;
extern AudioPlayQueue Q_out_R_Ex;

extern Bounce selectExitMenues;

extern Rotary volumeEncoder;        // (2,  3)
extern Rotary tuneEncoder;          // (16, 17)
extern Rotary menuChangeEncoder;        // (14, 15)
extern Rotary fineTuneEncoder;  // (4,  5);

extern Si5351 si5351;

extern const int SampleRate;

extern const arm_cfft_instance_f32 *S;
extern const arm_cfft_instance_f32 *iS;
extern const arm_cfft_instance_f32 *maskS;
extern const arm_cfft_instance_f32 *NR_FFT;
extern const arm_cfft_instance_f32 *NR_iFFT;
extern const arm_cfft_instance_f32 *spec_FFT;

extern arm_biquad_casd_df1_inst_f32 biquad_lowpass1;
extern arm_biquad_casd_df1_inst_f32 IIR_biquad_Zoom_FFT_I;
extern arm_biquad_casd_df1_inst_f32 IIR_biquad_Zoom_FFT_Q;

extern arm_fir_decimate_instance_f32 FIR_dec1_I;
extern arm_fir_decimate_instance_f32 FIR_dec1_Q;
extern arm_fir_decimate_instance_f32 FIR_dec2_I;
extern arm_fir_decimate_instance_f32 FIR_dec2_Q;
extern arm_fir_decimate_instance_f32 Fir_Zoom_FFT_Decimate_I;
extern arm_fir_decimate_instance_f32 Fir_Zoom_FFT_Decimate_Q;
extern arm_fir_interpolate_instance_f32 FIR_int1_I;
extern arm_fir_interpolate_instance_f32 FIR_int1_Q;
extern arm_fir_interpolate_instance_f32 FIR_int2_I;
extern arm_fir_interpolate_instance_f32 FIR_int2_Q;
extern arm_lms_norm_instance_f32 LMS_Norm_instance;
extern arm_lms_instance_f32      LMS_instance;
extern elapsedMicros usec;

struct band {
  long freq;      // Current frequency in Hz * 100
  long fBandLow;  // Lower band edge
  long fBandHigh; // Upper band edge
  const char* name; // name of band
  int mode;
  int FHiCut;
  int FLoCut;
  int RFgain;
  uint8_t band_type;
  float32_t gainCorrection; // is hardware dependent and has to be calibrated ONCE and hardcoded in the band table
  int AGC_thresh;
  int16_t pixel_offset;
};
extern struct band bands[];

extern uint8_t T41State;
extern const uint16_t n_dec1_taps;
extern const uint16_t n_dec2_taps;
extern int bandswitchPins[];
extern volatile int menuEncoderMove;
extern volatile long fineTuneEncoderMove;
extern const float32_t DF;
extern const float32_t DF1;           // decimation factor
extern const float32_t n_att;         // desired stopband attenuation
extern int xrState;
extern uint32_t N_BLOCKS;
extern uint32_t FFT_length;

extern float32_t bin_BW;
extern float32_t biquad_lowpass1_coeffs[];
extern float32_t float_buffer_L[];
extern float32_t float_buffer_R[];
extern float32_t /*DMAMEM*/ iFFT_buffer[];

extern float32_t FIR_dec1_coeffs[];
extern float32_t FIR_dec2_coeffs[];

extern float32_t last_sample_buffer_L[];
extern float32_t last_sample_buffer_R[];

extern float temp;

time_t getTeensy3Time();
