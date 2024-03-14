#include "SDT.h"
#include "Button.h"
#include "Display.h"
#include "EEPROM.h"
#include "Filter.h"
#include "FIR.h"
#include "Tune.h"
#include "Utility.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

#define NOTHING_TO_SEE_HERE         950     // If the analog pin is greater than this value, nothing's going on
#define TMS0_POWER_DOWN_MASK        (0x1U)
#define TMS1_MEASURE_FREQ(x)        (((uint32_t)(((uint32_t)(x)) << 0U)) & 0xFFFFU)
#define TEMPMON_ROOMTEMP    25.0f

uint8_t display_dbm = DISPLAY_S_METER_DBM; // DISPLAY_S_METER_DBM or DISPLAY_S_METER_DBMHZ
int old_demod_mode = -99;

// T41 Switch Labels
const char *labels[] = { "Select", "Menu Up", "Band Up",
                         "Zoom", "Menu Dn", "Band Dn",
                         "Filter", "DeMod", "Mode",
                         "NR", "Notch", "Noise Floor",
                         "Fine Tune", "Decoder", "Tune Increment",
                         "Reset Tuning", "Frequ Entry", "User 2" };

float32_t cosBuffer2[256];
float32_t cosBuffer3[256];
float32_t cosBuffer4[256];

float32_t sinBuffer[256];
float32_t sinBuffer2[256];
float32_t sinBuffer3[256];
float32_t sinBuffer4[256];

// Voltage in one-hundred 1 dB steps for volume control.
const float32_t volumeLog[] = { 0.000010, 0.000011, 0.000013, 0.000014, 0.000016, 0.000018, 0.000020, 0.000022, 0.000025, 0.000028,
                                0.000032, 0.000035, 0.000040, 0.000045, 0.000050, 0.000056, 0.000063, 0.000071, 0.000079, 0.000089,
                                0.000100, 0.000112, 0.000126, 0.000141, 0.000158, 0.000178, 0.000200, 0.000224, 0.000251, 0.000282,
                                0.000316, 0.000355, 0.000398, 0.000447, 0.000501, 0.000562, 0.000631, 0.000708, 0.000794, 0.000891,
                                0.001000, 0.001122, 0.001259, 0.001413, 0.001585, 0.001778, 0.001995, 0.002239, 0.002512, 0.002818,
                                0.003162, 0.003548, 0.003981, 0.004467, 0.005012, 0.005623, 0.006310, 0.007079, 0.007943, 0.008913,
                                0.010000, 0.011220, 0.012589, 0.014125, 0.015849, 0.017783, 0.019953, 0.022387, 0.025119, 0.028184,
                                0.031623, 0.035481, 0.039811, 0.044668, 0.050119, 0.056234, 0.063096, 0.070795, 0.079433, 0.089125,
                                0.100000, 0.112202, 0.125893, 0.141254, 0.158489, 0.177828, 0.199526, 0.223872, 0.251189, 0.281838,
                                0.316228, 0.354813, 0.398107, 0.446684, 0.501187, 0.562341, 0.630957, 0.707946, 0.794328, 0.891251, 1.000000 };

//-------------------------------------------------------------------------------------------------------------
// Forwards
//-------------------------------------------------------------------------------------------------------------

// the following functions are not used anywhere
// float32_t arm_atan2_f32(float32_t y, float32_t x);
// int Xmit_IQ_Cal();

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

