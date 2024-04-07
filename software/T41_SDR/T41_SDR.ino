/*********************************************************************************************

  This comment block must appear in the load page (e.g., main() or setup()) in any source code
  that uses code presented as whole or part of the T41-EP source code.

  (c) Frank Dziock, DD4WH, 2020_05_8
  "TEENSY CONVOLUTION SDR" substantially modified by Jack Purdum, W8TEE, and Al Peter, AC8GY

  This software is made available under the GNU GPLv3 license agreement. If commercial use of this
  software is planned, we would appreciate it if the interested parties contact Jack Purdum, W8TEE, 
  and Al Peter, AC8GY.

  Any and all other uses, written or implied, by the GPLv3 license are forbidden without written 
  permission from from Jack Purdum, W8TEE, and Al Peter, AC8GY.

*********************************************************************************************/

// setup() and loop() at the bottom of this file

#include "SDT.h"
#include "Bearing.h"
#include "Button.h"
#include "ButtonProc.h"
#include "CWProcessing.h"
#include "CW_Excite.h"
#include "Display.h"
#include "DSP_Fn.h"
#include "EEPROM.h"
#include "Encoders.h"
#include "Exciter.h"
#include "FFT.h"
#include "Filter.h"
#include "FIR.h"
#include "Freq_Shift.h"
#include "InfoBox.h"
#include "Menu.h"
#include "MenuProc.h"
#include "Noise.h"
#include "Tune.h"
#include "Utility.h"

#include "debug.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

#define BUFFER_SINE_COUNT       8                 // Leads to a 750Hz signal
#define HAM_BAND                1

//======================================== Teensy 4.1 Pin assignments ==========================================================
// Pins 0 and 1 are usually reserved for the USB COM port communications
// On the Teensy 4.1 board, pins GND, 0-12, and pins 13-23, 3.3V, GND, and
// Vin are "covered up" by the Audio board. However, not all of those pins are
// actually used by the board. See: https://www.pjrc.com/store/teensy3_audio.html
#ifdef FOURSQRP
    #define VOLUME_ENCODER_A         2
    #define VOLUME_ENCODER_B         3
    #define FILTER_ENCODER_A        16
    #define FILTER_ENCODER_B        15
    #define FINETUNE_ENCODER_A       4
    #define FINETUNE_ENCODER_B       5
    #define TUNE_ENCODER_A          14
    #define TUNE_ENCODER_B          17
#else
    #define VOLUME_ENCODER_A         2
    #define VOLUME_ENCODER_B         3
    #define FILTER_ENCODER_A        15
    #define FILTER_ENCODER_B        14
    #define FINETUNE_ENCODER_A       4
    #define FINETUNE_ENCODER_B       5
    #define TUNE_ENCODER_A          16
    #define TUNE_ENCODER_B          17
#endif
#define OPTO_OUTPUT                 24    // To optoisolator and keyed circuit
// Filter Board pins
#define FILTERPIN80M                30    // 80M filter relay
#define FILTERPIN40M                31    // 40M filter relay
#define FILTERPIN20M                28    // 20M filter relay
#define FILTERPIN15M                29    // 15M filter relay
#define PTT                         37    // Transmit/Receive
#define BAND_MENUS                 100    // encoder2 button = button3SW
#define BAND_PLUS                  101    // BAND+ = button2SW
#define CHANGE_INCREMENT           102    // this is the pushbutton pin of the tune encoder
#define CHANGE_FILTER              103    // this is the pushbutton pin of the filter encoder
#define CHANGE_MODE                104    // Change mode
#define CHANGE_MENU2               105    // this is the pushbutton pin of encoder 3
#define MENU_MINUS                 106    // Menu decrement
#define MENU_PLUS                  107    // this is the menu button pin
#define CHANGE_NOISE               108    // this is the pushbutton pin of NR
#define CHANGE_DEMOD               109    // this is the push button for demodulation
#define CHANGE_ZOOM                110    // Push button for display zoom feature
#define SET_FREQ_CURSOR            111    // Push button for frequency Cursor feature  was 39 for Al
//======================================== End Pin Assignments =================================================================

//------------------------- Global Variables ----------

int radioState, lastState;
long CWFreqShift;
long calFreqShift;

// SampleRate
//   8000, // SAMPLE_RATE_8K    // not OK
//  11025, // SAMPLE_RATE_11K   // not OK 
//  16000, // SAMPLE_RATE_16K   // OK
//  22050, // SAMPLE_RATE_22K   // OK
//  32000, // SAMPLE_RATE_32K   // OK, on
//  44100, // SAMPLE_RATE_44K   // OK
//  48000, // SAMPLE_RATE_48K   // OK
//  50223, // SAMPLE_RATE_50K   // NOT OK
//  88200, // SAMPLE_RATE_88K   // OK
//  96000, // SAMPLE_RATE_96K   // OK
// 100000, // SAMPLE_RATE_100K  // NOT OK
// 100466, // SAMPLE_RATE_101K  // NOT OK
// 176400, // SAMPLE_RATE_176K  // OK
// 192000, // SAMPLE_RATE_192K  // OK
// 234375, // SAMPLE_RATE_234K  // NOT OK
// 256000, // SAMPLE_RATE_256K  // NOT OK
// 281000, // SAMPLE_RATE_281K  // NOT OK
// 352800  // SAMPLE_RATE_353K  // NOT OK
const int SampleRate = 192000; // SAMPLE_RATE_192K

long NCOFreq;

//------------------------- Local Variables ----------

// *** why initialize here vs setup()
float gain_dB = 0.0; //computed desired gain value in dB
boolean use_HP_filter = true; //enable the software HP filter to get rid of DC?
float knee_dBFS, comp_ratio, attack_sec, release_sec;

//DB2OO, 29-AUG-23: take ITU_REGION into account for band limits
// and changed "gainCorrection" to see the correct dBm value on all bands.
// Calibration done with TinySA as signal generator with -73dBm levels (S9) at the FT8 frequencies
// with V010 QSD with the 12V mod of the pre-amp
// *** seems it would be better to treat hi/low filter values as absolute; changing this is a lot of work though ***
struct band bands[NUMBER_OF_BANDS] = {
//  freq      band low   band hi   name    mode         Hi   Low     Gain  type         gain     AGC   pixel
//                                                       filter                         correct        offset
#if defined(ITU_REGION) && ITU_REGION == 1                                                             
    3700000,  3500000,   3800000,  "80M",  DEMOD_LSB,  -200, -3000,  1,    HAM_BAND,    -2.0,    20,    20,
    7150000,  7000000,   7200000,  "40M",  DEMOD_LSB,  -200, -3000,  1,    HAM_BAND,    -2.0,    20,    20,
#elif defined(ITU_REGION) && ITU_REGION == 2                                                                    
    3700000,  3500000,   4000000,  "80M",  DEMOD_LSB,  -200, -3000,  1,    HAM_BAND,    -2.0,    20,    20,
    7150000,  7000000,   7300000, "40M",   DEMOD_LSB,  -200, -3000,  1,    HAM_BAND,    -2.0,    20,    20,

