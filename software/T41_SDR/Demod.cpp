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
  tft.fillRect(OPERATION_STATS_X + 160, FREQUENCY_Y + 30, tft.getFontWidth() * 11, tft.getFontHeight(), RA8875_BLUE);        // AFP 11-01-22 Clear top-left menu area
  tft.setCursor(OPERATION_STATS_X + 160, FREQUENCY_Y + 30);   // AFP 11-01-22
  tft.setTextColor(RA8875_WHITE);
  // tft.fill//Rect(OPERATION_STATS_X + 155, FREQUENCY_Y + 30, tft.getFontWidth() * 11, tft.getFontHeight(), RA8875_BLUE);
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
    tft.fillRect(OPERATION_STATS_X + 200, FREQUENCY_Y + 30, tft.getFontWidth() * 8, tft.getFontHeight(), RA8875_BLUE);
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

float32_t qlast, ilast;

float32_t fmdemod_quadri_novect_cf(float32_t  inow, float32_t  qnow) {

  //float32_t output = fmdemod_quadri_K * (inow * (qnow - qlast) - qnow * (inow - ilast)) / (inow * inow + qnow * qnow);
  // which can be simplified as:
  float32_t output = fmdemod_quadri_K * (qnow * ilast - inow * qlast) / (inow * inow + qnow * qnow);

  ilast=inow;
  qlast=qnow;

  return output;
}

// from: predefined.h in https://github.com/ha7ilm/csdr
//mkdeemph(48000,199,500)
float deemphasis_nfm_predefined_fir_48000[] = 
{ 0.00172568, 0.00179665, 0.00191952, 0.00205318, 0.00215178, 0.00217534, 0.00209924, 0.00192026, 0.00165789, 0.0013502, 0.00104545, 0.000790927, 0.000621911, 0.000553077, 0.000574554, 0.000653624, 0.000741816, 0.000785877, 0.000740151, 0.000577506, 0.000296217, -7.89273e-05, -0.0005017, -0.000914683, -0.00126243, -0.00150456, -0.00162564, -0.0016396, -0.00158725, -0.00152751, -0.00152401, -0.00163025, -0.00187658, -0.00226223, -0.00275443, -0.003295, -0.0038132, -0.00424193, -0.00453375, -0.00467274, -0.00467943, -0.00460728, -0.00453119, -0.00453056, -0.00467051, -0.00498574, -0.00547096, -0.00608027, -0.00673627, -0.00734698, -0.00782705, -0.00811841, -0.00820539, -0.00812057, -0.00793936, -0.00776415, -0.00770111, -0.00783479, -0.00820643, -0.00880131, -0.00954878, -0.0103356, -0.0110303, -0.011514, -0.0117094, -0.0116029, -0.0112526, -0.0107795, -0.010343, -0.0101053, -0.0101917, -0.0106561, -0.0114608, -0.0124761, -0.0135018, -0.0143081, -0.0146885, -0.0145126, -0.0137683, -0.0125796, -0.0111959, -0.00994914, -0.00918404, -0.00917447, -0.0100402, -0.0116822, -0.0137533, -0.0156723, -0.0166881, -0.0159848, -0.0128153, -0.00664117, 0.00274383, 0.0151313, 0.0298729, 0.0459219, 0.0619393, 0.076451, 0.0880348, 0.0955087, 0.098091, 0.0955087, 0.0880348, 0.076451, 0.0619393, 0.0459219, 0.0298729, 0.0151313, 0.00274383, -0.00664117, -0.0128153, -0.0159848, -0.0166881, -0.0156723, -0.0137533, -0.0116822, -0.0100402, -0.00917447, -0.00918404, -0.00994914, -0.0111959, -0.0125796, -0.0137683, -0.0145126, -0.0146885, -0.0143081, -0.0135018, -0.0124761, -0.0114608, -0.0106561, -0.0101917, -0.0101053, -0.010343, -0.0107795, -0.0112526, -0.0116029, -0.0117094, -0.011514, -0.0110303, -0.0103356, -0.00954878, -0.00880131, -0.00820643, -0.00783479, -0.00770111, -0.00776415, -0.00793936, -0.00812057, -0.00820539, -0.00811841, -0.00782705, -0.00734698, -0.00673627, -0.00608027, -0.00547096, -0.00498574, -0.00467051, -0.00453056, -0.00453119, -0.00460728, -0.00467943, -0.00467274, -0.00453375, -0.00424193, -0.0038132, -0.003295, -0.00275443, -0.00226223, -0.00187658, -0.00163025, -0.00152401, -0.00152751, -0.00158725, -0.0016396, -0.00162564, -0.00150456, -0.00126243, -0.000914683, -0.0005017, -7.89273e-05, 0.000296217, 0.000577506, 0.000740151, 0.000785877, 0.000741816, 0.000653624, 0.000574554, 0.000553077, 0.000621911, 0.000790927, 0.00104545, 0.0013502, 0.00165789, 0.00192026, 0.00209924, 0.00217534, 0.00215178, 0.00205318, 0.00191952, 0.00179665, 0.00172568 };