/*****
  Purpose: Generate Array with variable sinewave frequency tone AFP 05-17-22
  Parameter list:
    void
  Return value;
    void
*****/
void sineTone(int numCycles) {
  float theta;
  float freqSideTone2;
  float freqSideTone3 = 3000;         // Refactored 32 * 24000 / 256; //AFP 2-7-23
  float freqSideTone4 = 375;   
  freqSideTone2 = numCycles * 24000 / 256;
  for (int kf = 0; kf < 256; kf++) { //Calc: numCycles=8, 750 hz sine wave.
    theta = kf * 0.19634950849362;    // Simplify terms: theta = kf * 2 * PI * freqSideTone / 24000  JJP 6/28/23
    sinBuffer[kf] = sin(theta);
    theta = kf * 2 * PI * freqSideTone2 / 24000;
    sinBuffer2[kf] = sin(theta);
    cosBuffer2[kf] = cos(theta);
    theta = kf * 2.0 * PI * freqSideTone3 / 24000;
    sinBuffer3[kf] = sin(theta);
    cosBuffer3[kf] = cos(theta);
    theta = kf * 2.0 * PI * freqSideTone4 / 24000;
    sinBuffer4[kf] = sin(theta);
    cosBuffer4[kf] = cos(theta);
  }
}


const float32_t atanTable[68] = {
  -0.015623728620477f,
  0.000000000000000f,  // = 0 for in = 0.0
  0.015623728620477f,
  0.031239833430268f,
  0.046840712915970f,
  0.062418809995957f,
  0.077966633831542f,
  0.093476781158590f,
  0.108941956989866f,
  0.124354994546761f,
  0.139708874289164f,
  0.154996741923941f,
  0.170211925285474f,
  0.185347949995695f,
  0.200398553825879f,
  0.215357699697738f,
  0.230219587276844f,
  0.244978663126864f,
  0.259629629408258f,
  0.274167451119659f,
  0.288587361894077f,
  0.302884868374971f,
  0.317055753209147f,
  0.331096076704132f,
  0.345002177207105f,
  0.358770670270572f,
  0.372398446676754f,
  0.385882669398074f,
  0.399220769575253f,
  0.412410441597387f,
  0.425449637370042f,
  0.438336559857958f,
  0.451069655988523f,
  0.463647609000806f,
  0.476069330322761f,
  0.488333951056406f,
  0.500440813147294f,
  0.512389460310738f,
  0.524179628782913f,
  0.535811237960464f,
  0.547284380987437f,
  0.558599315343562f,
  0.569756453482978f,
  0.580756353567670f,
  0.591599710335111f,
  0.602287346134964f,
  0.612820202165241f,
  0.623199329934066f,
  0.633425882969145f,
  0.643501108793284f,
  0.653426341180762f,
  0.663202992706093f,
  0.672832547593763f,
  0.682316554874748f,
  0.691656621853200f,
  0.700854407884450f,
  0.709911618463525f,
  0.718829999621625f,
  0.727611332626511f,
  0.736257428981428f,
  0.744770125716075f,
  0.753151280962194f,
  0.761402769805578f,
  0.769526480405658f,
  0.777524310373348f,
  0.785398163397448f,  // = pi/4 for in = 1.0
  0.793149946109655f,
  0.800781565178043f
};

/*****
  Purpose: Generate Array with variable sinewave frequency tone
  Parameter list:
    void
  Return value;
    void
*****/
/*void SinTone(long freqSideTone) { // AFP 10-25-22
  float theta;
  for (int kf = 0; kf < 255; kf++) { //Calc 750 hz sine wave.  use 750 because it is 8 whole cycles in 256 buffer.
    theta = kf * 2 * PI * freqSideTone / 24000;
    sinBuffer2[kf] = sin(theta);
  }
  }*/

/*****
  Purpose: Correct Phase angle between I andQ channels
  Parameter list:
    void
  Return value;
    void
*****/
void IQPhaseCorrection(float32_t *I_buffer, float32_t *Q_buffer, float32_t factor, uint32_t blocksize) {
  float32_t temp_buffer[blocksize];
  if (factor < 0.0) {                                                             // mix a bit of I into Q
    arm_scale_f32 (I_buffer, factor, temp_buffer, blocksize);
    arm_add_f32 (Q_buffer, temp_buffer, Q_buffer, blocksize);
  } else {                                                      // mix a bit of Q into I
    arm_scale_f32 (Q_buffer, factor, temp_buffer, blocksize);
    arm_add_f32 (I_buffer, temp_buffer, I_buffer, blocksize);
  }
} // end IQphase_correction