    //3700000,  3500000,   4000000,  "80M",  DEMOD_LSB,  3000, 200,  1,    HAM_BAND,    -2.0,    20,    20,
    //7150000,  7000000,   7300000, "40M",   DEMOD_LSB,  3000, 200,  1,    HAM_BAND,    -2.0,    20,    20,
    //3700000,  3500000,   4000000,  "80M",  DEMOD_LSB,  200, 3000,  1,    HAM_BAND,    -2.0,    20,    20,
    //7150000,  7000000,   7300000, "40M",   DEMOD_LSB,  200, 3000,  1,    HAM_BAND,    -2.0,    20,    20,
#elif defined(ITU_REGION) && ITU_REGION == 3                                                                    
    3700000,  3500000,   3900000,  "80M",  DEMOD_LSB,  -200, -3000,  1,    HAM_BAND,    -2.0,    20,    20,
    7150000,  7000000,   7200000,  "40M",  DEMOD_LSB,  -200, -3000,  1,    HAM_BAND,    -2.0,    20,    20,
#endif                                                                                                        
    14200000, 14000000, 14350000,  "20M",  DEMOD_USB,  3000, 200,    1,    HAM_BAND,    2.0,     20,    20,
    18100000, 18068000, 18168000,  "17M",  DEMOD_USB,  3000, 200,    1,    HAM_BAND,    2.0,     20,    20,
    21200000, 21000000, 21450000,  "15M",  DEMOD_USB,  3000, 200,    1,    HAM_BAND,    5.0,     20,    20,
    24920000, 24890000, 24990000,  "12M",  DEMOD_USB,  3000, 200,    1,    HAM_BAND,    6.0,     20,    20,
    28350000, 28000000, 29700000,  "10M",  DEMOD_USB,  3000, 200,    1,    HAM_BAND,    8.5,     20,    20
};

uint32_t FFT_length = FFT_LENGTH;

AudioControlSGTL5000_Extended sgtl5000_1;      //controller for the Teensy Audio Board
AudioConvert_I16toF32 int2Float1, int2Float2;  //Converts Int16 to Float.  See class in AudioStream_F32.h
AudioEffectGain_F32 gain1, gain2;              //Applies digital gain to audio data.  Expected Float data.
AudioConvert_F32toI16 float2Int1, float2Int2;  //Converts Float to Int16.  See class in AudioStream_F32.h

AudioInputI2SQuad i2s_quadIn;
AudioOutputI2SQuad i2s_quadOut;

AudioMixer4 modeSelectInR;    // AFP 09-01-22
AudioMixer4 modeSelectInL;    // AFP 09-01-22
AudioMixer4 modeSelectInExR;  // AFP 09-01-22
AudioMixer4 modeSelectInExL;  // AFP 09-01-22

AudioMixer4 modeSelectOutL;    // AFP 09-01-22
AudioMixer4 modeSelectOutR;    // AFP 09-01-22
AudioMixer4 modeSelectOutExL;  // AFP 09-01-22
AudioMixer4 modeSelectOutExR;  // AFP 09-01-22

AudioRecordQueue Q_in_L;
AudioRecordQueue Q_in_R;
AudioRecordQueue Q_in_L_Ex;
AudioRecordQueue Q_in_R_Ex;

AudioPlayQueue Q_out_L;
AudioPlayQueue Q_out_R;
AudioPlayQueue Q_out_L_Ex;
AudioPlayQueue Q_out_R_Ex;

AudioConnection patchCord1(i2s_quadIn, 0, int2Float1, 0);  //connect the Left input to the Left Int->Float converter
AudioConnection patchCord2(i2s_quadIn, 1, int2Float2, 0);  //connect the Right input to the Right Int->Float converter

AudioConnection_F32 patchCord3(int2Float1, 0, comp1, 0);  //Left.  makes Float connections between objects
AudioConnection_F32 patchCord4(int2Float2, 0, comp2, 0);  //Right.  makes Float connections between objects
AudioConnection_F32 patchCord5(comp1, 0, float2Int1, 0);  //Left.  makes Float connections between objects
AudioConnection_F32 patchCord6(comp2, 0, float2Int2, 0);  //Right.  makes Float connections between objects
//AudioConnection_F32     patchCord3(int2Float1, 0, float2Int1, 0); //Left.  makes Float connections between objects
//AudioConnection_F32     patchCord4(int2Float2, 0, float2Int2, 0); //Right.  makes Float connections between objects

AudioConnection patchCord7(float2Int1, 0, modeSelectInExL, 0);  //Input Ex
AudioConnection patchCord8(float2Int2, 0, modeSelectInExR, 0);

AudioConnection patchCord9(i2s_quadIn, 2, modeSelectInL, 0);  //Input Rec
AudioConnection patchCord10(i2s_quadIn, 3, modeSelectInR, 0);

AudioConnection patchCord11(modeSelectInExR, 0, Q_in_R_Ex, 0);  //Ex in Queue
AudioConnection patchCord12(modeSelectInExL, 0, Q_in_L_Ex, 0);

AudioConnection patchCord13(modeSelectInR, 0, Q_in_R, 0);  //Rec in Queue
AudioConnection patchCord14(modeSelectInL, 0, Q_in_L, 0);

AudioConnection patchCord15(Q_out_L_Ex, 0, modeSelectOutExL, 0);  //Ex out Queue
AudioConnection patchCord16(Q_out_R_Ex, 0, modeSelectOutExR, 0);

AudioConnection patchCord17(Q_out_L, 0, modeSelectOutL, 0);  //Rec out Queue
AudioConnection patchCord18(Q_out_R, 0, modeSelectOutR, 0);

AudioConnection patchCord19(modeSelectOutExL, 0, i2s_quadOut, 0);  //Ex out
AudioConnection patchCord20(modeSelectOutExR, 0, i2s_quadOut, 1);
AudioConnection patchCord21(modeSelectOutL, 0, i2s_quadOut, 2);  //Rec out
AudioConnection patchCord22(modeSelectOutR, 0, i2s_quadOut, 3);

AudioConnection patchCord23(Q_out_L_Ex, 0, modeSelectOutL, 1);  //Rec out Queue for sidetone
AudioConnection patchCord24(Q_out_R_Ex, 0, modeSelectOutR, 1);

AudioControlSGTL5000 sgtl5000_2;

Bounce decreaseBand = Bounce(BAND_MENUS, 50);
Bounce increaseBand = Bounce(BAND_PLUS, 50);
Bounce modeSwitch = Bounce(CHANGE_MODE, 50);
Bounce decreaseMenu = Bounce(MENU_MINUS, 50);
Bounce frequencyIncrement = Bounce(CHANGE_INCREMENT, 50);
Bounce filterSwitch = Bounce(CHANGE_FILTER, 50);
Bounce increaseMenu = Bounce(MENU_PLUS, 50);
Bounce selectExitMenues = Bounce(CHANGE_MENU2, 50);
Bounce changeNR = Bounce(CHANGE_NOISE, 50);
Bounce demodSwitch = Bounce(CHANGE_DEMOD, 50);
Bounce zoomSwitch = Bounce(CHANGE_ZOOM, 50);
Bounce cursorSwitch = Bounce(SET_FREQ_CURSOR, 50);
Bounce KeyPin2 = Bounce(KEYER_DAH_INPUT_RING, 5);
Bounce KeyPin1 = Bounce(KEYER_DIT_INPUT_TIP, 5);

Rotary volumeEncoder = Rotary(VOLUME_ENCODER_A, VOLUME_ENCODER_B);        //( 2,  3)
Rotary tuneEncoder = Rotary(TUNE_ENCODER_A, TUNE_ENCODER_B);              //(16, 17)
Rotary menuChangeEncoder = Rotary(FILTER_ENCODER_A, FILTER_ENCODER_B);        //(15, 14)
Rotary fineTuneEncoder = Rotary(FINETUNE_ENCODER_A, FINETUNE_ENCODER_B);  //( 4,  5)

Si5351 si5351;

//Setup for EQ filters

//Hilbert FIR Filters
float32_t FIR_Hilbert_state_L[100 + 256 - 1];
float32_t FIR_Hilbert_state_R[100 + 256 - 1];
// CW decode Filters
float32_t FIR_CW_DecodeL_state[64 + 256 - 1];
float32_t FIR_CW_DecodeR_state[64 + 256 - 1];

arm_fir_interpolate_instance_f32 FIR_int1_EX_I;
arm_fir_interpolate_instance_f32 FIR_int1_EX_Q;
arm_fir_interpolate_instance_f32 FIR_int2_EX_I;
arm_fir_interpolate_instance_f32 FIR_int2_EX_Q;

float32_t DMAMEM FIR_dec1_EX_I_state[2095];
float32_t DMAMEM FIR_dec1_EX_Q_state[2095];

