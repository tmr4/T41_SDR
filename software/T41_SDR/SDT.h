#pragma once

#include "T41Config.h"
#define VERSION "sdr_dev.1" // Change this for updates. If you make this longer than 9 characters, brace yourself for surprises

#include <Arduino.h>

#ifndef float32_t
typedef float float32_t;
#endif

#ifndef uint8_t
typedef __uint8_t uint8_t;
#endif

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

// Radio State
#define SSB_RECEIVE_STATE 0
#define SSB_TRANSMIT_STATE 1
#define CW_RECEIVE_STATE 2
#define CW_TRANSMIT_STRAIGHT_STATE 3
#define CW_TRANSMIT_KEYER_STATE 4
#define DATA_RECEIVE_STATE 5
#define CALIBRATE_RECEIVE_STATE 6
#define CALIBRATE_TRANSMIT_STATE 7
#define CALIBRATE_TWOTONE_STATE 8
#define CALIBRATE_DONE_STATE 9

#define CALIBRATE_STATE 5

// demodulation modes
#define DEMOD_MIN                   0
#define DEMOD_USB                   0
#define DEMOD_LSB                   1
#define DEMOD_AM                    2
#define DEMOD_NFM                   3
#define DEMOD_PSK31_WAV             4
#define DEMOD_PSK31                 5
#define DEMOD_FT8_WAV               6
#define DEMOD_FT8                   7
#define DEMOD_SAM                   8
#define DEMOD_MAX                   8

#define NUMBER_OF_BANDS           7
#define BAND_80M                  0
#define BAND_40M                  1
#define BAND_20M                  2
#define BAND_17M                  3
#define BAND_15M                  4
#define BAND_12M                  5
#define BAND_10M                  6

#define SSB_MODE                  0
#define CW_MODE                   1
#define DATA_MODE                 2

#define OFF                       0
#define ON                        1

//---- Global Teensy 4.1 Pin assignments
#define RXTX                        22    // Transmit/Receive
#define KEYER_DAH_INPUT_RING        35    // Ring connection for keyer  -- default for righthanded user
#define KEYER_DIT_INPUT_TIP         36    // Tip connection for keyer
#define MUTE                        38    // Mute Audio,  HIGH = "On" Audio available from Audio PA, LOW = Mute audio
#ifdef PROJECTSYSTEM
#define BUSY_ANALOG_PIN             40    // pin 39 is TFT_MISO on Project System (the pin assigned here is only meaningful when testing switch matrix on non-front panel systems)
#else
#define BUSY_ANALOG_PIN             39    // This is the analog pin that controls the 18 switches
#endif
//---- End Global Teensy 4.1 Pin assignments

#define CLEAR_VAR(x) memset(x, 0, sizeof(x))
#define SET_VAR(x,y) memset(x, y, sizeof(x))

// delete once we get rid of global working variables
#include "gwv.h"

// radio hardware and state global variables

extern int radioState, lastState;  // Used by the loop to monitor current state.

extern float32_t float_buffer_L[];
extern float32_t float_buffer_R[];
extern float32_t float_buffer_L_EX[];
extern float32_t float_buffer_R_EX[];
extern float32_t float_buffer_Temp[];

typedef struct {
  long freq;      // Current frequency in Hz
  long fBandLow;  // Lower band edge
  long fBandHigh; // Upper band edge
  const char* name; // name of band
  int demod;
  int FHiCut;
  int FLoCut;
  int RFgain;
  long calFreq; // receive IQ calibration frequency
  float32_t gainCorrection; // is hardware dependent and has to be calibrated ONCE and hardcoded in the band table
  int AGC_thresh;
  int16_t pixel_offset;
} band;

extern band bands[];

extern int bandswitchPins[];