/*****
  Purpose: Calculate sinc function

  Parameter list:
    void
  Return value;
    void
*****/
float MSinc(int m, float fc) {
  float x = m * PIH;
  if (m == 0)
    return 1.0f;
  else
    return sinf(x * fc) / (fc * x);
}

/*****
  Purpose: Izero

  Parameter list:
    void
  Return value;
    void
*****/
float32_t Izero(float32_t x) {
  float32_t x2          = x / 2.0;
  float32_t summe       = 1.0;
  float32_t ds          = 1.0;
  float32_t di          = 1.0;
  float32_t errorlimit  = 1e-9;
  float32_t tmp;

  do
  {
    tmp = x2 / di;
    tmp *= tmp;
    ds *= tmp;
    summe += ds;
    di += 1.0;
  } while (ds >= errorlimit * summe);
  return (summe);
}  // END Izero

/*****
  Purpose:    Fast algorithm for log10
              This is a fast approximation to log2()
              Y = C[0]*F*F*F + C[1]*F*F + C[2]*F + C[3] + E;
              log10f is exactly log2(x)/log2(10.0f)
              Math_log10f_fast(x) =(log2f_approx(x)*0.3010299956639812f)

  Parameter list:
    float32_t X       number for conversion

  Return value;
    void
*****/
float32_t log10f_fast(float32_t X) {
  float Y, F;
  int E;
  F = frexpf(fabsf(X), &E);
  Y = 1.23149591368684f;
  Y *= F;
  Y += -4.11852516267426f;
  Y *= F;
  Y += 6.02197014179219f;
  Y *= F;
  Y += -3.13396450166353f;
  Y += E;
  return (Y * 0.3010299956639812f);
}

/*****
  Purpose: Fast approximation to the trigonometric atan2 function for floating-point data.

  Parameter list:
    x input value           Inputs
    y input value

  Return value;
    atan2(y, x) = atan(y/x) as radians.
*****/
float32_t arm_atan2_f32(float32_t y, float32_t x) {
  float32_t atan2Val, fract, in;                 /* Temporary variables for input, output */
  uint32_t index;                                /* Index variable */
  uint32_t tableSize = (uint32_t) TABLE_SIZE_64; /* Initialise tablesize */
  float32_t wa, wb, wc, wd;                      /* Cubic interpolation coefficients */
  float32_t a, b, c, d;                          /* Four nearest output values */
  float32_t *tablePtr;                           /* Pointer to table */
  uint8_t flags = 0;                             /* flags providing information about input values:
                                                    Bit0 = 1 if |x| < |y|
                                                    Bit1 = 1 if x < 0
                                                    Bit2 = 1 if y < 0 */

  /* calculate magnitude of input values */
  if (x < 0.0f) {
    x = -x;
    flags |= 0x02;
  }

  if (y < 0.0f) {
    y = -y;
    flags |= 0x04;
  }

  /* calculate in value for LUT [0 1] */
  if (x < y) {
    in = x / y;
    flags |= 0x01;
  } else {                /* x >= y */
    if (x > 0.0f)
      in = y / x;
    else                  /* both are 0.0 */
      in = 0.0;           /* prevent division by 0 */
  }

  /* Calculation of index of the table */
  index = (uint32_t) (tableSize * in);

  /* fractional value calculation */
  fract = ((float32_t) tableSize * in) - (float32_t) index;

  /* Initialise table pointer */
  tablePtr = (float32_t *) & atanTable[index];

  /* Read four nearest values of output value from the sin table */
  a = *tablePtr++;
  b = *tablePtr++;
  c = *tablePtr++;
  d = *tablePtr++;

  /* Cubic interpolation process */
  wa = -(((0.166666667f) * (fract * (fract * fract))) +
         ((0.3333333333333f) * fract)) + ((0.5f) * (fract * fract));
  wb = (((0.5f) * (fract * (fract * fract))) -
        ((fract * fract) + ((0.5f) * fract))) + 1.0f;
  wc = (-((0.5f) * (fract * (fract * fract))) +
        ((0.5f) * (fract * fract))) + fract;
  wd = ((0.166666667f) * (fract * (fract * fract))) -
       ((0.166666667f) * fract);

  atan2Val = ((a * wa) + (b * wb)) + ((c * wc) + (d * wd));     /* Calculate atan2 value */

  if (flags & 0x01)                                             /* exchanged input values? */

    atan2Val = 1.5707963267949f - atan2Val;                     /* output = pi/2 - output */

  if (flags & 0x02)
    atan2Val = 3.14159265358979f - atan2Val;                    /* negative x input? Quadrant 2 or 3 */

  if (flags & 0x04)
    atan2Val = - atan2Val;                                      /* negative y input? Quadrant 3 or 4 */

  return (atan2Val);                                            /* Return the output value */
}

