
// modified from https://github.com/DD4WH/Teensy-ConvolutionSDR
// which was the start for the T41 (see https://groups.io/g/SoftwareControlledHamRadio/message/25290)

#include "SDT.h"
#include "Demod.h"
#include "DemodFM.h"
#include "Display.h"
#include "FFT.h"
#include "FIR.h"
#include "Utility.h"

//float deemphasis_wfm_ff (float* input, float* output, int input_size, int sample_rate, float last_output);
void prepare_WFM(void);
void DemodStereo();
void DemodMono();

#define WFM_SAMPLE_RATE   234375.0f
#define RDS_FREQUENCY 57000.0
#define RDS_BITRATE (RDS_FREQUENCY/48.0) //1187.5 bps bitrate

//const uint32_t WFM_BLOCKS = 8;
const uint32_t WFM_BLOCKS = 16;

uint8_t bitnumber = 16; // test, how restriction to twelve bit alters sound quality

float32_t stereo_factor = 100.0;

/*********************************************************************************************************************************************************
   FIR main filter defined here
   also decimation etc.
*********************************************************************************************************************************************************/
int16_t *sp_L;
int16_t *sp_R;
float32_t I_old = 0.2;
float32_t Q_old = 0.2;
float32_t rawFM_old_L = 0.0;
float32_t rawFM_old_R = 0.0;

#define WFM_SAMPLE_RATE_NORM    (TWO_PI / WFM_SAMPLE_RATE) //to normalize Hz to radians
#define PILOTPLL_FREQ     19000.0f  //Centerfreq of pilot tone
#define PILOTPLL_RANGE    20.0f
#define PILOTPLL_BW       50.0f // was 10.0, but then it did not lock properly
#define PILOTPLL_ZETA     0.707f
#define PILOTPLL_LOCK_TIME_CONSTANT 1.0f // lock filter time in seconds
#define PILOTTONEDISPLAYALPHA 0.002f
#define WFM_LOCK_MAG_THRESHOLD     0.06f //0.013f // 0.001f bei taps==20 //0.108f // lock error magnitude
float32_t Pilot_tone_freq = 19000.0f;

#define FMDC_ALPHA 0.001  //time constant for DC removal filter
float32_t m_PilotPhaseAdjust = 1.42; //0.17607;
float32_t WFM_gain = 0.24;
float32_t m_PilotNcoPhase = 0.0;
float32_t WFM_fil_out = 0.0;
float32_t WFM_del_out = 0.0;
float32_t  m_PilotNcoFreq = PILOTPLL_FREQ * WFM_SAMPLE_RATE_NORM; //freq offset to bring to baseband
float32_t DMAMEM UKW_buffer_1[BUFFER_SIZE * WFM_BLOCKS];
float32_t DMAMEM UKW_buffer_2[BUFFER_SIZE * WFM_BLOCKS];
float32_t DMAMEM UKW_buffer_3[BUFFER_SIZE * WFM_BLOCKS];
float32_t DMAMEM UKW_buffer_4[BUFFER_SIZE * WFM_BLOCKS];
float32_t DMAMEM m_PilotPhase[BUFFER_SIZE * WFM_BLOCKS];

#define decimate_WFM 1
const uint16_t UKW_FIR_HILBERT_num_taps = 10;
float32_t WFM_Sin = 0.0;
float32_t WFM_Cos = 1.0;
float32_t WFM_tmp_re = 0.0;
float32_t WFM_tmp_im = 0.0;
float32_t WFM_phzerror = 1.0;
float32_t LminusR = 2.0;

  //initialize the PLL
float32_t  m_PilotNcoLLimit = m_PilotNcoFreq - PILOTPLL_RANGE * WFM_SAMPLE_RATE_NORM;    //clamp FM PLL NCO
float32_t  m_PilotNcoHLimit = m_PilotNcoFreq + PILOTPLL_RANGE * WFM_SAMPLE_RATE_NORM;
float32_t  m_PilotPllAlpha = 2.0 * PILOTPLL_ZETA * PILOTPLL_BW * WFM_SAMPLE_RATE_NORM; // 
float32_t  m_PilotPllBeta = (m_PilotPllAlpha * m_PilotPllAlpha) / (4.0 * PILOTPLL_ZETA * PILOTPLL_ZETA);
float32_t  m_PhaseErrorMagAve = 0.01;
float32_t  m_PhaseErrorMagAlpha = 1.0f - expf(-1.0f/(WFM_SAMPLE_RATE * PILOTPLL_LOCK_TIME_CONSTANT));
float32_t  one_m_m_PhaseErrorMagAlpha = 1.0f - m_PhaseErrorMagAlpha;

#define WFM_DEC_SAMPLES (WFM_BLOCKS * BUFFER_SIZE / 4)
const uint16_t WFM_decimation_taps = 20;
// decimation-by-4 after FM PLL
arm_fir_decimate_instance_f32 WFM_decimation_R;
float32_t DMAMEM WFM_decimation_R_state [WFM_decimation_taps + WFM_BLOCKS * BUFFER_SIZE]; 
arm_fir_decimate_instance_f32 WFM_decimation_L;
float32_t DMAMEM WFM_decimation_L_state [WFM_decimation_taps + WFM_BLOCKS * BUFFER_SIZE]; 
float32_t DMAMEM WFM_decimation_coeffs[WFM_decimation_taps];

// interpolation-by-4 after filtering
// pState is of length (numTaps/L)+blockSize-1 words where blockSize is the number of input samples processed by each call
arm_fir_interpolate_instance_f32 WFM_interpolation_R;
float32_t DMAMEM WFM_interpolation_R_state [WFM_decimation_taps / 4 + WFM_BLOCKS * BUFFER_SIZE / 4]; 
arm_fir_interpolate_instance_f32 WFM_interpolation_L;
float32_t DMAMEM WFM_interpolation_L_state [WFM_decimation_taps / 4 + WFM_BLOCKS * BUFFER_SIZE / 4]; 
float32_t DMAMEM WFM_interpolation_coeffs[WFM_decimation_taps];

// two-stage decimation for RDS I & Q in preparation for the baseband RDS signal PLL
// decimate from 256ksps down to 8ksps --> by 32: 1st stage: 8, 2nd stage: 4
//                                                   1 - sqrt(M * F / 2 - F)
// first stage decimation factor M optimal = 2 * M  ------------------------  
//                                                       2 - F (M + 1)
// total M = 32, F = (fstop - B´) / fstop = (2.4kHz - 1.5kHz) / 2.4kHz 
// M optimal is about 10 --> it has to be an integer of 2, so M1 1st stage == 8, M2 2nd stage == 4

