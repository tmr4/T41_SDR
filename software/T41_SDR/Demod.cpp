#include "SDT.h"
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
// Forwards
//-------------------------------------------------------------------------------------------------------------

float ApproxAtan2(float y, float x);

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
    //if (bands[current_band].mode == DEMOD_SAM_STEREO)
    //{
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