/*****
  Purpose:
  Parameter list:
    float32_t inphase
    float32_t quadrature

  Return value;
    float32_t
*****/
float32_t AlphaBetaMag(float32_t  inphase, float32_t  quadrature) {
  // (c) András Retzler
  // taken from libcsdr: https://github.com/simonyiszk/csdr
  // Min RMS Err      0.947543636291 0.392485425092
  // Min Peak Err     0.960433870103 0.397824734759
  // Min RMS w/ Avg=0 0.948059448969 0.392699081699
  const float32_t alpha = 0.960433870103; // 1.0; //0.947543636291;
  const float32_t beta =  0.397824734759;

  float32_t abs_inphase = fabs(inphase);
  float32_t abs_quadrature = fabs(quadrature);
  if (abs_inphase > abs_quadrature) {
    return alpha * abs_inphase + beta * abs_quadrature;
  } else {
    return alpha * abs_quadrature + beta * abs_inphase;
  }
}

/*****
  Purpose: copied from https://www.dsprelated.com/showarticle/1052.php
           Polynomial approximating arctangenet on the range -1,1.
           Max error < 0.005 (or 0.29 degrees)

  Parameter list:
    float z         value to approximate

  Return value;
    float           atan vakye
*****/
float ApproxAtan(float z) {
  const float n1 = 0.97239411f;
  const float n2 = -0.19194795f;
  return (n1 + n2 * z * z) * z;
}

/*****
  Purpose: function reads the analog value for each matrix switch and stores that value in EEPROM.

  Parameter list:
    void

  Return value;
    void
*****/
void SaveAnalogSwitchValues() {
  int index;
  int minVal;
  int value;
  int origRepeatDelay;

  tft.clearMemory();  // Need to clear overlay too
  tft.writeTo(L2);
  tft.fillWindow();
  tft.writeTo(L1);
  tft.clearScreen(RA8875_BLACK);
  tft.setFontScale(1);
  tft.setTextColor(RA8875_GREEN);
  tft.setCursor(10, 10);
  tft.print("Press button you");
  tft.setCursor(10, 30);
  tft.print("have assigned to");
  tft.setCursor(10, 50);
  tft.print("the switch shown.");

  // Disable button repeat for interrupt driven buttons
  origRepeatDelay = EEPROMData.buttonRepeatDelay;
  EEPROMData.buttonRepeatDelay = 0;

  for (index = 0; index < NUMBER_OF_SWITCHES;) {
    tft.setCursor(20, 100);
    tft.print(index + 1);
    tft.print(". ");
    tft.print(labels[index]);

    if (buttonInterruptsEnabled) {
      while ((value = ReadSelectedPushButton()) == -1) {
        // Wait until a button is pressed
      }
    } else {
      value = -1;
      minVal = NOTHING_TO_SEE_HERE;
      while (true) {
        value = ReadSelectedPushButton();
        if (value < NOTHING_TO_SEE_HERE && value > 0) {
          MyDelay(100L);
          if (value < minVal) {
            minVal = value;
          } else {
            value = minVal;
            break;
          }
        }
      }
    }

    tft.fillRect(20, 100, 300, 40, RA8875_BLACK);
    tft.setCursor(350, 20 + index * 25);
    tft.print(index + 1);
    tft.print(". ");
    tft.print(labels[index]);
    tft.setCursor(660, 20 + index * 25);
    tft.print(value);
    EEPROMData.switchValues[index] = value;

    // Set interrupt press/release thresholds based on the Select button, which has the highest ADC value
    if (index == 0) {
      EEPROMData.buttonThresholdPressed = EEPROMData.switchValues[0] + WIGGLE_ROOM;
      EEPROMData.buttonThresholdReleased = EEPROMData.buttonThresholdPressed + WIGGLE_ROOM;
    }

    index++;
    while ((value = ReadSelectedPushButton()) != -1 && value < NOTHING_TO_SEE_HERE) {
      // Wait until the button is released
    }
  }

  EEPROMData.buttonRepeatDelay = origRepeatDelay;  // Restore original repeat delay
}

