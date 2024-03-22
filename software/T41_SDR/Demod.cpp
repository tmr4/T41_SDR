#include "SDT.h"
#include "Demod.h"
#include "Display.h"
#include "FIR.h"
#include "Utility.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

// new synchronous AM PLL & PHASE detector
// wdsp Warren Pratt, 2016
int zeta_help = 65;
float32_t zeta = (float32_t)zeta_help / 100.0;  // PLL step response: smaller, slower response 1.0 - 0.1
float32_t omega_min = TPI * -pll_fmax * 1 / 24000;
float32_t omega_max = TPI * pll_fmax * 1 / 24000;
float32_t g1 = 1.0 - exp(-2.0 * omegaN * zeta * 1 / 24000);
float32_t g2 = -g1 + 2.0 * (1 - exp(-omegaN * zeta * 1 / 24000) * cosf(omegaN * 1 / 24000 * sqrtf(1.0 - zeta * zeta)));
float32_t phzerror = 0.0;
float32_t det = 0.0;
float32_t fil_out = 0.0;
float32_t del_out = 0.0;
float32_t omega2 = 0.0;

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

/*****
  Purpose: AMDecodeSAM()
  Parameter list:
    void
  Return value;
    void
  Notes:  Synchronous AM detection.  Determines the carrier frequency, adjusts freq and replaces the received carrier with a steady signal to prevent fading.
  This alogorithm works best of those implimented
      // taken from Warren Pratt´s WDSP, 2016
  // http://svn.tapr.org/repos_sdr_hpsdr/trunk/W5WC/PowerSDR_HPSDR_mRX_PS/Source/wdsp/
*****/
void AMDecodeSAM() {
  float32_t tauR = 0.02;  // original 0.02;
  float32_t tauI = 1.4;   // original 1.4;
  float32_t dc = 0.0;
  float32_t dc_insert = 0.0;
  float32_t dcu = 0.0;
  float32_t dc_insertu = 0.0;
  float32_t mtauR = exp(-1 / 24000 * tauR);
  float32_t onem_mtauR = 1.0 - mtauR;
  float32_t mtauI = exp(-1 / 24000 * tauI);
  float32_t onem_mtauI = 1.0 - mtauI;
  uint8_t fade_leveler = 1;
  float32_t SAM_carrier = 0.0;
  float32_t SAM_lowpass = 2700.0;
  float32_t SAM_carrier_freq_offset = 0.0;
  float32_t SAM_carrier_freq_offsetOld = 0.0;

  // taken from Warren Pratt´s WDSP, 2016
  // http://svn.tapr.org/repos_sdr_hpsdr/trunk/W5WC/PowerSDR_HPSDR_mRX_PS/Source/wdsp/
  // http://svn.tapr.org/repos_sdr_hpsdr/trunk/W5WC/PowerSDR_HPSDR_mRX_PS/Source/wdsp/
  //phzerror = 0;
  tft.setFontScale( (enum RA8875tsize) 0);
  tft.fillRect(OPERATION_STATS_L + 125 + 160, OPERATION_STATS_T, tft.getFontWidth() * 11, tft.getFontHeight(), RA8875_BLUE);        // AFP 11-01-22 Clear top-left menu area
  tft.setCursor(OPERATION_STATS_L + 125 + 160, OPERATION_STATS_T);   // AFP 11-01-22
  tft.setTextColor(RA8875_WHITE);
  // tft.fill//Rect(OPERATION_STATS_L + 125 + 155, OPERATION_STATS_T, tft.getFontWidth() * 11, tft.getFontHeight(), RA8875_BLUE);
    tft.print("(SAM) ");  //AFP 11-01-22
  for (unsigned i = 0; i < FFT_length / 2 ; i++)
  {
    float32_t Sin, Cos;
    float32_t ai, bi, aq, bq;
    float32_t audio;
    float32_t audiou = 0;
    float32_t corr[2];

    Sin = arm_sin_f32(phzerror);
    Cos = arm_cos_f32(phzerror);

    ai = Cos * iFFT_buffer[FFT_length + i * 2];
    bi = Sin * iFFT_buffer[FFT_length + i * 2];
    aq = Cos * iFFT_buffer[FFT_length + i * 2 + 1];
    bq = Sin * iFFT_buffer[FFT_length + i * 2 + 1];

    corr[0] = +ai + bq;
    corr[1] = -bi + aq;

    audio = (ai - bi) + (aq + bq);

    if (fade_leveler)
    {
      dc = mtauR * dc + onem_mtauR * audio;
      dc_insert = mtauI * dc_insert + onem_mtauI * corr[0];
      audio = audio + dc_insert - dc;
    }

    float_buffer_L[i] = audio;

    if (fade_leveler)
    {
      dcu = mtauR * dcu + onem_mtauR * audiou;
      dc_insertu = mtauI * dc_insertu + onem_mtauI * corr[0];
      audiou = audiou + dc_insertu - dcu;
    }
    float_buffer_R[i] = audiou;
    det = ApproxAtan2(corr[1], corr[0]);

    del_out = fil_out;
    omega2 = omega2 + g2 * det;
    if (omega2 < omega_min) omega2 = omega_min;
    else if (omega2 > omega_max) omega2 = omega_max;
    fil_out = g1 * det + omega2;
    phzerror = phzerror + del_out;

    //wrap round 2PI, modulus
    while (phzerror >= TPI) phzerror -= TPI;
    while (phzerror < 0.0) phzerror += TPI;
  }

  //arm_copy_f32(float_buffer_L, float_buffer_R, FFT_length);
  //}
  //        SAM_display_count++;
  //        if(SAM_display_count > 50) // to display the exact carrier frequency that the PLL is tuned to
  //        if(0)
  // in the small frequency display
  // we calculate carrier offset here and the display function is
  // then called in main loop every 100ms
  //{ // to make this smoother, a simple lowpass/exponential averager here . . .
  SAM_carrier = 0.08 * (omega2 * 24000) / (2 * TPI);
  SAM_carrier = SAM_carrier + 0.92 * SAM_lowpass;
  SAM_carrier_freq_offset =  (int)10*SAM_carrier;
  SAM_carrier_freq_offset=0.9*SAM_carrier_freq_offsetOld+0.1*SAM_carrier_freq_offset;
  //            SAM_display_count = 0;
  SAM_lowpass = SAM_carrier;

  if (SAM_carrier_freq_offset != SAM_carrier_freq_offsetOld) {
    tft.fillRect(OPERATION_STATS_L + 125 + 200, OPERATION_STATS_T, tft.getFontWidth() * 8, tft.getFontHeight(), RA8875_BLUE);
    tft.print(0.20024*SAM_carrier_freq_offset, 1); //AFP 11-01-22
  }
  SAM_carrier_freq_offsetOld=SAM_carrier_freq_offset;
}