#define WFM_RDS_M1  8
#define WFM_RDS_M2  4

// FIR filters for FM reception on VHF
// --> not needed: reuse FIR_Coef_I and FIR_Coef_Q
//const uint16_t FIR_WFM_num_taps = 24;
const uint16_t FIR_WFM_num_taps = 36;
arm_fir_instance_f32 FIR_WFM_I;
float32_t DMAMEM FIR_WFM_I_state [WFM_BLOCKS * BUFFER_SIZE + FIR_WFM_num_taps - 1]; // numTaps+blockSize-1
arm_fir_instance_f32 FIR_WFM_Q;
float32_t DMAMEM FIR_WFM_Q_state [WFM_BLOCKS * BUFFER_SIZE + FIR_WFM_num_taps - 1];

// 1st stage
// Att = 30dB, fstop = fnew - fpass
// fs = 256k; DF = 8; fnew = 32k
// fstop = 32k - 2.4k = 29.6k; fpass = 2.4k; 
// N taps = 30 / (22 * (29.6 / 256 - 2.4 / 256) ) = 30 / 2.34 = 13 taps

/****************************************************************************************
    init IIR filters
 ****************************************************************************************/
// 2-pole biquad IIR - definitions and Initialisation
const uint32_t N_stages_biquad_lowpass1 = 1;

const uint32_t N_stages_biquad_WFM_15k_L = 4;
float32_t biquad_WFM_15k_L_state [N_stages_biquad_WFM_15k_L * 4];
float32_t biquad_WFM_15k_L_coeffs[5 * N_stages_biquad_WFM_15k_L] = {0,0,0,0,0,  0,0,0,0,0,  0,0,0,0,0,  0,0,0,0,0};
arm_biquad_casd_df1_inst_f32 biquad_WFM_15k_L;

const uint32_t N_stages_biquad_WFM_15k_R = 4;
float32_t biquad_WFM_15k_R_state [N_stages_biquad_WFM_15k_R * 4];
float32_t biquad_WFM_15k_R_coeffs[5 * N_stages_biquad_WFM_15k_R] = {0,0,0,0,0,  0,0,0,0,0,  0,0,0,0,0,  0,0,0,0,0};
arm_biquad_casd_df1_inst_f32 biquad_WFM_15k_R;

//biquad_WFM_19k
const uint32_t N_stages_biquad_WFM_pilot_19k_I = 1;
float32_t biquad_WFM_pilot_19k_I_state [N_stages_biquad_WFM_pilot_19k_I * 4];
float32_t biquad_WFM_pilot_19k_I_coeffs[5 * N_stages_biquad_WFM_pilot_19k_I] = {0, 0, 0, 0, 0};
arm_biquad_casd_df1_inst_f32 biquad_WFM_pilot_19k_I;

//biquad_WFM_38k
const uint32_t N_stages_biquad_WFM_pilot_19k_Q = 1;
float32_t biquad_WFM_pilot_19k_Q_state [N_stages_biquad_WFM_pilot_19k_Q * 4];
float32_t biquad_WFM_pilot_19k_Q_coeffs[5 * N_stages_biquad_WFM_pilot_19k_Q] = {0, 0, 0, 0, 0};
arm_biquad_casd_df1_inst_f32 biquad_WFM_pilot_19k_Q;

//biquad_WFM_notch_19k
const uint32_t N_stages_biquad_WFM_notch_19k = 1;
float32_t biquad_WFM_notch_19k_R_state [N_stages_biquad_WFM_notch_19k * 4];
float32_t biquad_WFM_notch_19k_R_coeffs[5 * N_stages_biquad_WFM_notch_19k] = {0, 0, 0, 0, 0};
arm_biquad_casd_df1_inst_f32 biquad_WFM_notch_19k_R;
//biquad_WFM_notch_19k
float32_t biquad_WFM_notch_19k_L_state [N_stages_biquad_WFM_notch_19k * 4];
float32_t biquad_WFM_notch_19k_L_coeffs[5 * N_stages_biquad_WFM_notch_19k] = {0, 0, 0, 0, 0};
arm_biquad_casd_df1_inst_f32 biquad_WFM_notch_19k_L;

// WFM_RDS_IIR_bitrate
const uint32_t N_stages_WFM_RDS_IIR_bitrate = 1;
float32_t WFM_RDS_IIR_bitrate_state [N_stages_WFM_RDS_IIR_bitrate * 4];
float32_t WFM_RDS_IIR_bitrate_coeffs[5 * N_stages_WFM_RDS_IIR_bitrate] = {0, 0, 0, 0, 0};
arm_biquad_casd_df1_inst_f32 WFM_RDS_IIR_bitrate;

// higher WFM_DEEMPHASIS results in lower volume?
//const float WFM_DEEMPHASIS    = 1e-6f;
//const float WFM_DEEMPHASIS    = 25e-6f;
//const float WFM_DEEMPHASIS    = 50e-6f; //EU: 50 us -> tau = 50e-6, USA: 75 us -> tau = 75e-6
//const float WFM_DEEMPHASIS    = 75e-6f; //EU: 50 us -> tau = 50e-6, USA: 75 us -> tau = 75e-6
//const float WFM_DEEMPHASIS    = 100e-6f;
//const float WFM_DEEMPHASIS    = 1e-5f;
const float WFM_DEEMPHASIS    = 1e-4f;
//float deemp_alpha       = 1.0 - (float)expf(- 1.0 / (WFM_SAMPLE_RATE * WFM_DEEMPHASIS));
float deemp_alpha       = 1.0 - (float)expf(- 1.0 / (24000 * WFM_DEEMPHASIS));
float onem_deemp_alpha = 1.0 - deemp_alpha;

arm_fir_instance_f32 UKW_FIR_HILBERT_I;
float32_t DMAMEM UKW_FIR_HILBERT_I_Coef[UKW_FIR_HILBERT_num_taps];
float32_t DMAMEM UKW_FIR_HILBERT_I_state [WFM_BLOCKS * BUFFER_SIZE + UKW_FIR_HILBERT_num_taps - 1]; // numTaps+blockSize-1
arm_fir_instance_f32 UKW_FIR_HILBERT_Q;
float32_t DMAMEM UKW_FIR_HILBERT_Q_state [WFM_BLOCKS * BUFFER_SIZE + UKW_FIR_HILBERT_num_taps - 1];
float32_t DMAMEM UKW_FIR_HILBERT_Q_Coef[UKW_FIR_HILBERT_num_taps];