// ================== Clock stuff
/*****
  Purpose: DisplayClock()
  Parameter list:
    void
  Return value;
    void
*****/
void DisplayClock() {
  char timeBuffer[15];
  char temp[5];

  temp[0]       = '\0';
  timeBuffer[0] = '\0';
  strcpy(timeBuffer, MY_TIMEZONE);         // e.g., EST
#ifdef TIME_24H
  //DB2OO, 29-AUG-23: use 24h format
  itoa(hour(), temp, DEC);
#else
  itoa(hourFormat12(), temp, DEC);
#endif
  if (strlen(temp) < 2) {
    strcat(timeBuffer, "0");
  }
  strcat(timeBuffer, temp);
  strcat(timeBuffer, ":");

  itoa(minute(), temp, DEC);
  if (strlen(temp) < 2) {
    strcat(timeBuffer, "0");
  }
  strcat(timeBuffer, temp);
  strcat(timeBuffer, ":");

  itoa(second(), temp, DEC);
  if (strlen(temp) < 2) {
    strcat(timeBuffer, "0");
  }
  strcat(timeBuffer, temp);

  tft.setFontScale( (enum RA8875tsize) 1);

  tft.fillRect(TIME_X - 20, TIME_Y, XPIXELS - TIME_X - 1, CHAR_HEIGHT, RA8875_BLACK);
  tft.setCursor(TIME_X - 20, TIME_Y);
  tft.setTextColor(RA8875_WHITE);
  tft.print(timeBuffer);
}                                                   // end function displayTime

// ============== Mode stuff

/*****
  Purpose: SetupMode sets default mode for the selected band

  Parameter list:
    int sideBand            the sideband

  Return value;
    void
*****/
void SetupMode(int sideBand) {
  int temp;

  if (old_demod_mode != -99)                                    // first time radio is switched on and when changing bands
  {
    switch (sideBand) {
      case DEMOD_USB:
        temp = bands[currentBand].FHiCut;
        bands[currentBand].FHiCut = - bands[currentBand].FLoCut;
        bands[currentBand].FLoCut = - temp;
        break;

      case DEMOD_LSB:
        temp = bands[currentBand].FHiCut;
        bands[currentBand].FHiCut = - bands[currentBand].FLoCut;
        bands[currentBand].FLoCut = - temp;
        break;

      case DEMOD_AM:
      case DEMOD_NFM:
        bands[currentBand].FHiCut =  -bands[currentBand].FLoCut;
        break;
    }
  }

  ShowBandwidth();
  old_demod_mode = bands[currentBand].mode; // set old_mode flag for next time, at the moment only used for first time radio is switched on . . .
}

int Xmit_IQ_Cal() {
  return -1;
}


/*****
  Purpose: set Band
  Parameter list:
    void
  Return value;
    void
*****/
void SetBand() {
  old_demod_mode = -99; // used in setup_mode and when changing bands, so that LoCut and HiCut are not changed!
  SetupMode(bands[currentBand].mode);
  SetFreq();
  ShowFrequency();
  FilterBandwidth();
}