/*****
  Purpose: ApproxAtan2
  Parameter list:
    void
  Return value;
    void
*****/
float ApproxAtan2(float y, float x) {
  if (x != 0.0f)
  {
    if (fabsf(x) > fabsf(y))
    {
      const float z = y / x;
      if (x > 0.0f)
      {
        // atan2(y,x) = atan(y/x) if x > 0
        return ApproxAtan(z);
      }
      else if (y >= 0.0f)
      {
        // atan2(y,x) = atan(y/x) + PI if x < 0, y >= 0
        return ApproxAtan(z) + PI;
      }
      else
      {
        // atan2(y,x) = atan(y/x) - PI if x < 0, y < 0
        return ApproxAtan(z) - PI;
      }
    }
    else // Use property atan(y/x) = PI/2 - atan(x/y) if |y/x| > 1.
    {
      const float z = x / y;
      if (y > 0.0f)
      {
        // atan2(y,x) = PI/2 - atan(x/y) if |y/x| > 1, y > 0
        return -ApproxAtan(z) + TPI;
      }
      else
      {
        // atan2(y,x) = -PI/2 - atan(x/y) if |y/x| > 1, y < 0
        return -ApproxAtan(z) - TPI;
      }
    }
  }
  else
  {
    if (y > 0.0f) // x = 0, y > 0
    {
      return TPI;
    }
    else if (y < 0.0f) // x = 0, y < 0
    {
      return -TPI;
    }
  }
  return 0.0f; // x,y = 0. Could return NaN instead.
}