const float32_t FIR_WFM_Coef[] =
  // filter by FrankB 
{  0.024405154540077401,
   0.152055989770877281,
   0.390115413186706617,
   0.479032479716888671,
   0.174727105673974092,
  -0.204033825420417675,
  -0.149450624869351067,
   0.129118576221562753,
   0.093080019683336526,
  -0.102082295940738380,
  -0.043415978850535761,
   0.079969610654177556,
   0.007057413581358905,
  -0.054442211502033454,
   0.012783211116867430,
   0.029370378191609491,
  -0.017671723888926162,
  -0.010629639307850618,
   0.013493486514306215,
   599.8054155519106420E-6,
  -0.007062707888932793,
   0.002275020017800628,
   0.002251300239881718,
  -0.001861415601825186,
  -165.4992192582411690E-6,
   786.6717809775776690E-6,
  -257.8915879454064000E-6,
  -164.0510491491323820E-6,
   144.9767054771550360E-6,
  -8.011506011505582950E-6,
  -34.81432996292749490E-6,
   14.82004405549755430E-6,
   943.8477012610577500E-9,
  -2.016532405155152310E-6,
   215.5452254228744380E-9,
   115.0469292380613950E-9
};

void GetFMSample() {
  // get audio samples from the audio buffers and convert them to float
  for(int i = 0; i < (int)WFM_BLOCKS; i++) {
    sp_L = Q_in_L.readBuffer();
    sp_R = Q_in_R.readBuffer();

    switch(bitnumber) {
        
      case 15:
        for (int xx = 0; xx < 128; xx++) {
          sp_L[xx] &= ~((1 << 0));
          sp_R[xx] &= ~((1 << 0));
        }
        break;
        
      case 14:
        for (int xx = 0; xx < 128; xx++) {
          sp_L[xx] &= ~((1 << 0) | (1 << 1));
          sp_R[xx] &= ~((1 << 0) | (1 << 1));
        }
        break;
        
      case 13:
        for (int xx = 0; xx < 128; xx++) {
          sp_L[xx] &= ~((1 << 0) | (1 << 1) | (1 << 2));
          sp_R[xx] &= ~((1 << 0) | (1 << 1) | (1 << 2));
        }
        break;
        
      case 12:
        for (int xx = 0; xx < 128; xx++) {
          sp_L[xx] &= ~((1 << 0) | (1 << 1) | (1 << 2) | (1 << 3));
          sp_R[xx] &= ~((1 << 0) | (1 << 1) | (1 << 2) | (1 << 3));
        }
        break;
        
      case 11:
        for (int xx = 0; xx < 128; xx++) {
          sp_L[xx] &= ~((1 << 0) | (1 << 1) | (1 << 2) | (1 << 3) | (1 << 4));
          sp_R[xx] &= ~((1 << 0) | (1 << 1) | (1 << 2) | (1 << 3) | (1 << 4));
        }
        break;
        
      case 10:
        for (int xx = 0; xx < 128; xx++) {
          sp_L[xx] &= ~((1 << 0) | (1 << 1) | (1 << 2) | (1 << 3) | (1 << 4) | (1 << 5));
          sp_R[xx] &= ~((1 << 0) | (1 << 1) | (1 << 2) | (1 << 3) | (1 << 4) | (1 << 5));
        }
        break;
        
      case 9:
        for (int xx = 0; xx < 128; xx++) {
          sp_L[xx] &= ~((1 << 0) | (1 << 1) | (1 << 2) | (1 << 3) | (1 << 4) | (1 << 5) | (1 << 6));
          sp_R[xx] &= ~((1 << 0) | (1 << 1) | (1 << 2) | (1 << 3) | (1 << 4) | (1 << 5) | (1 << 6));
        }
        break;
        
      case 8:
        for (int xx = 0; xx < 128; xx++) {
          sp_L[xx] &= ~((1 << 0) | (1 << 1) | (1 << 2) | (1 << 3) | (1 << 4) | (1 << 5) | (1 << 6) | (1 << 7));
          sp_R[xx] &= ~((1 << 0) | (1 << 1) | (1 << 2) | (1 << 3) | (1 << 4) | (1 << 5) | (1 << 6) | (1 << 7));
        }
        break;
        
      case 7:
        for (int xx = 0; xx < 128; xx++) {
          sp_L[xx] &= ~((1 << 0) | (1 << 1) | (1 << 2) | (1 << 3) | (1 << 4) | (1 << 5) | (1 << 6) | (1 << 7) | (1 << 8));
          sp_R[xx] &= ~((1 << 0) | (1 << 1) | (1 << 2) | (1 << 3) | (1 << 4) | (1 << 5) | (1 << 6) | (1 << 7) | (1 << 8));
        }
        break;
        
      case 6:
        for (int xx = 0; xx < 128; xx++) {
          sp_L[xx] &= ~((1 << 0) | (1 << 1) | (1 << 2) | (1 << 3) | (1 << 4) | (1 << 5) | (1 << 6) | (1 << 7) | (1 << 8) | (1 << 9));
          sp_R[xx] &= ~((1 << 0) | (1 << 1) | (1 << 2) | (1 << 3) | (1 << 4) | (1 << 5) | (1 << 6) | (1 << 7) | (1 << 8) | (1 << 9));
        }
        break;
        
      case 5:
        for (int xx = 0; xx < 128; xx++) {
          sp_L[xx] &= ~((1 << 0) | (1 << 1) | (1 << 2) | (1 << 3) | (1 << 4) | (1 << 5) | (1 << 6) | (1 << 7) | (1 << 8) | (1 << 9) | (1 << 10));
          sp_R[xx] &= ~((1 << 0) | (1 << 1) | (1 << 2) | (1 << 3) | (1 << 4) | (1 << 5) | (1 << 6) | (1 << 7) | (1 << 8) | (1 << 9) | (1 << 10));
        }
        break;
        
      case 4:
        for (int xx = 0; xx < 128; xx++) {
          sp_L[xx] &= ~((1 << 0) | (1 << 1) | (1 << 2) | (1 << 3) | (1 << 4) | (1 << 5) | (1 << 6) | (1 << 7) | (1 << 8) | (1 << 9) | (1 << 10) | (1 << 11));
          sp_R[xx] &= ~((1 << 0) | (1 << 1) | (1 << 2) | (1 << 3) | (1 << 4) | (1 << 5) | (1 << 6) | (1 << 7) | (1 << 8) | (1 << 9) | (1 << 10) | (1 << 11));
        }
        break;
        
      case 3:
        for (int xx = 0; xx < 128; xx++) {
          sp_L[xx] &= ~((1 << 0) | (1 << 1) | (1 << 2) | (1 << 3) | (1 << 4) | (1 << 5) | (1 << 6) | (1 << 7) | (1 << 8) | (1 << 9) | (1 << 10) | (1 << 11) | (1 << 12));
          sp_R[xx] &= ~((1 << 0) | (1 << 1) | (1 << 2) | (1 << 3) | (1 << 4) | (1 << 5) | (1 << 6) | (1 << 7) | (1 << 8) | (1 << 9) | (1 << 10) | (1 << 11) | (1 << 12));
        }
        break;
    
      default:
        break;
    }

    // convert to float one buffer_size
    // float_buffer samples are now standardized from > -1.0 to < 1.0
    arm_q15_to_float(sp_L, &float_buffer_L[BUFFER_SIZE * i], BUFFER_SIZE); // convert int_buffer to float 32bit
    arm_q15_to_float(sp_R, &float_buffer_R[BUFFER_SIZE * i], BUFFER_SIZE); // convert int_buffer to float 32bit
    Q_in_L.freeBuffer();
    Q_in_R.freeBuffer();
  }
}