//mkdeemph(8000,79,500)
float deemphasis_nfm_predefined_fir_8000[] = 
{ 1.43777e+11, 1.45874e+11, -4.67746e+11, 9.98433e+10, -1.47835e+12, -3.78799e+11, -2.61333e+12, -1.07042e+12, -3.41242e+12, -1.57042e+12, -3.34195e+12, -1.4091e+12, -1.96864e+12, -2.26212e+11, 8.48259e+11, 2.04875e+12, 4.80451e+12, 5.06875e+12, 9.09434e+12, 8.04571e+12, 1.24874e+13, 9.85837e+12, 1.35433e+13, 9.28407e+12, 1.09287e+13, 5.30975e+12, 3.76762e+12, -2.54809e+12, -8.06152e+12, -1.39895e+13, -2.37664e+13, -2.77865e+13, -4.16745e+13, -4.16797e+13, -5.94708e+13, -5.17628e+13, -7.46014e+13, -4.66449e+13, -8.47575e+13, 1.51722e+14, 4.98196e+14, 1.51722e+14, -8.47575e+13, -4.66449e+13, -7.46014e+13, -5.17628e+13, -5.94708e+13, -4.16797e+13, -4.16745e+13, -2.77865e+13, -2.37664e+13, -1.39895e+13, -8.06152e+12, -2.54809e+12, 3.76762e+12, 5.30975e+12, 1.09287e+13, 9.28407e+12, 1.35433e+13, 9.85837e+12, 1.24874e+13, 8.04571e+12, 9.09434e+12, 5.06875e+12, 4.80451e+12, 2.04875e+12, 8.48259e+11, -2.26212e+11, -1.96864e+12, -1.4091e+12, -3.34195e+12, -1.57042e+12, -3.41242e+12, -1.07042e+12, -2.61333e+12, -3.78799e+11, -1.47835e+12, 9.98433e+10, -4.67746e+11, 1.45874e+11, 1.43777e+11 };

float deemphasis_nfm_predefined_fir_44100[] = 
{ 0.0025158, 0.00308564, 0.00365507, 0.00413598, 0.00446279, 0.00461162, 0.00460866, 0.00452474, 0.00445739, 0.00450444, 0.00473648, 0.0051757, 0.0057872, 0.00648603, 0.00715856, 0.00769296, 0.00801081, 0.00809096, 0.00797853, 0.00777577, 0.00761627, 0.00762871, 0.00789987, 0.00844699, 0.00920814, 0.0100543, 0.0108212, 0.0113537, 0.011551, 0.0113994, 0.0109834, 0.0104698, 0.0100665, 0.00996618, 0.0102884, 0.0110369, 0.0120856, 0.0131998, 0.0140907, 0.0144924, 0.0142417, 0.0133401, 0.0119771, 0.0105043, 0.00935909, 0.00895022, 0.00952985, 0.0110812, 0.0132522, 0.015359, 0.0164664, 0.0155409, 0.0116496, 0.00416925, -0.00703664, -0.021514, -0.0382135, -0.0555955, -0.0718318, -0.0850729, -0.0937334, -0.0967458, -0.0937334, -0.0850729, -0.0718318, -0.0555955, -0.0382135, -0.021514, -0.00703664, 0.00416925, 0.0116496, 0.0155409, 0.0164664, 0.015359, 0.0132522, 0.0110812, 0.00952985, 0.00895022, 0.00935909, 0.0105043, 0.0119771, 0.0133401, 0.0142417, 0.0144924, 0.0140907, 0.0131998, 0.0120856, 0.0110369, 0.0102884, 0.00996618, 0.0100665, 0.0104698, 0.0109834, 0.0113994, 0.011551, 0.0113537, 0.0108212, 0.0100543, 0.00920814, 0.00844699, 0.00789987, 0.00762871, 0.00761627, 0.00777577, 0.00797853, 0.00809096, 0.00801081, 0.00769296, 0.00715856, 0.00648603, 0.0057872, 0.0051757, 0.00473648, 0.00450444, 0.00445739, 0.00452474, 0.00460866, 0.00461162, 0.00446279, 0.00413598, 0.00365507, 0.00308564, 0.0025158 };