// modified from: fmdemod_quadri_novect_cf in libcsdr.c from https://github.com/ha7ilm/csdr
/*
complexf fmdemod_quadri_novect_cf(complexf* input, float* output, int input_size, complexf last_sample)
{
    output[0]=fmdemod_quadri_K*(iof(input,0)*(qof(input,0)-last_sample.q)-qof(input,0)*(iof(input,0)-last_sample.i))/(iof(input,0)*iof(input,0)+qof(input,0)*qof(input,0));
    for (int i=1; i<input_size; i++) //@fmdemod_quadri_novect_cf
    {
        float qnow=qof(input,i);
        float qlast=qof(input,i-1);
        float inow=iof(input,i);
        float ilast=iof(input,i-1);
        output[i]=fmdemod_quadri_K*(inow*(qnow-qlast)-qnow*(inow-ilast))/(inow*inow+qnow*qnow);
        //TODO: expression can be simplified as: (qnow*ilast-inow*qlast)/(inow*inow+qnow*qnow)
    }
    return input[input_size-1];
}
*/

void nfmdemod(float32_t* input, float32_t* output, int input_size) {
  static float32_t last_sample_i = 0;
  static float32_t last_sample_q = 0;

  output[0]=fmdemod_quadri_K*(input[0] * (input[1] - last_sample_q) - input[1] * (input[0] - last_sample_i))/(input[0] * input[0] + input[1] * input[1]);
  for (int i = 1; i < input_size; i++) {
    float qnow = input[i * 2 + 1];
    float qlast = input[(i - 1) * 2 + 1];
    float inow = input[i * 2];
    float ilast = input[(i - 1) * 2];
    output[i] = fmdemod_quadri_K * (qnow * ilast - inow * qlast) / (inow * inow + qnow * qnow);
  }

  last_sample_i = input[input_size - 2];
  last_sample_q = input[input_size - 1];
}
/*
float32_t qlast, ilast;

float32_t fmdemod_quadri_novect_cf(float32_t  inow, float32_t  qnow) {

  //float32_t output = fmdemod_quadri_K * (inow * (qnow - qlast) - qnow * (inow - ilast)) / (inow * inow + qnow * qnow);
  // which can be simplified as:
  float32_t output = fmdemod_quadri_K * (qnow * ilast - inow * qlast) / (inow * inow + qnow * qnow);

  ilast=inow;
  qlast=qnow;

  return output;
}
*/
// from: predefined.h in https://github.com/ha7ilm/csdr
//   De-emphasis FIR filters:
//     I chose not to implement a filter design algoritm for these, because:
//     - they are defined by their frequency envelope, and I didn't want to implement the least squares or similar algorithm to design such filters
//     - they are most likely needed at given fixed sample rates