void DemodFM() {
  //Serial.println("finished audio buffer prep");

  //Serial.println("starting FM scaling");

  /*************************************************************************************************************************************
    Here would be the right place for an FM filter for FM DXing
    plans:
    - 120kHz filter (narrow FM)
    - 80kHz filter (DX)
    - 50kHz filter (extreme DX)
    do those filters have to be phase linear FIR filters ???
  *************************************************************************************************************************************/
  // filter both: float_buffer_L and float_buffer_R
  arm_fir_f32 (&FIR_WFM_I, float_buffer_L, float_buffer_L, BUFFER_SIZE * WFM_BLOCKS);
  arm_fir_f32 (&FIR_WFM_Q, float_buffer_R, float_buffer_R, BUFFER_SIZE * WFM_BLOCKS);

  const float32_t WFM_scaling_factor = 0.24f; //
  
  FFT_buffer[0] = WFM_scaling_factor * ApproxAtan2(I_old * float_buffer_R[0] - float_buffer_L[0] * Q_old, I_old * float_buffer_L[0] + float_buffer_R[0] * Q_old);
  
  for(int i = 1; i < (int)(BUFFER_SIZE * WFM_BLOCKS); i++) {
    // KA7OEI: http://ka7oei.blogspot.com/2015/11/adding-fm-to-mchf-sdr-transceiver.html
    FFT_buffer[i] = WFM_scaling_factor * ApproxAtan2(float_buffer_L[i - 1] * float_buffer_R[i] - float_buffer_L[i] * float_buffer_R[i - 1], float_buffer_L[i - 1] * float_buffer_L[i] + float_buffer_R[i] * float_buffer_R[i - 1]);
  }
  
  // take care of last sample of each block
  I_old = float_buffer_L[BUFFER_SIZE * WFM_BLOCKS - 1];
  Q_old = float_buffer_R[BUFFER_SIZE * WFM_BLOCKS - 1];

  //Serial.println("finished FM scaling");

  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  // taken from cuteSDR and the excellent explanation by Wheatley (2013): thanks for that excellent piece of educational writing up!
  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  //    1   generate complex signal pair of I and Q
  //        Hilbert BP 10 - 75kHz 
  //    2   BPF 19kHz for pilote tone in both, I & Q
  //    3   PLL for pilot tone in order to determine the phase of the pilot tone 
  //    4   multiply audio with 2 times (2 x 19kHz) the phase of the pilot tone --> L-R signal !
  //    5   lowpass filter 15kHz
  //    6   notch filter 19kHz to eliminate pilot tone from audio

  //Serial.println("starting 1: generate complex signal pair of I and Q");

  // 1   generate complex signal pair of I and Q

  // demodulated signal is in FFT_buffer
  arm_fir_f32(&UKW_FIR_HILBERT_I, FFT_buffer, UKW_buffer_1, BUFFER_SIZE * WFM_BLOCKS);      
  arm_fir_f32(&UKW_FIR_HILBERT_Q, FFT_buffer, UKW_buffer_2, BUFFER_SIZE * WFM_BLOCKS);      

  //Serial.println("starting 2: BPF 19kHz for pilote tone in both, I & Q");

  // 2   BPF 19kHz for pilote tone in both, I & Q
  arm_biquad_cascade_df1_f32 (&biquad_WFM_pilot_19k_I, UKW_buffer_1, UKW_buffer_3, BUFFER_SIZE * WFM_BLOCKS);
  arm_biquad_cascade_df1_f32 (&biquad_WFM_pilot_19k_Q, UKW_buffer_2, UKW_buffer_4, BUFFER_SIZE * WFM_BLOCKS);

  // copy MPX-signal to UKW_buffer_1 for spectrum MPX signal view
  arm_copy_f32(FFT_buffer, UKW_buffer_1, BUFFER_SIZE * WFM_BLOCKS);

  Pilot_tone_freq = Pilot_tone_freq * (1.0f - PILOTTONEDISPLAYALPHA) + PILOTTONEDISPLAYALPHA * m_PilotNcoFreq * WFM_SAMPLE_RATE / TWO_PI;

  //Serial.println("starting 3: PLL for pilot tone in order to determine the phase of the pilot tone");

  // 3   PLL for pilot tone in order to determine the phase of the pilot tone
  for (unsigned i = 0; i < BUFFER_SIZE * WFM_BLOCKS; i++) {
    //Serial.print("at 3: i = ");
    //Serial.println(i);

    WFM_Sin = arm_sin_f32(m_PilotNcoPhase);
    WFM_Cos = arm_cos_f32(m_PilotNcoPhase);

    WFM_tmp_re = WFM_Cos * UKW_buffer_3[i] - WFM_Sin * UKW_buffer_4[i];
    WFM_tmp_im = WFM_Cos * UKW_buffer_4[i] + WFM_Sin * UKW_buffer_3[i];
    WFM_phzerror = -ApproxAtan2(WFM_tmp_im, WFM_tmp_re);
    WFM_del_out = WFM_fil_out; // wdsp
    m_PilotNcoFreq += (m_PilotPllBeta * WFM_phzerror);

    if (m_PilotNcoFreq > m_PilotNcoHLimit) {
      m_PilotNcoFreq = m_PilotNcoHLimit;
    } else {
      if (m_PilotNcoFreq < m_PilotNcoLLimit) {
        m_PilotNcoFreq = m_PilotNcoLLimit;
      }
    }
    WFM_fil_out = m_PilotNcoFreq + m_PilotPllAlpha * WFM_phzerror;

    m_PilotNcoPhase += WFM_del_out;
    m_PilotPhase[i] = m_PilotNcoPhase + m_PilotPhaseAdjust;

    // wrap round 2PI, modulus
    while (m_PilotNcoPhase >= TPI) {
      m_PilotNcoPhase -= TPI;
    }
    while (m_PilotNcoPhase < 0.0f) {
      m_PilotNcoPhase += TPI;
    }
    m_PhaseErrorMagAve = one_m_m_PhaseErrorMagAlpha * m_PhaseErrorMagAve + m_PhaseErrorMagAlpha * WFM_phzerror * WFM_phzerror;
  }

  // Teensy_Convolution_SDR.ino forces stereo processing here with a note of  "FIXME !!!"
  // Does this mean that the mono processing in that program is faulty?  Try stereo
  // prosessing as the mono processing isn't working (though the problem is likely my incorporation of the code).
  // Note my test signal isn't stereo.
  if(1) 
  { 
    // if pilot tone present, do stereo demuxing
    DemodStereo();
  } else {
  // no pilot so is mono
    DemodMono();
  }

  if (Q_in_L.available() >  25) {
    AudioNoInterrupts();
    Q_in_L.clear();
    AudioInterrupts();
  }
  
  if (Q_in_R.available() >  25) {
    AudioNoInterrupts();
    Q_in_R.clear();
    AudioInterrupts();
  }

  // it should be possible to do RDS decoding???
  // preliminary idea for the workflow: 
  /*
      192ksp sample rate of I & Q 
    Calculate real signal by atan2f (Nyquist is now 96k)
    Highpass-filter at 54kHz  keep 54 – 96k
    Decimate-by-2 (96ksps, Nyquist 48k)
    57kHz == 39kHz
    Highpass 36kHz  keep 36-48k
    Decimate-by-3 (32ksps, Nyquist 16k)
    39kHz = 7kHz
    Lowpass 8k
    Decimate-by-2 (16ksps, Nyquist 8k)
    Mix to baseband with 7kHz oscillator
    Lowpass 4k
    Decimate-by-2 (8ksps, Nyquist 4k)
    Decode baseband RDS signal
  */

  for (int i = 0; i < (int)WFM_BLOCKS; i++) {
    sp_L = Q_out_L.getBuffer();
    sp_R = Q_out_R.getBuffer();
    arm_float_to_q15 (&float_buffer_L[BUFFER_SIZE * i], sp_L, BUFFER_SIZE);
    arm_float_to_q15 (&iFFT_buffer[BUFFER_SIZE * i], sp_R, BUFFER_SIZE);
    Q_out_L.playBuffer(); // play it !
    Q_out_R.playBuffer(); // play it !
  }

  //if (show_spectrum_flag) {
  //  elapsed_micros_sum = elapsed_micros_sum + usec;
  //  elapsed_micros_idx_t++;
  //  //      Serial.print("elapsed_micros_idx_t = ");
  //  //      Serial.println(elapsed_micros_idx_t);
  //  //      if (elapsed_micros_idx_t > 1000)
  //  tft.setTextColor(ILI9341_RED);
  //  tft.setCursor(227, 5);
  //  tft.print("100%");
  //}

  //          tft.print (mean/29.0 * SR[SAMPLE_RATE].rate / AUDIO_SAMPLE_RATE_EXACT / WFM_BLOCKS);tft.print("%");
  //elapsed_micros_idx_t = 1;
  //elapsed_micros_sum = 0;
  //          Serial.print(" n_clear = "); Serial.println(n_clear);
  //          Serial.print ("1 - Alpha = "); Serial.println(onem_deemp_alpha);
  //          Serial.print ("Alpha = "); Serial.println(deemp_alpha);

}