/*****
  Purpose: Tries to open the EEPROM SD file to see if an SD card is present in the system

  Parameter list:
    void

  Return value;
    int               0 = SD not initialized, 1 = has data
*****/
int SDPresentCheck() {
  int retVal = 0;

  if(SD.begin(BUILTIN_SDCARD)) {
    // open the file.
    File dataFile = SD.open("SDEEPROMData.txt");

    if(dataFile) {
      retVal = 1;
    }

    dataFile.close();
  } else {
    Serial.print("No SD card or cannot be initialized.");
    tft.setFontScale((enum RA8875tsize)1);
    tft.setForegroundColor(RA8875_RED);
    tft.setCursor(20, 300);
    tft.print("No SD card or not initialized.");
    tft.setForegroundColor(RA8875_WHITE);
  }

  return retVal;
}

double elapsed_micros_idx_t = 0;
double elapsed_micros_sum;

/*****
  Purpose: Display the current temperature and load figures for T4.1

  Parameter list:
    int notchF        the notch to use
    int MODE          the current MODE

  Return value;
    void
*****/
void ShowTempAndLoad() {
  char buff[10];
  int valueColor = RA8875_GREEN;
  double block_time;
  double processor_load;
  float CPU_temperature;
  double elapsed_micros_mean;

  elapsed_micros_mean = elapsed_micros_sum / elapsed_micros_idx_t;

  block_time = 128.0 / (double)SampleRate;  // one audio block is 128 samples and uses this in seconds
  block_time = block_time * N_BLOCKS;

  block_time *= 1000000.0;                                  // now in µseconds
  processor_load = elapsed_micros_mean / block_time * 100;  // take audio processing time divide by block_time, convert to %

  if (processor_load >= 100.0) {
    processor_load = 100.0;
    valueColor = RA8875_RED;
  }

  tft.setFontScale((enum RA8875tsize)0);

  CPU_temperature = TGetTemp();

  tft.fillRect(TEMP_X_OFFSET, TEMP_Y_OFFSET, MAX_WATERFALL_WIDTH, tft.getFontHeight(), RA8875_BLACK);  // Erase current data
  tft.setCursor(TEMP_X_OFFSET, TEMP_Y_OFFSET);
  tft.setTextColor(RA8875_WHITE);
  tft.print("Temp:");
  tft.setCursor(TEMP_X_OFFSET + 120, TEMP_Y_OFFSET);
  tft.print("Load:");

  tft.setTextColor(valueColor);
  MyDrawFloat(CPU_temperature, 1, TEMP_X_OFFSET + tft.getFontWidth() * 3, TEMP_Y_OFFSET, buff);

  tft.drawCircle(TEMP_X_OFFSET + 80, TEMP_Y_OFFSET + 5, 3, RA8875_GREEN);
  MyDrawFloat(processor_load, 1, TEMP_X_OFFSET + 150, TEMP_Y_OFFSET, buff);
  tft.print("%");
  elapsed_micros_idx_t = 0;
  elapsed_micros_sum = 0;
  elapsed_micros_mean = 0;
  tft.setTextColor(RA8875_WHITE);
}

/*****
  Purpose: to cause a delay in program execution

  Parameter list:
    unsigned long millisWait    // the number of millseconds to wait

  Return value:
    void
*****/
void MyDelay(unsigned long millisWait) 
{
  unsigned long now = millis();

  while (millis() - now < millisWait)
    ;  // Twiddle thumbs until delay ends...
}

uint32_t roomCount;      // !< The value of TEMPMON_TEMPSENSE0[TEMP_VALUE] at the hot temperature
uint32_t s_roomC_hotC;   // !< The value of s_roomCount minus s_hotCount
uint32_t s_hotTemp;      // !< The value of TEMPMON_TEMPSENSE0[TEMP_VALUE] at room temperature 
uint32_t s_hotCount;     // !< The value of TEMPMON_TEMPSENSE0[TEMP_VALUE] at the hot temperature
float s_hotT_ROOM;       // !< The value of s_hotTemp minus room temperature(25¡æ)