float32_t DMAMEM FIR_dec2_EX_I_state[535];
float32_t DMAMEM FIR_dec2_EX_Q_state[535];

float32_t DMAMEM FIR_int2_EX_I_state[519];
float32_t DMAMEM FIR_int2_EX_Q_state[519];

float32_t DMAMEM FIR_int1_EX_I_state[279];
float32_t DMAMEM FIR_int1_EX_Q_state[279];

float32_t DMAMEM float_buffer_L_EX[2048];
float32_t DMAMEM float_buffer_R_EX[2048];
float32_t DMAMEM float_buffer_Temp[2048];

byte sharedRAM1[1024 * 8];
byte DMAMEM sharedRAM2[2048 * 13] __attribute__ ((aligned (4)));


const arm_cfft_instance_f32 *S;
const arm_cfft_instance_f32 *iS;
const arm_cfft_instance_f32 *maskS;
const arm_cfft_instance_f32 *NR_FFT;
const arm_cfft_instance_f32 *NR_iFFT;
const arm_cfft_instance_f32 *spec_FFT;

arm_biquad_casd_df1_inst_f32 biquad_lowpass1;
arm_biquad_casd_df1_inst_f32 IIR_biquad_Zoom_FFT_I;
arm_biquad_casd_df1_inst_f32 IIR_biquad_Zoom_FFT_Q;

arm_fir_decimate_instance_f32 FIR_dec1_I;
arm_fir_decimate_instance_f32 FIR_dec1_Q;
arm_fir_decimate_instance_f32 FIR_dec2_I;
arm_fir_decimate_instance_f32 FIR_dec2_Q;
arm_fir_decimate_instance_f32 Fir_Zoom_FFT_Decimate_I;
arm_fir_decimate_instance_f32 Fir_Zoom_FFT_Decimate_Q;
arm_fir_interpolate_instance_f32 FIR_int1_I;
arm_fir_interpolate_instance_f32 FIR_int1_Q;
arm_fir_interpolate_instance_f32 FIR_int2_I;
arm_fir_interpolate_instance_f32 FIR_int2_Q;
arm_lms_norm_instance_f32 LMS_Norm_instance;
arm_lms_instance_f32 LMS_instance;

const float32_t DF1 = 4.0;             // decimation factor
const float32_t DF2 = 2.0;             // decimation factor
const float32_t DF = DF1 * DF2;        // decimation factor
const float32_t n_att = 90.0;        // need here for later def's
const float32_t n_desired_BW = 9.0;  // desired max BW of the filters
const float32_t n_samplerate = 176.0;  // samplerate before decimation
const float32_t n_fpass1 = n_desired_BW / n_samplerate;
const float32_t n_fpass2 = n_desired_BW / (n_samplerate / DF1);
const float32_t n_fstop1 = ((n_samplerate / DF1) - n_desired_BW) / n_samplerate;
const float32_t n_fstop2 = ((n_samplerate / (DF1 * DF2)) - n_desired_BW) / (n_samplerate / DF1);

const uint16_t n_dec1_taps = (1 + (uint16_t)(n_att / (22.0 * (n_fstop1 - n_fpass1))));
const uint16_t n_dec2_taps = (1 + (uint16_t)(n_att / (22.0 * (n_fstop2 - n_fpass2))));

uint8_t T41State = 1;
int16_t spectrum_height = 96;
const uint32_t IIR_biquad_Zoom_FFT_N_stages = 4;
const uint32_t N_stages_biquad_lowpass1 = 1;
int bandswitchPins[] = {
  FILTERPIN80M,  // 80M
  FILTERPIN40M,  // 40M
  FILTERPIN20M,  // 20M
  FILTERPIN15M,  // 17M
  FILTERPIN15M,  // 15M
  0,   // 12M  Note that 12M and 10M both use the 10M filter, which is always in (no relay).  KF5N September 27, 2023.
  0    // 10M
};
volatile int menuEncoderMove = 0;
volatile long fineTuneEncoderMove = 0L;
int xrState;  // T41 xmit/rec state: 1 = rec, 0 = xmt *** this seems duplicate ***

unsigned long cwTimer;
unsigned long ditTimerOn;
uint16_t temp_check_frequency;

const uint32_t N_B = FFT_LENGTH / 2 / BUFFER_SIZE * (uint32_t)DF;
uint32_t N_BLOCKS = N_B;
long favoriteFrequencies[13];

float32_t bin_BW = 1.0 / (DF * FFT_length) * SampleRate;
float32_t biquad_lowpass1_state[N_stages_biquad_lowpass1 * 4];
float32_t biquad_lowpass1_coeffs[5 * N_stages_biquad_lowpass1] = { 0, 0, 0, 0, 0 };
float32_t DMAMEM float_buffer_L[BUFFER_SIZE * N_B];
float32_t DMAMEM float_buffer_R[BUFFER_SIZE * N_B];
float32_t DMAMEM iFFT_buffer[FFT_LENGTH * 2 + 1];
float32_t IIR_biquad_Zoom_FFT_I_state[IIR_biquad_Zoom_FFT_N_stages * 4];
float32_t IIR_biquad_Zoom_FFT_Q_state[IIR_biquad_Zoom_FFT_N_stages * 4];

float temp;

// *** moving these to associated files causes array size not an integer error ***
const int INT1_STATE_SIZE = 24 + BUFFER_SIZE * N_B / (uint32_t)DF - 1;
const int INT2_STATE_SIZE = 8 + BUFFER_SIZE * N_B / (uint32_t)DF1 - 1;
const int DEC2STATESIZE = n_dec2_taps + (BUFFER_SIZE * N_B / (uint32_t)DF1) - 1;

float32_t DMAMEM FIR_dec1_I_state[n_dec1_taps + (uint16_t)BUFFER_SIZE * (uint32_t)N_B - 1];
float32_t DMAMEM FIR_dec2_I_state[DEC2STATESIZE];
float32_t DMAMEM FIR_dec2_Q_state[DEC2STATESIZE];
float32_t DMAMEM FIR_int2_I_state[INT2_STATE_SIZE];
float32_t DMAMEM FIR_int2_Q_state[INT2_STATE_SIZE];
float32_t DMAMEM FIR_dec1_Q_state[n_dec1_taps + (uint16_t)BUFFER_SIZE * (uint16_t)N_B - 1];
float32_t DMAMEM FIR_int1_I_state[INT1_STATE_SIZE];
float32_t DMAMEM FIR_int1_Q_state[INT1_STATE_SIZE];
float32_t DMAMEM Fir_Zoom_FFT_Decimate_I_state[4 + BUFFER_SIZE * N_B - 1];
float32_t DMAMEM Fir_Zoom_FFT_Decimate_Q_state[4 + BUFFER_SIZE * N_B - 1];
float32_t DMAMEM FIR_dec1_coeffs[n_dec1_taps]; // have to include these here to avoid "size of array is not an integral constant-expression" error
float32_t DMAMEM FIR_dec2_coeffs[n_dec2_taps];

const uint32_t N_DEC_B = N_B / (uint32_t)DF;

float32_t DMAMEM last_sample_buffer_L[BUFFER_SIZE * N_DEC_B];
float32_t DMAMEM last_sample_buffer_R[BUFFER_SIZE * N_DEC_B];

//-------------------------------------------------------------------------------------------------------------
// Forwards
//-------------------------------------------------------------------------------------------------------------

void Splash();

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

/*****
  Purpose: To read the local time

  Parameter list:
    void

  Return value:
    time_t                a time data point
*****/
time_t getTeensy3Time() {
  return Teensy3Clock.get();
}

//#pragma GCC diagnostic ignored "-Wunused-variable"

// is added in Teensyduino 1.52 beta-4, so this can be deleted !?