void DemodStereo() {
  //Serial.println("starting 4: multiply audio with 2 times (2 x 19kHz) the phase of the pilot tone --> L-R signal !");

  for(unsigned i = 0; i < BUFFER_SIZE * WFM_BLOCKS; i++)
  {
    //    4   multiply audio with 2 times (2 x 19kHz) the phase of the pilot tone --> L-R signal !
    LminusR = (stereo_factor / 100.0f) * FFT_buffer[i] * arm_sin_f32(m_PilotPhase[i] * 2.0f);

    float_buffer_R[i] = FFT_buffer[i]; // MPX-Signal: L+R
    iFFT_buffer[i] = LminusR;          // L-R - Signal
    UKW_buffer_2[i] = FFT_buffer[i] * arm_sin_f32(m_PilotPhase[i] * 3.0f); // is this the RDS signal at 57kHz ?
  }

  //Serial.println("at 4a");

  // STEREO post-processing
  // decimate-by-4 --> 64ksps
  arm_fir_decimate_f32(&WFM_decimation_R, float_buffer_R, float_buffer_R, BUFFER_SIZE * WFM_BLOCKS);
  arm_fir_decimate_f32(&WFM_decimation_L, iFFT_buffer, iFFT_buffer, BUFFER_SIZE * WFM_BLOCKS);

  //Serial.println("at 4b");

  // make L & R channels
  for(unsigned i = 0; i < WFM_DEC_SAMPLES; i++)
  {
    float hilfsV = float_buffer_R[i]; // L+R
    float_buffer_R[i] = float_buffer_R[i] + iFFT_buffer[i]; // left channel
    iFFT_buffer[i] = hilfsV - iFFT_buffer[i]; // right channel
  }

  //Serial.println("starting 5: lowpass filter 15kHz & deemphasis");

  //   5   lowpass filter 15kHz & deemphasis
  // Right channel: lowpass filter with 15kHz Fstop & deemphasis
  rawFM_old_R = deemphasis_wfm_ff (float_buffer_R, FFT_buffer, WFM_DEC_SAMPLES, WFM_SAMPLE_RATE / 4.0f, rawFM_old_R);
  arm_biquad_cascade_df1_f32 (&biquad_WFM_15k_R, FFT_buffer, float_buffer_R, WFM_DEC_SAMPLES);

  // Left channel: lowpass filter with 15kHz Fstop & deemphasis
  rawFM_old_L = deemphasis_wfm_ff (iFFT_buffer, float_buffer_L, WFM_DEC_SAMPLES, WFM_SAMPLE_RATE / 4.0f, rawFM_old_L);
  arm_biquad_cascade_df1_f32 (&biquad_WFM_15k_L, float_buffer_L, FFT_buffer, WFM_DEC_SAMPLES);

  if (spectrum_zoom != 0) {
    ZoomFFTExe(BUFFER_SIZE * WFM_BLOCKS);
  }

  //Serial.println("starting 6: notch filter 19kHz to eliminate pilot tone from audio");

  //   6   notch filter 19kHz to eliminate pilot tone from audio
  arm_biquad_cascade_df1_f32 (&biquad_WFM_notch_19k_R, float_buffer_R, float_buffer_L, WFM_DEC_SAMPLES);
  arm_biquad_cascade_df1_f32 (&biquad_WFM_notch_19k_L, FFT_buffer, iFFT_buffer, WFM_DEC_SAMPLES);

  /**********************************************************************************
      EXPERIMENTAL: Audio Equalizer
      by Bob Larkin
  **********************************************************************************/
  //arm_fir_f32(&AudioEqualizer_FIR_L, float_buffer_L, float_buffer_L, WFM_DEC_SAMPLES);    
  //arm_fir_f32(&AudioEqualizer_FIR_R, iFFT_buffer, iFFT_buffer, WFM_DEC_SAMPLES);

  // interpolate-by-4 to 256ksps before sending audio to DAC
  arm_fir_interpolate_f32(&WFM_interpolation_R, float_buffer_L, float_buffer_R, WFM_DEC_SAMPLES);
  arm_fir_interpolate_f32(&WFM_interpolation_L, iFFT_buffer, FFT_buffer, WFM_DEC_SAMPLES);

  // scaling after interpolation !
  //arm_scale_f32(float_buffer_R, 4.0f, float_buffer_L, BUFFER_SIZE * WFM_BLOCKS);
  //arm_scale_f32(FFT_buffer, 4.0f, iFFT_buffer, BUFFER_SIZE * WFM_BLOCKS);
  arm_scale_f32(float_buffer_R, 0.1f, float_buffer_L, BUFFER_SIZE * WFM_BLOCKS);
  arm_scale_f32(FFT_buffer, 0.1f, iFFT_buffer, BUFFER_SIZE * WFM_BLOCKS);
}