/*****
  Purpose: Read the Teensy's temperature. Get worried over 50C

  Parameter list:
    void

  Return value:
    float           temperature Centigrade
*****/
float TGetTemp() {
  uint32_t nmeas;
  float tmeas;
  while (!(TEMPMON_TEMPSENSE0 & 0x4U)) {
    ;
  }
  /* ready to read temperature code value */
  nmeas = (TEMPMON_TEMPSENSE0 & 0xFFF00U) >> 8U;
  tmeas = s_hotTemp - (float)((nmeas - s_hotCount) * s_hotT_ROOM / s_roomC_hotC);  // Calculate temperature
  return tmeas;
}

/*****
  Purpose: void initTempMon

  Parameter list:
    void
  Return value;
    void
*****/
void initTempMon(uint16_t freq, uint32_t lowAlarmTemp, uint32_t highAlarmTemp, uint32_t panicAlarmTemp) {

  uint32_t calibrationData;
  uint32_t roomCount;
  //first power on the temperature sensor - no register change
  TEMPMON_TEMPSENSE0 &= ~TMS0_POWER_DOWN_MASK;
  TEMPMON_TEMPSENSE1 = TMS1_MEASURE_FREQ(freq);

  calibrationData = HW_OCOTP_ANA1;
  s_hotTemp = (uint32_t)(calibrationData & 0xFFU) >> 0x00U;
  s_hotCount = (uint32_t)(calibrationData & 0xFFF00U) >> 0X08U;
  roomCount = (uint32_t)(calibrationData & 0xFFF00000U) >> 0x14U;
  s_hotT_ROOM = s_hotTemp - TEMPMON_ROOMTEMP;
  s_roomC_hotC = roomCount - s_hotCount;
}

/*****
  Purpose: Format frequency for printing
  Parameter list:
    void
  Return value;
    void
    // show frequency
*****/
void FormatFrequency(long freq, char *freqBuffer) {
  char outBuffer[15];
  int i;
  int len;
  ltoa((long)freq, outBuffer, 10);
  len = strlen(outBuffer);

  switch (len) {
    case 6:  // below 530.999 KHz
      freqBuffer[0] = outBuffer[0];
      freqBuffer[1] = outBuffer[1];
      freqBuffer[2] = outBuffer[2];
      freqBuffer[3] = FREQ_SEP_CHARACTER;  // Add separation charcter
      for (i = 4; i < len; i++) {
        freqBuffer[i] = outBuffer[i - 1];  // Next 3 digit chars
      }
      freqBuffer[i] = '0';       // trailing 0
      freqBuffer[i + 1] = '\0';  // Make it a string
      break;

    case 7:  // 1.0 - 9.999 MHz
      freqBuffer[0] = outBuffer[0];
      freqBuffer[1] = FREQ_SEP_CHARACTER;  // Add separation charcter
      for (i = 2; i < 5; i++) {
        freqBuffer[i] = outBuffer[i - 1];  // Next 3 digit chars
      }
      freqBuffer[5] = FREQ_SEP_CHARACTER;  // Add separation charcter
      for (i = 6; i < 9; i++) {
        freqBuffer[i] = outBuffer[i - 2];  // Last 3 digit chars
      }
      freqBuffer[i] = '\0';  // Make it a string
      break;

    case 8:  // 10 MHz - 30MHz
      freqBuffer[0] = outBuffer[0];
      freqBuffer[1] = outBuffer[1];
      freqBuffer[2] = FREQ_SEP_CHARACTER;  // Add separation charcter
      for (i = 3; i < 6; i++) {
        freqBuffer[i] = outBuffer[i - 1];  // Next 3 digit chars
      }
      freqBuffer[6] = FREQ_SEP_CHARACTER;  // Add separation charcter
      for (i = 7; i < 10; i++) {
        freqBuffer[i] = outBuffer[i - 2];  // Last 3 digit chars
      }
      freqBuffer[i] = '\0';  // Make it a string
      break;
  }
}