/*****
  Purpose: To set the real time clock

  Parameter list:
    void

  Return value:
    void
*****/
FLASHMEM void T4_rtc_set(unsigned long t) {
  //#if defined (T4)
#if 0
  // stop the RTC
  SNVS_HPCR &= ~(SNVS_HPCR_RTC_EN | SNVS_HPCR_HP_TS);
  while (SNVS_HPCR & SNVS_HPCR_RTC_EN); // wait
  // stop the SRTC
  SNVS_LPCR &= ~SNVS_LPCR_SRTC_ENV;
  while (SNVS_LPCR & SNVS_LPCR_SRTC_ENV); // wait
  // set the SRTC
  SNVS_LPSRTCLR = t << 15;
  SNVS_LPSRTCMR = t >> 17;
  // start the SRTC
  SNVS_LPCR |= SNVS_LPCR_SRTC_ENV;
  while (!(SNVS_LPCR & SNVS_LPCR_SRTC_ENV)); // wait
  // start the RTC and sync it to the SRTC
  SNVS_HPCR |= SNVS_HPCR_RTC_EN | SNVS_HPCR_HP_TS;
#endif
}


// Teensy 4.0, 4.1
/*****
  Purpose: to collect array inits in one place

  Parameter list:
    void

  Return value:
    void
*****/
FLASHMEM void InitializeDataArrays() {
  int LP_F_help;

  //DB2OO, 11-SEP-23: don't use the fixed sizes, but use the caculated ones, otherwise a code change will create very difficult to find problems
#define CLEAR_VAR(x) memset(x, 0, sizeof(x))

  CLEAR_VAR(buffer_spec_FFT);
  CLEAR_VAR(FFT_spec);
  CLEAR_VAR(FFT_spec_old);
  CLEAR_VAR(pixelnew);
  CLEAR_VAR(pixelold);
  CLEAR_VAR(pixelCurrent);
  CLEAR_VAR(NR_FFT_buffer);
  CLEAR_VAR(NR_output_audio_buffer);
  CLEAR_VAR(NR_last_iFFT_result);
  CLEAR_VAR(NR_last_sample_buffer_L);
  CLEAR_VAR(NR_last_sample_buffer_R);
  CLEAR_VAR(NR_M);
  CLEAR_VAR(NR_lambda);
  CLEAR_VAR(NR_G);
  CLEAR_VAR(NR_SNR_prio);
  CLEAR_VAR(NR_SNR_post);
  CLEAR_VAR(NR_Hk_old);
  CLEAR_VAR(NR_X);
  CLEAR_VAR(NR_Nest);
  CLEAR_VAR(NR_Gts);
  CLEAR_VAR(NR_E);
  CLEAR_VAR(ANR_d);
  CLEAR_VAR(ANR_w);
  CLEAR_VAR(LMS_StateF32);
  CLEAR_VAR(LMS_NormCoeff_f32);
  CLEAR_VAR(LMS_nr_delay);

  /****************************************************************************************
     set filter bandwidth
  ****************************************************************************************/
  // *** this is going to get done in call to CalcFilters() in setup.  Do we really need it here as well? ***
  CalcCplxFIRCoeffs(FIR_Coef_I, FIR_Coef_Q, m_NumTaps, (float32_t)bands[currentBand].FLoCut, (float32_t)bands[currentBand].FHiCut, (float)SampleRate / DF);

  /****************************************************************************************
     init complex FFTs
  ****************************************************************************************/
  switch (FFT_length) {
    case 2048:
      S = &arm_cfft_sR_f32_len2048;
      iS = &arm_cfft_sR_f32_len2048;
      maskS = &arm_cfft_sR_f32_len2048;
      break;
    case 1024:
      S = &arm_cfft_sR_f32_len1024;
      iS = &arm_cfft_sR_f32_len1024;
      maskS = &arm_cfft_sR_f32_len1024;
      break;
    case 512:
      S = &arm_cfft_sR_f32_len512;
      iS = &arm_cfft_sR_f32_len512;
      maskS = &arm_cfft_sR_f32_len512;
      break;
  }

  spec_FFT = &arm_cfft_sR_f32_len512;  //Changed specification to 512 instance
  NR_FFT = &arm_cfft_sR_f32_len256;
  NR_iFFT = &arm_cfft_sR_f32_len256;

  /****************************************************************************************
     Calculate the FFT of the FIR filter coefficients once to produce the FIR filter mask
  ****************************************************************************************/
  InitFilterMask();

  /****************************************************************************************
     Set sample rate
  ****************************************************************************************/
  SetI2SFreq(SampleRate);

  biquad_lowpass1.numStages = N_stages_biquad_lowpass1;  // set number of stages
  biquad_lowpass1.pCoeffs = biquad_lowpass1_coeffs;      // set pointer to coefficients file

  for (unsigned i = 0; i < 4 * N_stages_biquad_lowpass1; i++) {
    biquad_lowpass1_state[i] = 0.0;  // set state variables to zero
  }
  biquad_lowpass1.pState = biquad_lowpass1_state;  // set pointer to the state variables

  /****************************************************************************************
     set filter bandwidth of IIR filter
  ****************************************************************************************/
  // also adjust IIR AM filter
  // calculate IIR coeffs
  LP_F_help = bands[currentBand].FHiCut;
  if (LP_F_help < -bands[currentBand].FLoCut)
    LP_F_help = -bands[currentBand].FLoCut;
  SetIIRCoeffs((float32_t)LP_F_help, 1.3, (float32_t)SampleRate / DF, 0);  // 1st stage
  for (int i = 0; i < 5; i++) {                                                     // fill coefficients into the right file
    biquad_lowpass1_coeffs[i] = coefficient_set[i];
  }

  /****************************************************************************************
     Initiate decimation and interpolation FIR filters
  ****************************************************************************************/
  // Decimation filter 1, M1 = DF1
  CalcFIRCoeffs(FIR_dec1_coeffs, n_dec1_taps, (float32_t)(n_desired_BW * 1000.0), n_att, 0, 0.0, (float32_t)SampleRate);

  if(arm_fir_decimate_init_f32(&FIR_dec1_I, n_dec1_taps, (uint32_t)DF1, FIR_dec1_coeffs, FIR_dec1_I_state, BUFFER_SIZE * N_BLOCKS)) {
    while(1);
  }

  if(arm_fir_decimate_init_f32(&FIR_dec1_Q, n_dec1_taps, (uint32_t)DF1, FIR_dec1_coeffs, FIR_dec1_Q_state, BUFFER_SIZE * N_BLOCKS)) {
    while(1);
  }

  // Decimation filter 2, M2 = DF2
  CalcFIRCoeffs(FIR_dec2_coeffs, n_dec2_taps, (float32_t)(n_desired_BW * 1000.0), n_att, 0, 0.0, (float32_t)(SampleRate / DF1));
  if(arm_fir_decimate_init_f32(&FIR_dec2_I, n_dec2_taps, (uint32_t)DF2, FIR_dec2_coeffs, FIR_dec2_I_state, BUFFER_SIZE * N_BLOCKS / (uint32_t)DF1)) {
    while(1);
  }

  if(arm_fir_decimate_init_f32(&FIR_dec2_Q, n_dec2_taps, (uint32_t)DF2, FIR_dec2_coeffs, FIR_dec2_Q_state, BUFFER_SIZE * N_BLOCKS / (uint32_t)DF1)) {
    while(1);
  }

  // Interpolation filter 1, L1 = 2
  // not sure whether I should design with the final sample rate ??
  // yes, because the interpolation filter is AFTER the upsampling, so it has to be in the target sample rate!
  CalcFIRCoeffs(FIR_int1_coeffs, 48, (float32_t)(n_desired_BW * 1000.0), n_att, 0, 0.0, SampleRate / 4.0);
  //    if(arm_fir_interpolate_init_f32(&FIR_int1_I, (uint32_t)DF2, 16, FIR_int1_coeffs, FIR_int1_I_state, BUFFER_SIZE * N_BLOCKS / (uint32_t)DF)) {
  if(arm_fir_interpolate_init_f32(&FIR_int1_I, (uint8_t)DF2, 48, FIR_int1_coeffs, FIR_int1_I_state, BUFFER_SIZE * N_BLOCKS / (uint32_t)DF)) {
    while(1);
  }
  //    if(arm_fir_interpolate_init_f32(&FIR_int1_Q, (uint32_t)DF2, 16, FIR_int1_coeffs, FIR_int1_Q_state, BUFFER_SIZE * N_BLOCKS / (uint32_t)DF)) {
  if(arm_fir_interpolate_init_f32(&FIR_int1_Q, (uint8_t)DF2, 48, FIR_int1_coeffs, FIR_int1_Q_state, BUFFER_SIZE * N_BLOCKS / (uint32_t)DF)) {
    while(1);
  }

  // Interpolation filter 2, L2 = 4
  // not sure whether I should design with the final sample rate ??
  // yes, because the interpolation filter is AFTER the upsampling, so it has to be in the target sample rate!
  CalcFIRCoeffs(FIR_int2_coeffs, 32, (float32_t)(n_desired_BW * 1000.0), n_att, 0, 0.0, (float32_t)SampleRate);

  if(arm_fir_interpolate_init_f32(&FIR_int2_I, (uint8_t)DF1, 32, FIR_int2_coeffs, FIR_int2_I_state, BUFFER_SIZE * N_BLOCKS / (uint32_t)DF1)) {
    while (1);
  }
  //    if(arm_fir_interpolate_init_f32(&FIR_int2_Q, (uint32_t)DF1, 16, FIR_int2_coeffs, FIR_int2_Q_state, BUFFER_SIZE * N_BLOCKS / (uint32_t)DF1)) {
  if(arm_fir_interpolate_init_f32(&FIR_int2_Q, (uint8_t)DF1, 32, FIR_int2_coeffs, FIR_int2_Q_state, BUFFER_SIZE * N_BLOCKS / (uint32_t)DF1)) {
    while (1);
  }

  SetDecIntFilters();  // here, the correct bandwidths are calculated and set accordingly

  /****************************************************************************************
     Zoom FFT: Initiate decimation and interpolation FIR filters AND IIR filters
  ****************************************************************************************/
  float32_t Fstop_Zoom = 0.5 * (float32_t)SampleRate / (1 << spectrum_zoom);

  CalcFIRCoeffs(Fir_Zoom_FFT_Decimate_coeffs, 4, Fstop_Zoom, 60, 0, 0.0, (float32_t)SampleRate);

  // Attention: max decimation rate is 128 !
  //  if (arm_fir_decimate_init_f32(&Fir_Zoom_FFT_Decimate_I, 4, 1 << spectrum_zoom, Fir_Zoom_FFT_Decimate_coeffs, Fir_Zoom_FFT_Decimate_I_state, BUFFER_SIZE * N_BLOCKS)) {
  if (arm_fir_decimate_init_f32(&Fir_Zoom_FFT_Decimate_I, 4, 128, Fir_Zoom_FFT_Decimate_coeffs, Fir_Zoom_FFT_Decimate_I_state, BUFFER_SIZE * N_BLOCKS)) {
    while (1)
      ;
  }
  // same coefficients, but specific state variables
  //  if (arm_fir_decimate_init_f32(&Fir_Zoom_FFT_Decimate_Q, 4, 1 << spectrum_zoom, Fir_Zoom_FFT_Decimate_coeffs, Fir_Zoom_FFT_Decimate_Q_state, BUFFER_SIZE * N_BLOCKS)) {
  if (arm_fir_decimate_init_f32(&Fir_Zoom_FFT_Decimate_Q, 4, 128, Fir_Zoom_FFT_Decimate_coeffs, Fir_Zoom_FFT_Decimate_Q_state, BUFFER_SIZE * N_BLOCKS)) {
    while (1)
      ;
  }

  IIR_biquad_Zoom_FFT_I.numStages = IIR_biquad_Zoom_FFT_N_stages;  // set number of stages
  IIR_biquad_Zoom_FFT_Q.numStages = IIR_biquad_Zoom_FFT_N_stages;  // set number of stages
  for (unsigned i = 0; i < 4 * IIR_biquad_Zoom_FFT_N_stages; i++) {
    IIR_biquad_Zoom_FFT_I_state[i] = 0.0;  // set state variables to zero
    IIR_biquad_Zoom_FFT_Q_state[i] = 0.0;  // set state variables to zero
  }
  IIR_biquad_Zoom_FFT_I.pState = IIR_biquad_Zoom_FFT_I_state;  // set pointer to the state variables
  IIR_biquad_Zoom_FFT_Q.pState = IIR_biquad_Zoom_FFT_Q_state;  // set pointer to the state variables

  // this sets the coefficients for the ZoomFFT decimation filter
  // according to the desired magnification mode
  // for 0 the mag_coeffs will a NULL  ptr, since the filter is not going to be used in this  mode!
  IIR_biquad_Zoom_FFT_I.pCoeffs = mag_coeffs[spectrum_zoom];
  IIR_biquad_Zoom_FFT_Q.pCoeffs = mag_coeffs[spectrum_zoom];

  ZoomFFTPrep();

  SpectralNoiseReductionInit();
  InitLMSNoiseReduction();

  temp_check_frequency = 0x03U;  //updates the temp value at a RTC/3 clock rate
  //0xFFFF determines a 2 second sample rate period

  //initTempMon(temp_check_frequency, lowAlarmTemp, highAlarmTemp, panicAlarmTemp);
  initTempMon(temp_check_frequency, 25U, 85U, 90U);  // 85U = 42 degrees C?
  // this starts the measurements
  TEMPMON_TEMPSENSE0 |= 0x2U;
}