void DemodMono() {
  //Serial.println("starting 4?: multiply audio with 2 times (2 x 19kHz) the phase of the pilot tone --> L-R signal !");

  // no pilot so is mono. Just copy real FM demod into both right and left channels
  // plus MONO post-processing
  // decimate-by-4 --> 64ksps
  arm_fir_decimate_f32(&WFM_decimation_R, UKW_buffer_1, FFT_buffer, BUFFER_SIZE * WFM_BLOCKS);

  //Serial.println("starting 5: lowpass filter 15kHz & deemphasis");

  //   5   lowpass filter 15kHz & deemphasis
  // lowpass filter with 15kHz Fstop & deemphasis
  rawFM_old_R = deemphasis_wfm_ff (FFT_buffer, float_buffer_R, WFM_DEC_SAMPLES, WFM_SAMPLE_RATE / 4.0f, rawFM_old_R);
  arm_biquad_cascade_df1_f32 (&biquad_WFM_15k_L, float_buffer_R, FFT_buffer, WFM_DEC_SAMPLES);




  arm_copy_f32(float_buffer_L, float_buffer_R, WFM_DEC_SAMPLES);

  if (spectrum_zoom != 0) {
    ZoomFFTExe(BUFFER_SIZE * WFM_BLOCKS);
  }



  //Serial.println("starting 6: notch filter 19kHz to eliminate pilot tone from audio");

  //   6   notch filter 19kHz to eliminate pilot tone from audio
  arm_biquad_cascade_df1_f32 (&biquad_WFM_notch_19k_L, FFT_buffer, iFFT_buffer, WFM_DEC_SAMPLES);

  // interpolate-by-4 to 256ksps before sending audio to DAC
  arm_fir_interpolate_f32(&WFM_interpolation_L, iFFT_buffer, FFT_buffer, WFM_DEC_SAMPLES);

  // scaling after interpolation !
  //arm_scale_f32(FFT_buffer, 4.0f, iFFT_buffer, BUFFER_SIZE * WFM_BLOCKS);
  arm_scale_f32(FFT_buffer, 0.1f, iFFT_buffer, BUFFER_SIZE * WFM_BLOCKS);

  arm_copy_f32(iFFT_buffer, float_buffer_L, BUFFER_SIZE * WFM_BLOCKS); 
}

float deemphasis_wfm_ff (float* input, float* output, int input_size, int sample_rate, float last_output) {
  /* taken from libcsdr
    typical time constant (tau) values:
    WFM transmission in USA: 75 us -> tau = 75e-6
    WFM transmission in EU:  50 us -> tau = 50e-6
    More info at: http://www.cliftonlaboratories.com/fm_receivers_and_de-emphasis.htm
    Simulate in octave: tau=75e-6; dt=1/48000; alpha = dt/(tau+dt); freqz([alpha],[1 -(1-alpha)])
  */
  //  if(is_nan(last_output)) last_output=0.0; //if last_output is NaN
  output[0] = deemp_alpha * input[0] + (onem_deemp_alpha) * last_output;
  for (int i = 1; i < input_size; i++) //@deemphasis_wfm_ff
    output[i] = deemp_alpha * input[i] + (onem_deemp_alpha) * output[i - 1]; //this is the simplest IIR LPF
  return output[input_size - 1];
}