/*
octave>
	
function mkdeemph(sr,tapnum,norm_freq) 
	% Make NFM deemphasis filter. Parameters: samplerate, filter length, frequency to normalize at (filter gain will be 0dB at this frequency)
	normalize_at_freq = @(vect,freq) vect/dot(vect,sin(2*pi*freq*[0:1/sr:size(vect)(1)/sr])); 
	freqvect=[0,200, 200,400, 400,3700, 3700,sr/2];
	coeffs=firls(tapnum,freqvect/(sr/2),[0,0,0,1,1,0.1,0,0]);
	coeffs=normalize_at_freq(coeffs,norm_freq);
	freqz(coeffs);
	printf("%g, ",coeffs);
	printf("\n")
end 
*/
/*

// modified from: fmdemod_quadri_novect_cf in libcsdr.c from https://github.com/ha7ilm/csdr
int deemphasis_nfm_ff (float* input, float* output, int input_size, int sample_rate)
{
    float* taps;
    int taps_length=0;

    DNFMFF_ADD_ARRAY(48000)
    DNFMFF_ADD_ARRAY(44100)
    DNFMFF_ADD_ARRAY(8000)
    DNFMFF_ADD_ARRAY(11025)

    if(!taps_length) return 0; //sample rate n
    int i;
    for(i=0;i<input_size-taps_length;i++) //@deemphasis_nfm_ff: outer loop
    {
        float acc=0;
        for(int ti=0;ti<taps_length;ti++) acc+=taps[ti]*input[i+ti]; //@deemphasis_nfm_ff: inner loop
        output[i]=acc;
    }
    return i; //number of samples processed (and output samples)
}

// I used the following at https://octave-online.net

sr=24000
tapnum=79
norm_freq=500
normalize_at_freq = @(vect,freq) vect/dot(vect,sin(2*pi*freq*[0:1/sr:size(vect)(1)/sr])); 
freqvect=[0,200, 200,400, 400,3700, 3700,sr/2];
coeffs=firls(tapnum,freqvect/(sr/2),[0,0,0,1,1,0.1,0,0]);
coeffs=normalize_at_freq(coeffs,norm_freq);
freqz(coeffs);
printf("%g, ",coeffs);
printf("\n")

// to generate the following coefficients for a sample rate of 24000
// the online calculator wasn't able to handle the function but worked ok with hard coded values

0.000481913, -0.000816211, -0.00205384, -0.00264474, -0.00258229, -0.00247939, -0.00305299, -0.00448116, -0.00620366, -0.00737591, -0.00761292, -0.00737176, -0.0075984, -0.00890065, -0.0109592, -0.0127338, -0.0133493, -0.0129165, -0.0125289, -0.013351, -0.0155348, -0.0179452, -0.0190498, -0.0183068, -0.016827, -0.0165808, -0.0186455, -0.0219659, -0.0238965, -0.0223995, -0.0182146, -0.0149414, -0.0163342, -0.0223751, -0.0271497, -0.020849, 0.00446391, 0.0485999, 0.100768, 0.143223, 0.159583, 0.143223, 0.100768, 0.0485999, 0.00446391, -0.020849, -0.0271497, -0.0223751, -0.0163342, -0.0149414, -0.0182146, -0.0223995, -0.0238965, -0.0219659, -0.0186455, -0.0165808, -0.016827, -0.0183068, -0.0190498, -0.0179452, -0.0155348, -0.013351, -0.0125289, -0.0129165, -0.0133493, -0.0127338, -0.0109592, -0.00890065, -0.0075984, -0.00737176, -0.00761292, -0.00737591, -0.00620366, -0.00448116, -0.00305299, -0.00247939, -0.00258229, -0.00264474, -0.00205384, -0.000816211, 0.000481913, 

I also got the following as a check:
//mkdeemph(11025,79,500)
-0.00103853, -0.000836244, -0.00159516, 0.0003133, 0.000779497, 0.000303457, 0.00266777, 0.00327929, 0.00292913, 0.00557135, 0.0060497, 0.00552063, 0.00813664, 0.00807828, 0.00697331, 0.00920894, 0.00816718, 0.00609763, 0.00766544, 0.00525786, 0.00196495, 0.00277562, -0.00121968, -0.00575421, -0.00547422, -0.0110798, -0.0165593, -0.0161744, -0.0232899, -0.0290528, -0.027393, -0.0361026, -0.0409327, -0.0357863, -0.0473975, -0.0479012, -0.0321382, -0.0551507, -0.0149708, 0.199631, 0.347392, 0.199631, -0.0149708, -0.0551507, -0.0321382, -0.0479012, -0.0473975, -0.0357863, -0.0409327, -0.0361026, -0.027393, -0.0290528, -0.0232899, -0.0161744, -0.0165593, -0.0110798, -0.00547422, -0.00575421, -0.00121968, 0.00277562, 0.00196495, 0.00525786, 0.00766544, 0.00609763, 0.00816718, 0.00920894, 0.00697331, 0.00807828, 0.00813664, 0.00552063, 0.0060497, 0.00557135, 0.00292913, 0.00327929, 0.00266777, 0.000303457, 0.000779497, 0.0003133, -0.00159516, -0.000836244, -0.00103853,

//mkdeemph(8000,79,500)
0.000138165, 0.00014018, -0.000449489, 9.59462e-05, -0.00142064, -0.000364014, -0.00251133, -0.00102864, -0.00327923, -0.00150912, -0.0032115, -0.0013541, -0.0018918, -0.000217383, 0.000815149, 0.00196878, 0.00461698, 0.00487091, 0.00873936, 0.00773166, 0.012, 0.00947357, 0.0130147, 0.00892168, 0.0105021, 0.0051025, 0.00362056, -0.00244863, -0.00774686, -0.0134435, -0.0228388, -0.0267019, -0.0400478, -0.0400529, -0.0571495, -0.0497424, -0.0716895, -0.0448243, -0.0814492, 0.1458, 0.47875, 0.1458, -0.0814492, -0.0448243, -0.0716895, -0.0497424, -0.0571495, -0.0400529, -0.0400478, -0.0267019, -0.0228388, -0.0134435, -0.00774686, -0.00244863, 0.00362056, 0.0051025, 0.0105021, 0.00892168, 0.0130147, 0.00947357, 0.012, 0.00773166, 0.00873936, 0.00487091, 0.00461698, 0.00196878, 0.000815149, -0.000217383, -0.0018918, -0.0013541, -0.0032115, -0.00150912, -0.00327923, -0.00102864, -0.00251133, -0.000364014, -0.00142064, 9.59462e-05, -0.000449489, 0.00014018, 0.000138165, 

*** these don't match those from predefined.h ***
*/
/*
float deemphasis_nfm_predefined_fir_24000[] =
{ 0.000481913, -0.000816211, -0.00205384, -0.00264474, -0.00258229, -0.00247939, -0.00305299, -0.00448116, -0.00620366, -0.00737591, -0.00761292, -0.00737176, -0.0075984, -0.00890065, -0.0109592, -0.0127338, -0.0133493, -0.0129165, -0.0125289, -0.013351, -0.0155348, -0.0179452, -0.0190498, -0.0183068, -0.016827, -0.0165808, -0.0186455, -0.0219659, -0.0238965, -0.0223995, -0.0182146, -0.0149414, -0.0163342, -0.0223751, -0.0271497, -0.020849, 0.00446391, 0.0485999, 0.100768, 0.143223, 0.159583, 0.143223, 0.100768, 0.0485999, 0.00446391, -0.020849, -0.0271497, -0.0223751, -0.0163342, -0.0149414, -0.0182146, -0.0223995, -0.0238965, -0.0219659, -0.0186455, -0.0165808, -0.016827, -0.0183068, -0.0190498, -0.0179452, -0.0155348, -0.013351, -0.0125289, -0.0129165, -0.0133493, -0.0127338, -0.0109592, -0.00890065, -0.0075984, -0.00737176, -0.00761292, -0.00737591, -0.00620366, -0.00448116, -0.00305299, -0.00247939, -0.00258229, -0.00264474, -0.00205384, -0.000816211, 0.000481913 };

#define DNFMFF_ADD_ARRAY(x) if(sample_rate==x) { taps=deemphasis_nfm_predefined_fir_##x; taps_length=sizeof(deemphasis_nfm_predefined_fir_##x)/sizeof(float); }

void deemphasis_nfm_ff(float* input, float* output, int input_size, int sample_rate) {
  float* taps;
  int taps_length=0;

  DNFMFF_ADD_ARRAY(24000)

  if(!taps_length) return;

  for(int i = 0; i < input_size - taps_length; i++)
  {
    float acc=0;
    for(int ti = 0; ti < taps_length; ti++) {
      acc += taps[ti] * input[i + ti];
    }
    output[i]=acc;
  }
}
*/
// modified from: fmdemod_atan_cf in libcsdr.c from https://github.com/ha7ilm/csdr
/*
#define argof(complexf_input_p,i) (atan2(qof(complexf_input_p,i),iof(complexf_input_p,i)))

float fmdemod_atan_cf(complexf* input, float *output, int input_size, float last_phase)
{
    //GCC most likely won't vectorize nor atan, nor atan2.
    //For more comments, look at: https://github.com/simonyiszk/minidemod/blob/master/minidemod-wfm-atan.c
    float phase, dphase;
    for (int i=0; i<input_size; i++) //@fmdemod_atan_novect
    {
        phase=argof(input,i);
        dphase=phase-last_phase;
        if(dphase<-PI) dphase+=2*PI;
        if(dphase>PI) dphase-=2*PI;
        output[i]=dphase/PI;
        last_phase=phase;
    }
    return last_phase;
}
*/
/*
void fmdemod_atan_cf(float32_t* input, float32_t* output, int input_size) {
  //GCC most likely won't vectorize nor atan, nor atan2.
  //For more comments, look at: https://github.com/simonyiszk/minidemod/blob/master/minidemod-wfm-atan.c
  float32_t phase, dphase;
  static float32_t last_phase = 0;
  float32_t x, y;

  //output
  for (int i = 0; i < input_size; i++) {

    phase = ApproxAtan2(input[i * 2 + 1], input[i * 2]);

    dphase = phase - last_phase;
    if(dphase < -PI) {
      dphase += 2 * PI;
    }

    if(dphase > PI) {
      dphase -= 2 * PI;
    }

    output[i] = dphase / PI;
    last_phase = phase;
  }
}
*/