/*****
  Purpose: The initial screen display on startup. Expect this to be customized

  Parameter list:
    void

  Return value:
    void
*****/
FLASHMEM void Splash() {
  int centerTxt;
  int line1_Y = YPIXELS / 10;
  int line2_Y = YPIXELS / 3;
  int line3_Y = line1_Y + 55;
  int line4_Y = YPIXELS / 4 + 80;
  int line5_Y = line4_Y + 40;
  int line6_Y = YPIXELS / 2 + 110;
  int line7_Y = line6_Y + 50;

  // 50 char max for 800x480 display with font scale = 1:
  //                     "          1         2         3         4"
  //                     "01234567890123456789012345678901234567890123456789";
  const char*line1Txt = "T41-EP Receiver";
  const char*line2Txt = "By: Terrance Robertson, KN6ZDE";
  const char*line3Txt = "Version: "; // + VERSION
  const char*line4Txt = "Based on the T41-EP code by:";
  const char*line5Txt = "Al Peter, AC8GY and Jack Purdum, W8TEE";
  const char*line6Txt = "Property of:"; // line 7 MY_CALL

  tft.fillWindow(RA8875_BLACK);

  tft.setFontScale(3);
  tft.setTextColor(RA8875_GREEN);
  centerTxt = (XPIXELS - strlen(line1Txt) * tft.getFontWidth()) / 2;
  tft.setCursor(centerTxt, line1_Y);
  tft.print(line1Txt);

  tft.setFontScale(1);
  tft.setTextColor(RA8875_YELLOW);
  centerTxt = (XPIXELS - strlen(line2Txt) * tft.getFontWidth()) / 2;
  tft.setCursor(centerTxt, line2_Y);
  tft.print(line2Txt);

  tft.setFontScale(2);
  tft.setTextColor(RA8875_MAGENTA);
  centerTxt = (XPIXELS - (strlen(line3Txt) + strlen(VERSION)) * tft.getFontWidth()) / 2;
  tft.setCursor(centerTxt, line3_Y);
  tft.print("Version: ");
  tft.print(VERSION);

  tft.setFontScale(1);
  tft.setTextColor(RA8875_WHITE);
  centerTxt = (XPIXELS - strlen(line4Txt) * tft.getFontWidth()) / 2;
  tft.setCursor(centerTxt, line4_Y);
  tft.print(line4Txt);
  centerTxt = (XPIXELS - strlen(line5Txt) * tft.getFontWidth()) / 2;
  tft.setCursor(centerTxt, line5_Y);
  tft.print(line5Txt);

  centerTxt = (XPIXELS - strlen(line6Txt) * tft.getFontWidth()) / 2;
  tft.setCursor(centerTxt, line6_Y);
  tft.print(line6Txt);

  tft.setFontScale(2);
  tft.setTextColor(RA8875_GREEN);
  centerTxt = (XPIXELS - strlen(MY_CALL) * tft.getFontWidth()) / 2;
  tft.setCursor(centerTxt, line7_Y);
  tft.print(MY_CALL);

  delay(1000);
  //delay(SPLASH_DELAY);
  tft.fillWindow(RA8875_BLACK);
}