void setupFM() {
  for(unsigned i = 0; i < BUFFER_SIZE * WFM_BLOCKS; i++)
  {
      UKW_buffer_1[i] = 0.0;
      UKW_buffer_2[i] = 0.0;
      UKW_buffer_3[i] = 0.0;
      UKW_buffer_4[i] = 0.0;
  }

  biquad_WFM_15k_L.numStages = N_stages_biquad_WFM_15k_L; // set number of stages
  biquad_WFM_15k_L.pCoeffs = biquad_WFM_15k_L_coeffs; // set pointer to coefficients file
  for (unsigned i = 0; i < 4 * N_stages_biquad_WFM_15k_L; i++)
  {
    biquad_WFM_15k_L_state[i] = 0.0; // set state variables to zero
  }
  biquad_WFM_15k_L.pState = biquad_WFM_15k_L_state; // set pointer to the state variables

  biquad_WFM_15k_R.numStages = N_stages_biquad_WFM_15k_R; // set number of stages
  biquad_WFM_15k_R.pCoeffs = biquad_WFM_15k_L_coeffs; // set pointer to coefficients file --> YES, right channel filter uses the left channels´ coeffs!
  for (unsigned i = 0; i < 4 * N_stages_biquad_WFM_15k_R; i++)
  {
    biquad_WFM_15k_R_state[i] = 0.0; // set state variables to zero
  }
  biquad_WFM_15k_R.pState = biquad_WFM_15k_R_state; // set pointer to the state variables

  biquad_WFM_pilot_19k_I.numStages = N_stages_biquad_WFM_pilot_19k_I; // set number of stages
  biquad_WFM_pilot_19k_I.pCoeffs = biquad_WFM_pilot_19k_I_coeffs; // set pointer to coefficients file
  for (unsigned i = 0; i < 4 * N_stages_biquad_WFM_pilot_19k_I; i++)
  {
    biquad_WFM_pilot_19k_I_state[i] = 0.0; // set state variables to zero
  }
  biquad_WFM_pilot_19k_I.pState = biquad_WFM_pilot_19k_I_state; // set pointer to the state variables

  biquad_WFM_pilot_19k_Q.numStages = N_stages_biquad_WFM_pilot_19k_Q; // set number of stages
  biquad_WFM_pilot_19k_Q.pCoeffs = biquad_WFM_pilot_19k_Q_coeffs; // set pointer to coefficients file
  for (unsigned i = 0; i < 4 * N_stages_biquad_WFM_pilot_19k_Q; i++)
  {
    biquad_WFM_pilot_19k_Q_state[i] = 0.0; // set state variables to zero
  }
  biquad_WFM_pilot_19k_Q.pState = biquad_WFM_pilot_19k_Q_state; // set pointer to the state variables

  biquad_WFM_notch_19k_R.numStages = N_stages_biquad_WFM_notch_19k; // set number of stages
  biquad_WFM_notch_19k_R.pCoeffs = biquad_WFM_notch_19k_R_coeffs; // set pointer to coefficients file
  for (unsigned i = 0; i < 4 * N_stages_biquad_WFM_notch_19k; i++)
  {
    biquad_WFM_notch_19k_R_state[i] = 0.0; // set state variables to zero
  }
  biquad_WFM_notch_19k_R.pState = biquad_WFM_notch_19k_R_state; // set pointer to the state variables

  biquad_WFM_notch_19k_L.numStages = N_stages_biquad_WFM_notch_19k; // set number of stages
  biquad_WFM_notch_19k_L.pCoeffs = biquad_WFM_notch_19k_L_coeffs; // set pointer to coefficients file
  for (unsigned i = 0; i < 4 * N_stages_biquad_WFM_notch_19k; i++)
  {
    biquad_WFM_notch_19k_L_state[i] = 0.0; // set state variables to zero
  }
  biquad_WFM_notch_19k_L.pState = biquad_WFM_notch_19k_L_state; // set pointer to the state variables

  /****************************************************************************************
     Coefficients for WFM Hilbert filters
  ****************************************************************************************/
  // calculate Hilbert filter pair for splitting of UKW MPX signal
  CalcCplxFIRCoeffs(UKW_FIR_HILBERT_I_Coef, UKW_FIR_HILBERT_Q_Coef, UKW_FIR_HILBERT_num_taps, (float32_t)17000, (float32_t)75000, (float)WFM_SAMPLE_RATE);

  // Hilbert filters to generate PLL for 19kHz pilote tone
  arm_fir_init_f32 (&UKW_FIR_HILBERT_I, UKW_FIR_HILBERT_num_taps, UKW_FIR_HILBERT_I_Coef, UKW_FIR_HILBERT_I_state, (uint32_t)WFM_BLOCKS * BUFFER_SIZE);
  arm_fir_init_f32 (&UKW_FIR_HILBERT_Q, UKW_FIR_HILBERT_num_taps, UKW_FIR_HILBERT_Q_Coef, UKW_FIR_HILBERT_Q_state, (uint32_t)WFM_BLOCKS * BUFFER_SIZE);

  arm_fir_init_f32 (&FIR_WFM_I, FIR_WFM_num_taps, (float32_t*)FIR_WFM_Coef, FIR_WFM_I_state, (uint32_t)WFM_BLOCKS * BUFFER_SIZE);
  arm_fir_init_f32 (&FIR_WFM_Q, FIR_WFM_num_taps, (float32_t*)FIR_WFM_Coef, FIR_WFM_Q_state, (uint32_t)WFM_BLOCKS * BUFFER_SIZE);

  CalcFIRCoeffs(WFM_decimation_coeffs, WFM_decimation_taps, (float32_t)15000, 60, 0, 0.0, WFM_SAMPLE_RATE);
  if (arm_fir_decimate_init_f32(&WFM_decimation_R, WFM_decimation_taps, (uint32_t)4 , WFM_decimation_coeffs, WFM_decimation_R_state, BUFFER_SIZE * WFM_BLOCKS)) {
    Serial.println("Init of decimation failed");
    while(1);
  }
  if (arm_fir_decimate_init_f32(&WFM_decimation_L, WFM_decimation_taps, (uint32_t)4 , WFM_decimation_coeffs, WFM_decimation_L_state, BUFFER_SIZE * WFM_BLOCKS)) {
    Serial.println("Init of decimation failed");
    while(1);
  }

  CalcFIRCoeffs(WFM_interpolation_coeffs, WFM_decimation_taps, (float32_t)15000, 60, 0, 0.0, WFM_SAMPLE_RATE / 4.0f);
  if (arm_fir_interpolate_init_f32(&WFM_interpolation_R, (uint8_t)4, WFM_decimation_taps, WFM_interpolation_coeffs, WFM_interpolation_R_state, BUFFER_SIZE * WFM_BLOCKS / (uint32_t)4)) 
  {
    Serial.println("Init of interpolation failed");
    while(1);
  }
  if (arm_fir_interpolate_init_f32(&WFM_interpolation_L, (uint8_t)4, WFM_decimation_taps, WFM_interpolation_coeffs, WFM_interpolation_L_state, BUFFER_SIZE * WFM_BLOCKS / (uint32_t)4)) {
    Serial.println("Init of interpolation failed");
    while(1);
  }

  prepare_WFM();

}