//mkdeemph(11025,79,500)
float deemphasis_nfm_predefined_fir_11025[] =
{ 0.00113162, 0.000911207, 0.00173815, -0.000341385, -0.000849373, -0.00033066, -0.00290692, -0.00357326, -0.0031917, -0.00607078, -0.00659201, -0.00601551, -0.00886603, -0.00880243, -0.00759841, -0.0100344, -0.0088993, -0.00664423, -0.00835258, -0.00572919, -0.00214109, -0.00302443, 0.00132902, 0.00627003, 0.00596494, 0.0120731, 0.0180437, 0.0176243, 0.0253776, 0.0316572, 0.0298485, 0.0393389, 0.0446019, 0.0389943, 0.0516463, 0.0521951, 0.0350192, 0.0600945, 0.0163128, -0.217526, -0.378533, -0.217526, 0.0163128, 0.0600945, 0.0350192, 0.0521951, 0.0516463, 0.0389943, 0.0446019, 0.0393389, 0.0298485, 0.0316572, 0.0253776, 0.0176243, 0.0180437, 0.0120731, 0.00596494, 0.00627003, 0.00132902, -0.00302443, -0.00214109, -0.00572919, -0.00835258, -0.00664423, -0.0088993, -0.0100344, -0.00759841, -0.00880243, -0.00886603, -0.00601551, -0.00659201, -0.00607078, -0.0031917, -0.00357326, -0.00290692, -0.00033066, -0.000849373, -0.000341385, 0.00173815, 0.000911207, 0.00113162 };

// modified from: fmdemod_quadri_novect_cf in libcsdr.c from https://github.com/ha7ilm/csdr
#define DNFMFF_ADD_ARRAY(x) if(sample_rate==x) { taps=deemphasis_nfm_predefined_fir_##x; taps_length=sizeof(deemphasis_nfm_predefined_fir_##x)/sizeof(float); }
/*
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
*/

void deemphasis_nfm_ff(float* input, float* output, int input_size, int sample_rate) {
  /*
      Warning! This only works on predefined samplerates, as it uses fixed FIR coefficients defined in predefined.h
      However, there are the octave commands to generate the taps for your custom (fixed) sample rate.
      What it does:
          - reject below 400 Hz
          - passband between 400 HZ - 4 kHz, but with 20 dB/decade rolloff (for deemphasis)
          - reject everything above 4 kHz
  */

  float* taps;
  int taps_length=0;

  DNFMFF_ADD_ARRAY(48000)
  DNFMFF_ADD_ARRAY(44100)
  DNFMFF_ADD_ARRAY(8000)
  DNFMFF_ADD_ARRAY(11025)

  if(!taps_length) return;

  //float* taps = deemphasis_nfm_predefined_fir_48000;
  //int taps_length = sizeof(deemphasis_nfm_predefined_fir_48000) / sizeof(float) / 2;

  for(int i = 0; i < input_size - taps_length; i++)
  {
    float acc=0;
    for(int ti = 0; ti < taps_length; ti++) {
      acc += taps[ti] * input[i+ti];
    }
    output[i]=acc;
  }
}