/*****
  Purpose: perform a soft reset of the radio
              This resets the user modifiable radio settings to the startup state

  Parameter list:
    void

  Return value:
    void
*****/
FLASHMEM void SoftReset() {
  // can't use any working variables until after this, we can get rid of this when we use EEPROMData
  LoadOpVars();

  SetKeyPowerUp();  // Use keyType and paddleFlip to configure key GPIs
  SetDitLength(currentWPM);
  SetTransmitDitLength(currentWPM);
  CWFreqShift = 750;
  calFreqShift = 0;
  menuEncoderMove = 0;
  fineTuneEncoderMove = 0L;
  xrState = RECEIVE_STATE;  // Enter loop() in receive state

  initCW();

  mainMenuIndex = 0;             // Changed from middle to first. Do Menu Down to get to Calibrate quickly
  secondaryMenuIndex = -1;       // -1 means haven't determined secondary menu
  menuStatus = NO_MENUS_ACTIVE;  // Blank menu field

  knee_dBFS = -15.0;   // Is this variable actually used???
  comp_ratio = 5.0;
  attack_sec = .1;
  release_sec = 2.0;
  comp1.setPreGain_dB(-10);  //set the gain of the Left-channel gain processor
  comp2.setPreGain_dB(-10);  //set the gain of the Right-channel gain processor

  // set T41 last state different from radio state indicating a state change
  // so receiver will be configured on the first pass through loop()
  lastState = -1;

  // the following items in addition to the radio state change
  // are sufficient to fully draw the display
  DrawStaticDisplayItems();
  ShowOperatingStats();
  ShowSpectrumdBScale();
  ShowBandwidthBarValues();
  UpdateInfoBox();

  AGCPrep(); // no audio without this unless AGC is off

  NCOFreq = 0;
  ResetTuning();
  SetBandRelay(HIGH);
}

/*****
  Purpose: program entry point that sets the environment for program

  Parameter list:
    void

  Return value:
    void
*****/
FLASHMEM void setup() {
  Serial.begin(9600);

  setSyncProvider(getTeensy3Time);  // get TIME from real time clock with 3V backup battery
  setTime(now());
  Teensy3Clock.set(now());  // set the RTC
  T4_rtc_set(Teensy3Clock.get());

  // Enable the audio shield. select input. and enable output
  sgtl5000_1.setAddress(LOW);
  sgtl5000_1.enable();
  AudioMemory(500);
  AudioMemory_F32(10);
  sgtl5000_1.inputSelect(AUDIO_INPUT_MIC);
  sgtl5000_1.micGain(20);
  sgtl5000_1.lineInLevel(0);
  sgtl5000_1.lineOutLevel(20);
  sgtl5000_1.adcHighPassFilterDisable();  //reduces noise.  https://forum.pjrc.com/threads/27215-24-bit-audio-boards?p=78831&viewfull=1#post78831
  sgtl5000_2.setAddress(HIGH);
  sgtl5000_2.enable();
  sgtl5000_2.inputSelect(AUDIO_INPUT_LINEIN);
  sgtl5000_2.volume(0.5);

  pinMode(FILTERPIN15M, OUTPUT);
  pinMode(FILTERPIN20M, OUTPUT);
  pinMode(FILTERPIN40M, OUTPUT);
  pinMode(FILTERPIN80M, OUTPUT);
  pinMode(RXTX, OUTPUT);
  pinMode(MUTE, OUTPUT);
  digitalWrite(MUTE, LOW);
  pinMode(PTT, INPUT_PULLUP);
  pinMode(BUSY_ANALOG_PIN, INPUT);
  pinMode(FILTER_ENCODER_A, INPUT);
  pinMode(FILTER_ENCODER_B, INPUT);
  pinMode(OPTO_OUTPUT, OUTPUT);
  pinMode(KEYER_DIT_INPUT_TIP, INPUT_PULLUP);
  pinMode(KEYER_DAH_INPUT_RING, INPUT_PULLUP);
  pinMode(TFT_MOSI, OUTPUT);
  digitalWrite(TFT_MOSI, HIGH);
  pinMode(TFT_SCLK, OUTPUT);
  digitalWrite(TFT_SCLK, HIGH);
  pinMode(TFT_CS, OUTPUT);
  digitalWrite(TFT_CS, HIGH);

  arm_fir_init_f32(&FIR_Hilbert_L, 100, FIR_Hilbert_coeffs_45, FIR_Hilbert_state_L, 256);
  arm_fir_init_f32(&FIR_Hilbert_R, 100, FIR_Hilbert_coeffs_neg45, FIR_Hilbert_state_R, 256);
  arm_fir_init_f32(&FIR_CW_DecodeL, 64, CW_Filter_Coeffs2, FIR_CW_DecodeL_state, 256);  //AFP 10-25-22
  arm_fir_init_f32(&FIR_CW_DecodeR, 64, CW_Filter_Coeffs2, FIR_CW_DecodeR_state, 256);
  arm_fir_decimate_init_f32(&FIR_dec1_EX_I, 48, 4, coeffs192K_10K_LPF_FIR, FIR_dec1_EX_I_state, 2048);
  arm_fir_decimate_init_f32(&FIR_dec1_EX_Q, 48, 4, coeffs192K_10K_LPF_FIR, FIR_dec1_EX_Q_state, 2048);
  arm_fir_decimate_init_f32(&FIR_dec2_EX_I, 24, 2, coeffs48K_8K_LPF_FIR, FIR_dec2_EX_I_state, 512);
  arm_fir_decimate_init_f32(&FIR_dec2_EX_Q, 24, 2, coeffs48K_8K_LPF_FIR, FIR_dec2_EX_Q_state, 512);
  arm_fir_interpolate_init_f32(&FIR_int1_EX_I, 2, 48, coeffs48K_8K_LPF_FIR, FIR_int1_EX_I_state, 256);
  arm_fir_interpolate_init_f32(&FIR_int1_EX_Q, 2, 48, coeffs48K_8K_LPF_FIR, FIR_int1_EX_Q_state, 256);
  arm_fir_interpolate_init_f32(&FIR_int2_EX_I, 4, 32, coeffs192K_10K_LPF_FIR, FIR_int2_EX_I_state, 512);
  arm_fir_interpolate_init_f32(&FIR_int2_EX_Q, 4, 32, coeffs192K_10K_LPF_FIR, FIR_int2_EX_Q_state, 512);

  //***********************  EQ Gain Settings ************
  uint32_t iospeed_display = IOMUXC_PAD_DSE(3) | IOMUXC_PAD_SPEED(1);
  *(digital_pin_to_info_PGM + 13)->pad = iospeed_display;  //clk
  *(digital_pin_to_info_PGM + 11)->pad = iospeed_display;  //MOSI
  *(digital_pin_to_info_PGM + TFT_CS)->pad = iospeed_display;

  tuneEncoder.begin(true);
  volumeEncoder.begin(true);
  attachInterrupt(digitalPinToInterrupt(VOLUME_ENCODER_A), EncoderVolume, CHANGE);
  attachInterrupt(digitalPinToInterrupt(VOLUME_ENCODER_B), EncoderVolume, CHANGE);
  menuChangeEncoder.begin(true);
  attachInterrupt(digitalPinToInterrupt(FILTER_ENCODER_A), EncoderMenuChangeFilter, CHANGE);
  attachInterrupt(digitalPinToInterrupt(FILTER_ENCODER_B), EncoderMenuChangeFilter, CHANGE);
  fineTuneEncoder.begin(true);
  attachInterrupt(digitalPinToInterrupt(FINETUNE_ENCODER_A), EncoderFineTune, CHANGE);
  attachInterrupt(digitalPinToInterrupt(FINETUNE_ENCODER_B), EncoderFineTune, CHANGE);
  attachInterrupt(digitalPinToInterrupt(KEYER_DIT_INPUT_TIP), KeyTipOn, CHANGE);
  attachInterrupt(digitalPinToInterrupt(KEYER_DAH_INPUT_RING), KeyRingOn, CHANGE);

  tft.begin(RA8875_800x480, 8, 20000000UL, 4000000UL);  // parameter list from library code
  tft.setRotation(0);

  // Setup for scrolling attributes. Part of initSpectrum_RA8875() call written by Mike Lewis
  tft.useLayers(true); // mainly used to turn on layers
  tft.layerEffect(OR); // overlay layers
  tft.writeTo(L2);
  tft.clearMemory();
  tft.writeTo(L1);
  tft.clearMemory();

  Splash();

  sdCardPresent = InitializeSDCard();  // Is there an SD card that can be initialized?

  EEPROMStartup();

#ifdef DEBUG
  EEPROMShow();
#endif

  // Enable switch matrix button interrupts (from T41EEE.3)
  EnableButtonInterrupts();

  Q_in_L.begin();  //Initialize receive input buffers
  Q_in_R.begin();
  Q_out_L.setBehaviour(AudioPlayQueue::NON_STALLING); // NON_STALLING
  delay(100L);

  /****************************************************************************************
     start local oscillator Si5351
  ****************************************************************************************/
  si5351.reset();
  si5351.init(SI5351_CRYSTAL_LOAD_10PF, Si_5351_crystal, freqCorrectionFactor);
  si5351.set_ms_source(SI5351_CLK2, SI5351_PLLB); //  Allows CLK1 and CLK2 to exceed 100 MHz simultaneously.
  si5351.drive_strength(SI5351_CLK1, SI5351_DRIVE_8MA);
  si5351.drive_strength(SI5351_CLK2, SI5351_DRIVE_8MA);

  InitializeDataArrays();

  sineTone(BUFFER_SINE_COUNT);  // Set to 8

  sdCardPresent = SDPresentCheck();

  SoftReset();

  //memCheck = true;
}