void prepare_WFM(void)
{
  if(decimate_WFM)
  {
          //          uint64_t prec_help = (95400000 - 0.75 * (uint64_t)SR[SAMPLE_RATE].rate) / 0.03;
        //          bands[current_band].freq = (unsigned long long)(prec_help);

        deemp_alpha       = 1.0 - (float)expf(- 1.0 / (WFM_SAMPLE_RATE / 4.0f * WFM_DEEMPHASIS));
        onem_deemp_alpha = 1.0 - deemp_alpha;
    
      // IIR lowpass filter for wideband FM at 15k
      SetIIRCoeffs((float32_t)15000, 0.54, (float32_t)WFM_SAMPLE_RATE / 4.0, 0); // 1st stage
      for (int i = 0; i < 5; i++)
      { // fill coefficients into the right file
        biquad_WFM_15k_L_coeffs[i] = coefficient_set[i];
        biquad_WFM_15k_L_coeffs[i + 10] = coefficient_set[i];
      }
      SetIIRCoeffs((float32_t)15000, 1.3, (float32_t)WFM_SAMPLE_RATE / 4.0, 0); // 1st stage
      for (int i = 0; i < 5; i++)
      { // fill coefficients into the right file
        biquad_WFM_15k_L_coeffs[i + 5] = coefficient_set[i];
        biquad_WFM_15k_L_coeffs[i + 15] = coefficient_set[i];
      }
    
      // high Q IIR bandpass filter for wideband FM at 19k
    //  SetIIRCoeffs((float32_t)19000, 1000.0, (float32_t)WFM_SAMPLE_RATE, 2); // 1st stage
      SetIIRCoeffs((float32_t)19000, 200.0, (float32_t)WFM_SAMPLE_RATE, 2); // 1st stage
      for (int i = 0; i < 5; i++)
      { // fill coefficients into the right file
        biquad_WFM_pilot_19k_I_coeffs[i] = coefficient_set[i];
      }
    
      // high Q IIR bandpass filter for wideband FM at 19k
    //  SetIIRCoeffs((float32_t)19000, 1000.0, (float32_t)WFM_SAMPLE_RATE, 2); // 1st stage
      SetIIRCoeffs((float32_t)19000, 200.0, (float32_t)WFM_SAMPLE_RATE, 2); // 1st stage
      for (int i = 0; i < 5; i++)
      { // fill coefficients into the right file
        biquad_WFM_pilot_19k_Q_coeffs[i] = coefficient_set[i];
      }
    
      // high Q IIR 19kHz notch filter for wideband FM at 64ksps sample rate
    //  SetIIRCoeffs((float32_t)19000, 1000.0, (float32_t)WFM_SAMPLE_RATE / 4.0, 3); // 1st stage
      SetIIRCoeffs((float32_t)19000, 200.0, (float32_t)WFM_SAMPLE_RATE / 4.0, 3); // 1st stage
      for (int i = 0; i < 5; i++)
      { // fill coefficients into the right file
        biquad_WFM_notch_19k_R_coeffs[i] = coefficient_set[i];
      }
      // high Q IIR 19kHz notch filter for wideband FM at 64ksps sample rate
    //  SetIIRCoeffs((float32_t)19000, 1000.0, (float32_t)WFM_SAMPLE_RATE / 4.0, 3); // 1st stage
      SetIIRCoeffs((float32_t)19000, 200.0, (float32_t)WFM_SAMPLE_RATE / 4.0, 3); // 1st stage
      for (int i = 0; i < 5; i++)
      { // fill coefficients into the right file
        biquad_WFM_notch_19k_L_coeffs[i] = coefficient_set[i];
      }
    
      // high Q IIR BP filter for RDS bitrate at 8ksps sample rate
      SetIIRCoeffs((float32_t)RDS_BITRATE, 500.0, (float32_t)WFM_SAMPLE_RATE / WFM_RDS_M1 / WFM_RDS_M2, 2); // 1st stage
      for (int i = 0; i < 5; i++)
      { // fill coefficients into the right file
        WFM_RDS_IIR_bitrate_coeffs[i] = coefficient_set[i];
      }
  }
  else
  {
        deemp_alpha      = 1.0 - (float)expf(- 1.0 / ((float32_t)SampleRate) * WFM_DEEMPHASIS);
        onem_deemp_alpha = 1.0 - deemp_alpha;        
    
      // IIR lowpass filter for wideband FM at 15k
      SetIIRCoeffs((float32_t)15000, 0.54, (float32_t)WFM_SAMPLE_RATE, 0); // 1st stage
      for (int i = 0; i < 5; i++)
      { // fill coefficients into the right file
        biquad_WFM_15k_L_coeffs[i] = coefficient_set[i];
        biquad_WFM_15k_L_coeffs[i + 10] = coefficient_set[i];
      }
      SetIIRCoeffs((float32_t)15000, 1.3, (float32_t)WFM_SAMPLE_RATE, 0); // 2nd stage
      for (int i = 0; i < 5; i++)
      { // fill coefficients into the right file
        biquad_WFM_15k_L_coeffs[i + 5] = coefficient_set[i];
        biquad_WFM_15k_L_coeffs[i + 15] = coefficient_set[i];
      }
    
      // high Q IIR bandpass filter for wideband FM at 19k
    //  SetIIRCoeffs((float32_t)19000, 1000.0, (float32_t)WFM_SAMPLE_RATE, 2); // 1st stage
      SetIIRCoeffs((float32_t)19000, 200.0, (float32_t)WFM_SAMPLE_RATE, 2); // 1st stage
      for (int i = 0; i < 5; i++)
      { // fill coefficients into the right file
        biquad_WFM_pilot_19k_I_coeffs[i] = coefficient_set[i];
      }
    
      // high Q IIR bandpass filter for wideband FM at 19k
    //  SetIIRCoeffs((float32_t)19000, 1000.0, (float32_t)WFM_SAMPLE_RATE, 2); // 1st stage
      SetIIRCoeffs((float32_t)19000, 200.0, (float32_t)WFM_SAMPLE_RATE, 2); // 1st stage
      for (int i = 0; i < 5; i++)
      { // fill coefficients into the right file
        biquad_WFM_pilot_19k_Q_coeffs[i] = coefficient_set[i];
      }
    
      // high Q IIR 19kHz notch filter for wideband FM at WFM sample rate
    //  SetIIRCoeffs((float32_t)19000, 1000.0, (float32_t)WFM_SAMPLE_RATE, 3); // 1st stage
      SetIIRCoeffs((float32_t)19000, 200.0, (float32_t)WFM_SAMPLE_RATE, 3); // 1st stage
      for (int i = 0; i < 5; i++)
      { // fill coefficients into the right file
        biquad_WFM_notch_19k_R_coeffs[i] = coefficient_set[i];
      }
      // high Q IIR 19kHz notch filter for wideband FM at WFM sample rate
    //  SetIIRCoeffs((float32_t)19000, 1000.0, (float32_t)WFM_SAMPLE_RATE, 3); // 1st stage
      SetIIRCoeffs((float32_t)19000, 200.0, (float32_t)WFM_SAMPLE_RATE, 3); // 1st stage
      for (int i = 0; i < 5; i++)
      { // fill coefficients into the right file
        biquad_WFM_notch_19k_L_coeffs[i] = coefficient_set[i];
      }
      // high Q IIR BP filter for RDS bitrate at 8ksps sample rate
      SetIIRCoeffs((float32_t)RDS_BITRATE, 500.0, (float32_t)WFM_SAMPLE_RATE / WFM_RDS_M1 / WFM_RDS_M2, 2); // 1st stage
      for (int i = 0; i < 5; i++)
      { // fill coefficients into the right file
        WFM_RDS_IIR_bitrate_coeffs[i] = coefficient_set[i];
      }  
  }
}