elapsedMicros usec = 0;  // Automatically increases as time passes; no ++ necessary.

/*****
  Purpose: Code here executes forever, or until: 1) power is removed, 2) user does a reset, 3) a component
           fails, or 4) the cows come home.

  Parameter list:
    void

  Return value:
    void
*****/
FASTRUN void loop()  // Replaced entire loop() with Greg's code  JJP  7/14/23
{
  int pushButtonSwitchIndex = -1;
  int valPin;
  long ditTimerOff;
  long dahTimerOn;

  if(memCheck) {
    if(++loopCounter == 100) {
      memInfo();
      loopCounter = 0;
    }
  }

#ifdef DEBUG_LOOP
  EnterLoop();
#endif

  valPin = ReadSelectedPushButton();                     // Poll UI push buttons
  if (valPin != BOGUS_PIN_READ) {                        // If a button was pushed...
    pushButtonSwitchIndex = ProcessButtonPress(valPin);  // Winner, winner...chicken dinner!
    ExecuteButtonPress(pushButtonSwitchIndex);
  }

#ifdef DEBUG_LOOP
  ButtonInfoOut(valPin, pushButtonSwitchIndex);
#endif

  //  State detection
  if(xmtMode == SSB_MODE && digitalRead(PTT) == HIGH) {
    radioState = SSB_RECEIVE_STATE;
  }
  if(xmtMode == SSB_MODE && digitalRead(PTT) == LOW) {
    radioState = SSB_TRANSMIT_STATE;
  }
  if(xmtMode == CW_MODE && (digitalRead(paddleDit) == HIGH && digitalRead(paddleDah) == HIGH)) {
    radioState = CW_RECEIVE_STATE;  // Was using symbolic constants. Also changed in code below.  KF5N August 8, 2023
  }
  if(xmtMode == CW_MODE && (digitalRead(paddleDit) == LOW && keyType == 0)) {
    radioState = CW_TRANSMIT_STRAIGHT_STATE;
  }
  if(xmtMode == CW_MODE && (keyPressedOn == 1 && keyType == 1)) {
    radioState = CW_TRANSMIT_KEYER_STATE;
  }

  if(lastState != radioState) {
    SetFreq();  // Update frequencies if the radio state has changed.
  }

  //  Begin radio state machines

  //  Begin SSB Mode state machine

  switch (radioState) {
    case (SSB_RECEIVE_STATE):
      if (lastState != radioState) {
        digitalWrite(MUTE, LOW);      // Audio Mute off
        modeSelectInR.gain(0, 1);
        modeSelectInL.gain(0, 1);
        digitalWrite(RXTX, LOW);  //xmit off
        T41State = SSB_RECEIVE;
        xrState = RECEIVE_STATE;
        modeSelectInR.gain(0, 1);
        modeSelectInL.gain(0, 1);
        modeSelectInExR.gain(0, 0);
        modeSelectInExL.gain(0, 0);
        modeSelectOutL.gain(0, 1);
        modeSelectOutR.gain(0, 1);
        modeSelectOutL.gain(1, 0);
        modeSelectOutR.gain(1, 0);
        modeSelectOutExL.gain(0, 0);
        modeSelectOutExR.gain(0, 0);
        if (keyPressedOn == 1) {
          return;
        }
        ShowTransmitReceiveStatus();
      }
      ShowSpectrum();
      //delay(150);
      break;
    case SSB_TRANSMIT_STATE:
      Q_in_L.end();  //Set up input Queues for transmit
      Q_in_R.end();
      Q_in_L_Ex.begin();
      Q_in_R_Ex.begin();
      comp1.setPreGain_dB(currentMicGain);
      comp2.setPreGain_dB(currentMicGain);
      if (compressorFlag == 1) {
        SetupMyCompressors(use_HP_filter, (float)currentMicThreshold, comp_ratio, attack_sec, release_sec);  // Cast currentMicThreshold to float.  KF5N, October 31, 2023
      } else {
        if (compressorFlag == 0) {
          SetupMyCompressors(use_HP_filter, 0.0, comp_ratio, 0.01, 0.01);
        }
      }
      xrState = TRANSMIT_STATE;
      digitalWrite(MUTE, HIGH);  //  Mute Audio  (HIGH=Mute)
      digitalWrite(RXTX, HIGH);  //xmit on
      xrState = TRANSMIT_STATE;
      modeSelectInR.gain(0, 0);
      modeSelectInL.gain(0, 0);
      modeSelectInExR.gain(0, 1);
      modeSelectInExL.gain(0, 1);
      modeSelectOutL.gain(0, 0);
      modeSelectOutR.gain(0, 0);
      modeSelectOutExL.gain(0, powerOutSSB[currentBand]);  //AFP 10-21-22
      modeSelectOutExR.gain(0, powerOutSSB[currentBand]);  //AFP 10-21-22
      ShowTransmitReceiveStatus();

      while (digitalRead(PTT) == LOW) {
        ExciterIQData();
      }
      Q_in_L_Ex.end();  // End Transmit Queue
      Q_in_R_Ex.end();
      Q_in_L.begin();  // Start Receive Queue
      Q_in_R.begin();
      xrState = RECEIVE_STATE;
      break;
    default:
      break;
  }
  //======================  End SSB Mode =================

  // Begin CW Mode state machine

  switch (radioState) {
    case CW_RECEIVE_STATE:
      if (lastState != radioState) {  // G0ORX 01092023
        digitalWrite(MUTE, LOW);      //turn off mute
        T41State = CW_RECEIVE;
        ShowTransmitReceiveStatus();
        xrState = RECEIVE_STATE;
        //SetFreq();   // KF5N
        modeSelectInR.gain(0, 1);
        modeSelectInL.gain(0, 1);
        modeSelectInExR.gain(0, 0);
        modeSelectInExL.gain(0, 0);
        modeSelectOutL.gain(0, 1);
        modeSelectOutR.gain(0, 1);
        modeSelectOutL.gain(1, 0);
        modeSelectOutR.gain(1, 0);
        modeSelectOutExL.gain(0, 0);
        modeSelectOutExR.gain(0, 0);
        keyPressedOn = 0;
      }
      ShowSpectrum();  // if removed CW signal on is 2 mS
      //delay(150);
      break;
    case CW_TRANSMIT_STRAIGHT_STATE:
      powerOutCW[currentBand] = (-.0133 * transmitPowerLevel * transmitPowerLevel + .7884 * transmitPowerLevel + 4.5146) * CWPowerCalibrationFactor[currentBand];
      CW_ExciterIQData();
      xrState = TRANSMIT_STATE;
      ShowTransmitReceiveStatus();
      digitalWrite(MUTE, HIGH);  //   Mute Audio  (HIGH=Mute)
      modeSelectInR.gain(0, 0);
      modeSelectInL.gain(0, 0);
      modeSelectInExR.gain(0, 0);
      modeSelectOutL.gain(0, 0);
      modeSelectOutR.gain(0, 0);
      modeSelectOutExL.gain(0, 0);
      modeSelectOutExR.gain(0, 0);
      cwTimer = millis();
      while (millis() - cwTimer <= cwTransmitDelay) {  //Start CW transmit timer on
        digitalWrite(RXTX, HIGH);
        if (digitalRead(paddleDit) == LOW && keyType == 0) {       // AFP 09-25-22  Turn on CW signal
          cwTimer = millis();                                      //Reset timer
          modeSelectOutExL.gain(0, powerOutCW[currentBand]);       //AFP 10-21-22
          modeSelectOutExR.gain(0, powerOutCW[currentBand]);       //AFP 10-21-22
          digitalWrite(MUTE, LOW);                                 // unmutes audio
          modeSelectOutL.gain(1, volumeLog[(int)sidetoneVolume]);  // Sidetone  AFP 10-01-22
        } else {
          if (digitalRead(paddleDit) == HIGH && keyType == 0) {  //Turn off CW signal
            keyPressedOn = 0;
            digitalWrite(MUTE, HIGH);     // mutes audio
            modeSelectOutExL.gain(0, 0);  //Power = 0
            modeSelectOutExR.gain(0, 0);
            modeSelectOutL.gain(1, 0);  // Sidetone off
            modeSelectOutR.gain(1, 0);
          }
        }
        CW_ExciterIQData();
      }
      modeSelectOutExL.gain(0, 0);  //Power = 0 //AFP 10-11-22
      modeSelectOutExR.gain(0, 0);  //AFP 10-11-22
      digitalWrite(RXTX, LOW);      // End Straight Key Mode
      break;
    case CW_TRANSMIT_KEYER_STATE:
      CW_ExciterIQData();
      xrState = TRANSMIT_STATE;
      ShowTransmitReceiveStatus();
      digitalWrite(MUTE, HIGH);  //   Mute Audio  (HIGH=Mute)
      modeSelectInR.gain(0, 0);
      modeSelectInL.gain(0, 0);
      modeSelectInExR.gain(0, 0);
      modeSelectInExL.gain(0, 0);
      modeSelectOutL.gain(0, 0);
      modeSelectOutR.gain(0, 0);
      modeSelectOutExL.gain(0, 0);
      modeSelectOutExR.gain(0, 0);
      cwTimer = millis();
      while (millis() - cwTimer <= cwTransmitDelay) {
        digitalWrite(RXTX, HIGH);  //Turns on relay
        CW_ExciterIQData();
        modeSelectInR.gain(0, 0);
        modeSelectInL.gain(0, 0);
        modeSelectInExR.gain(0, 0);
        modeSelectInExL.gain(0, 0);
        modeSelectOutL.gain(0, 0);
        modeSelectOutR.gain(0, 0);

        if (digitalRead(paddleDit) == LOW) {  // Keyer Dit
          cwTimer = millis();
          ditTimerOn = millis();
          //          while (millis() - ditTimerOn <= ditLength) {
          while (millis() - ditTimerOn <= transmitDitLength) {       // JJP 8/19/23
            modeSelectOutExL.gain(0, powerOutCW[currentBand]);       //AFP 10-21-22
            modeSelectOutExR.gain(0, powerOutCW[currentBand]);       //AFP 10-21-22
            digitalWrite(MUTE, LOW);                                 // unmutes audio
            modeSelectOutL.gain(1, volumeLog[(int)sidetoneVolume]);  // Sidetone
            CW_ExciterIQData();                                      // Creates CW output signal
            keyPressedOn = 0;
          }
          ditTimerOff = millis();
          //          while (millis() - ditTimerOff <= ditLength - 10) {  //Time between
          while (millis() - ditTimerOff <= transmitDitLength - 10L) {  // JJP 8/19/23
            modeSelectOutExL.gain(0, 0);                               //Power =0
            modeSelectOutExR.gain(0, 0);
            modeSelectOutL.gain(1, 0);  // Sidetone off
            modeSelectOutR.gain(1, 0);
            CW_ExciterIQData();
            keyPressedOn = 0;
          }
        } else {
          if (digitalRead(paddleDah) == LOW) {  //Keyer DAH
            cwTimer = millis();
            dahTimerOn = millis();
            //            while (millis() - dahTimerOn <= 3UL * ditLength) {
            while (millis() - dahTimerOn <= 3UL * transmitDitLength) {  // JJP 8/19/23
              modeSelectOutExL.gain(0, powerOutCW[currentBand]);        //AFP 10-21-22
              modeSelectOutExR.gain(0, powerOutCW[currentBand]);        //AFP 10-21-22
              digitalWrite(MUTE, LOW);                                  // unmutes audio
              modeSelectOutL.gain(1, volumeLog[(int)sidetoneVolume]);   // Dah sidetone was using constants.  KD0RC
              CW_ExciterIQData();                                       // Creates CW output signal
              keyPressedOn = 0;
            }
            ditTimerOff = millis();
            //            while (millis() - ditTimerOff <= ditLength - 10UL) {  //Time between characters                         // mutes audio
            while (millis() - ditTimerOff <= transmitDitLength - 10UL) {  // JJP 8/19/23
              modeSelectOutExL.gain(0, 0);                                //Power =0
              modeSelectOutExR.gain(0, 0);
              modeSelectOutL.gain(1, 0);  // Sidetone off
              modeSelectOutR.gain(1, 0);
              CW_ExciterIQData();
            }
          }
        }
        CW_ExciterIQData();
        keyPressedOn = 0;  // Fix for keyer click-clack.  KF5N August 16, 2023
      }                    //End Relay timer

      modeSelectOutExL.gain(0, 0);  //Power = 0 //AFP 10-11-22
      modeSelectOutExR.gain(0, 0);  //AFP 10-11-22
      digitalWrite(RXTX, LOW);
      xmtMode = CW_MODE;
      break;
    default:
      break;
  }

  //  End radio state machine
  if (lastState != radioState) {
    lastState = radioState;
    ShowTransmitReceiveStatus();
  }

  // update processor load and temp info box items every 200 loops
  if (elapsed_micros_idx_t > (SampleRate / 960)) {
    UpdateInfoBoxItem(&infoBox[IB_ITEM_TEMP]);
    UpdateInfoBoxItem(&infoBox[IB_ITEM_LOAD]);
  }

  if (volumeChangeFlag == true) {
    volumeChangeFlag = false;
    UpdateInfoBoxItem(&infoBox[IB_ITEM_VOL]);
  }

#ifdef DEBUG_LOOP
  ExitLoop();
#endif
}
