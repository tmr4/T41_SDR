#include "SDT.h"
#include "Bearing.h"
#include "Button.h"
#include "ButtonProc.h"
#include "CW_Excite.h"
#include "CWProcessing.h"
#include "Display.h"
#include "DSP_Fn.h"
#include "Encoders.h"
#include "EEPROM.h"
#include "Exciter.h"
#include "FFT.h"
#include "Filter.h"
#include "ft8.h"
#include "InfoBox.h"
#include "Menu.h"
#include "Noise.h"
#include "Process.h"
#include "Tune.h"
#include "Utility.h"

//-------------------------------------------------------------------------------------------------------------
/*
  The T41 Display Functions:
  The key to updating the T41 display efficiently is to know the areas of the display, the functions that update
  them, and when these are called during each loop.

  Dynamic Areas:
    Several areas, the frequency and audio spectrums, waterfall, and filter markers in the audio spectrum box
    are updated dynamically through a loop call to ShowSpectrum.  These areas can't be updated individually.
    The the S-meter bar is also updated each loop but has its own update function, DrawSmeterBar.  The
    transmit/receive status indicator (ShowTransmitReceiveStatus) is also updated each loop with a state change.

  Static Areas:
    Static areas of the display usually don't change so these functions only need called once on startup or
    when that area of the display is used for another purpose:
      ShowName
      DrawSpectrumFrame
      DrawSMeterContainer
      DrawAudioSpectContainer
      DrawInfoBoxFrame

  Other Areas:
    All other areas are updated in response to user interaction with the radio, whether by encoder, button or
    menu.  Some encoder actions are accumulated and/or processed each loop during the call to ShowSpectrum.
    These include changes to the tuned frequency (center or fine tuned), filter bandwidth (position or width).
    The functions that update the display for these are:
      ShowFrequency           - writes VFO A and VFO B frequencies at the top of the display
      ShowOperatingStats      - writes the center frequency, band, mode, demod mode and power level
      DrawBandwidthBar        - draws the bandwith bar on the frequency spectrum
      ShowBandwidthBarValues  - writes bandwidth values above the bandwidth bar
      ShowSpectrumFreqValues  - writes fequency markers below sprectrum box

    Some frequency changes resulting from button interaction are updated during the call to ShowSpectrum as well.
    These include:
      ButtonFrequencyEntry - does this now, but this should be changed.
      
    Info box items are updated individually in response to user interaction with the radio.  The entire box only
    needs redrawn when its area has been used for other purposes.

*/
//-------------------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

#define NEW_SI5351_FREQ_MULT  1UL
#define FLOAT_PRECISION         6             // Assumed precision for a float
#define RIGNAME_X_OFFSET      570             // Pixel count to rig name field

//------------------------- Global Variables ----------

int filterWidth;
int centerLine = (SPECTRUM_RES + SPECTRUM_LEFT_X) / 2;

int16_t pixelCurrent[SPECTRUM_RES];
int16_t pixelnew[SPECTRUM_RES];
int16_t pixelold[SPECTRUM_RES];
int16_t pixelnew2[SPECTRUM_RES + 1];
int16_t pixelold2[SPECTRUM_RES];
int newCursorPosition = 0;
int oldCursorPosition = 256;
int updateDisplayFlag = 1;
int wfRows = WATERFALL_H;

#ifndef RA8875_DISPLAY
ILI9488_t3 tft = ILI9488_t3(&SPI, TFT_CS, TFT_DC, TFT_RST);
#else
#define RA8875_CS TFT_CS
#define RA8875_RESET TFT_DC  // any pin or nothing!
RA8875 tft = RA8875(RA8875_CS, RA8875_RESET);
#endif

dispSc displayScale[] =
{
  // *dbText,dBScale, pixelsPerDB, baseOffset, offsetIncrement
  { "20 dB/", 10.0, 2, 24, 1.00 },
  { "10 dB/", 20.0, 4, 10, 0.50 },
  { "5 dB/", 40.0, 8, 58, 0.25 },
  { "2 dB/", 100.0, 20, 120, 0.10 },
  { "1 dB/", 200.0, 40, 200, 0.05 }
};

int newSpectrumFlag = 0; // 0 - oldNF needs initialized in ShowSpectrum(), 1 - it doesn't need initialized

//------------------------- Local Variables ----------
uint8_t twinpeaks_tested = 2;  // this is never changed
uint8_t write_analog_gain = 0; // this is never changed
int16_t pos_x_time = 390;
int16_t pos_y_time = 5;
int16_t spectrum_x = 10;
float xExpand = 1.4;
int attenuator = 0;

const uint16_t gradient[] = {  // Color array for waterfall background
  0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9,
  0x10, 0x1F, 0x11F, 0x19F, 0x23F, 0x2BF, 0x33F, 0x3BF, 0x43F, 0x4BF,
  0x53F, 0x5BF, 0x63F, 0x6BF, 0x73F, 0x7FE, 0x7FA, 0x7F5, 0x7F0, 0x7EB,
  0x7E6, 0x7E2, 0x17E0, 0x3FE0, 0x67E0, 0x8FE0, 0xB7E0, 0xD7E0, 0xFFE0, 0xFFC0,
  0xFF80, 0xFF20, 0xFEE0, 0xFE80, 0xFE40, 0xFDE0, 0xFDA0, 0xFD40, 0xFD00, 0xFCA0,
  0xFC60, 0xFC00, 0xFBC0, 0xFB60, 0xFB20, 0xFAC0, 0xFA80, 0xFA20, 0xF9E0, 0xF980,
  0xF940, 0xF8E0, 0xF8A0, 0xF840, 0xF800, 0xF802, 0xF804, 0xF806, 0xF808, 0xF80A,
  0xF80C, 0xF80E, 0xF810, 0xF812, 0xF814, 0xF816, 0xF818, 0xF81A, 0xF81C, 0xF81E,
  0xF81E, 0xF81E, 0xF81E, 0xF83E, 0xF83E, 0xF83E, 0xF83E, 0xF85E, 0xF85E, 0xF85E,
  0xF85E, 0xF87E, 0xF87E, 0xF83E, 0xF83E, 0xF83E, 0xF83E, 0xF85E, 0xF85E, 0xF85E,
  0xF85E, 0xF87E, 0xF87E, 0xF87E, 0xF87E, 0xF87E, 0xF87E, 0xF87E, 0xF87E, 0xF87E,
  0xF87E, 0xF87E, 0xF87E, 0xF87E, 0xF88F, 0xF88F, 0xF88F
};

uint16_t waterfall[WATERFALL_W];
int maxYPlot;
int filterWidthX;  // The current filter X.

struct DEMOD_Descriptor
{ const uint8_t DEMOD_n;
  const char* const text;
};
const DEMOD_Descriptor DEMOD[6] = {
  //   DEMOD_n, name
  { DEMOD_USB, "(USB)" },
  { DEMOD_LSB, "(LSB)" },
  { DEMOD_AM, "(AM)" },
  { DEMOD_NFM, "(NFM)" },
  { DEMOD_FT8_WAV, "(FT8.wav)" },
  { DEMOD_FT8, "(FT8)" },
};

Metro ms_500 = Metro(500);  // Set up a Metro

//-------------------------------------------------------------------------------------------------------------
// Forwards
//-------------------------------------------------------------------------------------------------------------

void DrawSMeterContainer();
void DrawAudioSpectContainer();

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

/*****
  Purpose: Show the program name and version number

  Parameter list:
    void

  Return value;
    void
*****/
void ShowName() {
  tft.fillRect(RIGNAME_X_OFFSET, 0, XPIXELS - RIGNAME_X_OFFSET, tft.getFontHeight(), RA8875_BLACK);

  tft.setFontScale((enum RA8875tsize)1);

  tft.setTextColor(RA8875_YELLOW);
  tft.setCursor(RIGNAME_X_OFFSET - 20, 1);
  tft.print(RIGNAME);
  tft.setFontScale(0);
  tft.print(" ");                  // Added to correct for deleted leading space 4/16/2022 JACK
#ifdef FOURSQRP                    // Give visual indication that were using the 4SQRP code.  W8TEE October 11, 2023.
  tft.setTextColor(RA8875_GREEN);  // Make it green
#else
  tft.setTextColor(RA8875_RED);  // Make it red
#endif
  tft.print(VERSION);
}

/*****
  Purpose: Show Spectrum display
            This routine calls the Audio process Function during each display cycle,
            for each of the 512 display frequency bins.  This means that the audio is
            refreshed at the maximum rate and does not have to wait for the display to
            complete drawing the full spectrum.  However, the display data are only
            updated ONCE during each full display cycle, ensuring consistent data for
            the erase/draw cycle at each frequency point.

  Parameter list:
    void

  Return value;
    void
*****/
FASTRUN void ShowSpectrum() {
  int filterLoPositionMarker;
  int filterHiPositionMarker;
  int filterLoPosition;
  int filterHiPosition;
  int y_new_plot, y1_new_plot, y_old_plot, y1_old_plot;
  static int oldNF;
  const int currentNF = currentNoiseFloor[currentBand]; // noise floor is constant for each spectrum update
  int filterLoColor;
  int filterHiColor;

  // initialize old noise floor if this is a new spectrum
  if(newSpectrumFlag == 0) {
    oldNF = currentNF;
    newSpectrumFlag = 1;
  }

  // Draw the main Spectrum, Waterfall and Audio displays
  for (int x1 = 0; x1 < SPECTRUM_RES - 1; x1++) {
    // Update the frequency here only.  This is the beginning of the 512 wide spectrum display
    if (x1 == 0) {
      // Set flag so the display FFTs are calculated only once during each display refresh cycle
      updateDisplayFlag = 1;
    } else {
      // Do not save the the display data for the remainder of the display update
      updateDisplayFlag = 0;
    }

    // update filters if changed
    if (posFilterEncoder != lastFilterEncoder || filter_pos_BW != last_filter_pos_BW) {
      SetBWFilters();
      
      ShowBandwidthBarValues();
      DrawBandwidthBar();
    }

    // Handle tuning changes
    // Done here to minimize interruption to signal stream during tuning.  There
    // may seem some duplication of display updates here, but these tuning events
    // shouldn't occur on the same loop so little efficiency to be gained by changing
    SetCenterTune();
    if(fineTuneFlag) {
      ShowFrequency();
      DrawBandwidthBar();
      fineTuneFlag = false;
    }
    if(resetTuningFlag) {
      resetTuningFlag = false; // DrawBandwidthBar relies on this being set prior to the ResetTuning call
      ResetTuning();
    }

    if(T41State == SSB_RECEIVE || T41State == CW_RECEIVE) {
      // Call the Audio process from within the display routine to eliminate conflicts with drawing the spectrum and waterfall displays
      ProcessIQData();
    }

    // pixelold spectrum is saved by the FFT function prior to a new FFT which generates the pixelnew spectrum
    y_new_plot = spectrumNoiseFloor - pixelnew[x1] - currentNF;
    y1_new_plot = spectrumNoiseFloor - pixelnew[x1 + 1] - currentNF;
    y_old_plot = spectrumNoiseFloor - pixelold[x1] - oldNF;
    y1_old_plot = spectrumNoiseFloor - pixelold[x1 + 1] - oldNF;

    // Prevent spectrum from going below the bottom of the spectrum area
    if (y_new_plot > SPECTRUM_BOTTOM) y_new_plot = SPECTRUM_BOTTOM;
    if (y1_new_plot > SPECTRUM_BOTTOM) y1_new_plot = SPECTRUM_BOTTOM;
    if (y_old_plot > SPECTRUM_BOTTOM) y_old_plot = SPECTRUM_BOTTOM;
    if (y1_old_plot > SPECTRUM_BOTTOM) y1_old_plot = SPECTRUM_BOTTOM;

    // Prevent spectrum from going above the top of the spectrum area
    if (y_new_plot < SPECTRUM_TOP_Y) y_new_plot = SPECTRUM_TOP_Y;
    if (y1_new_plot < SPECTRUM_TOP_Y) y1_new_plot = SPECTRUM_TOP_Y;
    if (y_old_plot < SPECTRUM_TOP_Y) y_old_plot = SPECTRUM_TOP_Y;
    if (y1_old_plot < SPECTRUM_TOP_Y) y1_old_plot = SPECTRUM_TOP_Y;

    // Erase the old spectrum, and draw the new spectrum.
    tft.drawLine(SPECTRUM_LEFT_X + x1, y1_old_plot, SPECTRUM_LEFT_X + x1, y_old_plot, RA8875_BLACK);
    tft.drawLine(SPECTRUM_LEFT_X + x1, y1_new_plot, SPECTRUM_LEFT_X + x1, y_new_plot, RA8875_YELLOW);

    // What is the actual spectrum at this time?  It's a combination of the old and new spectrums
    // In the case of a CW interrupt, the array pixelnew should be saved as the actual spectrum
    // This is the actual "old" spectrum!  This is required due to CW interrupts
    // pixelCurrent gets copied to pixelold by the FFT function
    pixelCurrent[x1] = pixelnew[x1];  

    if (x1 < AUDIO_SPEC_BOX_W - 2) { // don't overwrite right edge of audio spectrum box
      if (keyPressedOn == 1) {
        return;
      } else {
        // erase old audio spectrum line at this position (including filter lines)
        tft.drawFastVLine(AUDIO_SPEC_BOX_L + x1 + 1, AUDIO_SPEC_BOX_T + 1, AUDIO_SPEC_BOX_H - 2, RA8875_BLACK);

        // draw current audio spectrum line at this position
        if (audioYPixel[x1] != 0) {
          // maintain spectrum within box
          if (audioYPixel[x1] > CLIP_AUDIO_PEAK)
          {
            audioYPixel[x1] = CLIP_AUDIO_PEAK;
          }
          tft.drawFastVLine(AUDIO_SPEC_BOX_L + x1 + 1, AUDIO_SPEC_BOTTOM - audioYPixel[x1] - 2, audioYPixel[x1], RA8875_MAGENTA);  // draw new AUDIO spectrum line
        }

        // draw fiter indicator lines on the audio spectrum (have to do this here or the filter lines will "blink")
        // abs prevents these from going below the bottom of the audio spectrum display but that the
        // resulting filter value isn't meaningful, should fix at the encoder
        filterLoPositionMarker = map(bands[currentBand].FLoCut, 0, 6400, 0, AUDIO_SPEC_BOX_W);
        filterHiPositionMarker = map(bands[currentBand].FHiCut, 0, 6400, 0, AUDIO_SPEC_BOX_W);
        filterLoPosition = abs(filterLoPositionMarker);
        filterHiPosition = abs(filterHiPositionMarker);

        // set color of active filter bar to green
        switch (bands[currentBand].mode) {
          case DEMOD_USB:
          case DEMOD_FT8: // ft8 is USB
            if (lowerAudioFilterActive) {
              filterLoColor = RA8875_GREEN;
              filterHiColor = RA8875_LIGHT_GREY;
            } else {
              filterLoColor = RA8875_LIGHT_GREY;
              filterHiColor = RA8875_GREEN;
            } 
            break;

          case DEMOD_LSB:
            if (lowerAudioFilterActive) {
              filterLoColor = RA8875_LIGHT_GREY;
              filterHiColor = RA8875_GREEN;
            } else {
              filterLoColor = RA8875_GREEN;
              filterHiColor = RA8875_LIGHT_GREY;
            } 
            break;

          case DEMOD_NFM:
            if (nfmBWFilterActive) {
              filterLoColor = RA8875_LIGHT_GREY;
              filterHiColor = RA8875_LIGHT_GREY;
            } else {
              if (lowerAudioFilterActive) {
                filterLoColor = RA8875_GREEN;
                filterHiColor = RA8875_LIGHT_GREY;
              } else {
                filterLoColor = RA8875_LIGHT_GREY;
                filterHiColor = RA8875_GREEN;
              } 
            }
            break;

          case DEMOD_AM:
          case DEMOD_SAM:
          default:
            filterLoColor = RA8875_LIGHT_GREY;
            filterHiColor = RA8875_GREEN;
            break;
        }

        // limit the filter line from going out of the spectrum box to the right
        if(filterLoPosition > 0 && filterLoPosition < (AUDIO_SPEC_BOX_W - 1)) {
          tft.drawFastVLine(AUDIO_SPEC_BOX_L + filterLoPosition, AUDIO_SPEC_BOX_T, AUDIO_SPEC_BOX_H - 1, filterLoColor);
        }
        if(filterHiPosition > 0 && filterHiPosition < (AUDIO_SPEC_BOX_W - 1)) {
          tft.drawFastVLine(AUDIO_SPEC_BOX_L + filterHiPosition, AUDIO_SPEC_BOX_T, AUDIO_SPEC_BOX_H - 1, filterHiColor);
        }
      }
    }

    // create data for waterfall
    int test1;
    test1 = -y_new_plot + 230;  // Nudged waterfall towards blue
    if (test1 < 0) test1 = 0;
    if (test1 > 117) test1 = 117;
    waterfall[x1] = gradient[test1];  // Try to put pixel values in middle of gradient array
  }

  // update S-meter once per loop
  DrawSmeterBar();

  pixelCurrent[SPECTRUM_RES - 1] = pixelnew[SPECTRUM_RES - 1];  

  oldNF = currentNF; // save the noise floor we used for this spectrum

  // scroll the waterfall display
  // Use the Block Transfer Engine (BTE) to move waterfall down a line
  if (keyPressedOn == 1) {
    return;
  } else {
    // copy the waterfall to layer 2, moving it down to row 2
    tft.BTE_move(WATERFALL_L, WATERFALL_T, WATERFALL_W, wfRows, WATERFALL_L, WATERFALL_T + 1, 1, 2);
    while (tft.readStatus())  // Make sure it is done.  Memory moves can take time.
      ;
    // copy the waterfall back to layer 1, row 2
    tft.BTE_move(WATERFALL_L, WATERFALL_T + 1, WATERFALL_W, wfRows, WATERFALL_L, WATERFALL_T + 1, 2);
    while (tft.readStatus())  // Make sure it's done.
      ;
  }

  // write new row of data into the top row to finish the scrolling effect
  tft.writeRect(WATERFALL_L, WATERFALL_T, WATERFALL_W, 1, waterfall);

  // update clock
  if (ms_500.check() == 1) {
    DisplayClock();
  }
}

/*****
  Purpose: this routine prints the frequency bars under the spectrum display
           and displays the bandwidth bar indicating demodulation bandwidth

  Parameter list:
    void

  Return value;
    void
*****/
void ShowBandwidthBarValues() {
  char buff[10];
  int centerLine = (SPECTRUM_RES + SPECTRUM_LEFT_X) / 2;
  int posLeft, posRight;
  //int hi_offset = 80;
  int loColor = RA8875_LIGHT_GREY;
  int hiColor = RA8875_LIGHT_GREY;
  //float32_t pixel_per_khz;
  float loValue = (float)(bands[currentBand].FLoCut / 1000.0f);
  float hiValue = (float)(bands[currentBand].FHiCut / 1000.0f);

  //pixel_per_khz = 0.0055652173913043;  // Al: I factored this constant: 512/92000;
  //pixel_per_khz = ((1 << spectrum_zoom) * SPECTRUM_RES * 1000.0 / SampleRate) ;
  //pos_left = centerLine + ((int)(bands[currentBand].FLoCut / 1000.0 * pixel_per_khz));
  //if (pos_left < spectrum_x) {
  //  pos_left = spectrum_x;
  //}

  // Need to add in code for zoom factor here

  //filterWidthX = pos_left + newCursorPosition - centerLine;

  tft.writeTo(L2); // switch to layer 2

  tft.setFontScale((enum RA8875tsize)0);
  posLeft = centerLine - tft.getFontWidth() * 9 - 10;
  posRight = centerLine - 10; // MyDrawFloat provides some padding I guess is intended to erase old value

  // erase old values (needed when mode changes, could just do it there)
  tft.fillRect(posLeft, FILTER_PARAMETERS_Y, 200, tft.getFontHeight(), RA8875_BLACK);

  // set color of active filter value to green
  switch (bands[currentBand].mode) {
    case DEMOD_USB:
    case DEMOD_FT8: // ft8 is USB
      if (lowerAudioFilterActive) {
        loColor = RA8875_GREEN;
      } else {
        hiColor = RA8875_GREEN;
      } 
      break;

    case DEMOD_LSB:
      if (lowerAudioFilterActive) {
        hiColor = RA8875_GREEN;
      } else {
        loColor = RA8875_GREEN;
      } 
      break;

    case DEMOD_NFM:
      if (nfmBWFilterActive) {
        hiColor = RA8875_GREEN;
      }
      hiValue = (float)(nfmFilterBW / 1000.0f);
      posRight = centerLine - tft.getFontWidth() * 5 - 4;
      //hiValue = (float)(nfmFilterBW / 2000.0f);
      //loValue = -hiValue;
      break;

    case DEMOD_AM:
    case DEMOD_SAM:
    default:
      loColor = RA8875_GREEN;
      hiColor = RA8875_GREEN;
      break;
  }

  if(bands[currentBand].mode != DEMOD_NFM) {
    tft.setTextColor(loColor);
    MyDrawFloat(loValue, 1, posLeft, FILTER_PARAMETERS_Y, buff);
    tft.print("kHz");
  }

  tft.setTextColor(hiColor);
  MyDrawFloat(hiValue, 1, posRight, FILTER_PARAMETERS_Y, buff);
  tft.print("kHz");

  tft.writeTo(L1); // return to layer 1
}

/*****
  Purpose: ShowSpectrumdBScale()
  Parameter list:
    void
  Return value;
    void
*****/
void ShowSpectrumdBScale() {
  tft.writeTo(L2);
  tft.setFontScale((enum RA8875tsize)0);

  //tft.fillRect(SPECTRUM_LEFT_X + 1, SPECTRUM_TOP_Y + 10, 33, tft.getFontHeight(), RA8875_BLACK);
  //tft.setCursor(SPECTRUM_LEFT_X + 5, SPECTRUM_TOP_Y + 10);
  tft.fillRect(SPECTRUM_LEFT_X + 1, FILTER_PARAMETERS_Y, 33, tft.getFontHeight(), RA8875_BLACK);
  tft.setCursor(SPECTRUM_LEFT_X + 5, FILTER_PARAMETERS_Y);
  tft.setTextColor(RA8875_WHITE);
  tft.print(displayScale[currentScale].dbText);
  tft.writeTo(L1);
}

/*****
  Purpose: This function draws the frequency bar at the bottom of the spectrum scope, putting markers at every
            graticule and the full frequency
            *** this can be more efficient by moving the tick marks to the static function, but this increases code size ***

  Parameter list:
    void

  Return value;
    void
*****/
void ShowSpectrumFreqValues() {
  char txt[16];

  int bignum;
  int centerIdx;
  int pos_help;
  float disp_freq;

  float freq_calc;
  float grat;
  int centerLine = SPECTRUM_RES / 2 + SPECTRUM_LEFT_X;
  // positions for graticules: first for spectrum_zoom < 3, then for spectrum_zoom > 2
  const static int idx2pos[2][9] = {
    { -43, 21, 50, 250, 140, 250, 232, 250, 315 },
    { -43, 21, 50, 85, 200, 200, 232, 218, 315 }
  };

  grat = (float)(SampleRate / 8000.0) / (float)(1 << spectrum_zoom);  // 1, 2, 4, 8, 16, 32, 64 . . . 4096

  tft.setTextColor(RA8875_WHITE);
  tft.setFontScale((enum RA8875tsize)0);

  // erase frequency bar values
  tft.fillRect(SPECTRUM_LEFT_X, SPEC_BOX_LABELS, SPECTRUM_RES + 5, tft.getFontHeight(), RA8875_BLACK);

  freq_calc = (float)(centerFreq / NEW_SI5351_FREQ_MULT);  // get current frequency in Hz

  if (activeVFO == VFO_A) {
    currentFreqA = TxRxFreq;
  } else {
    currentFreqB = TxRxFreq;
  }

  if (spectrum_zoom == 0) {
    freq_calc += (float32_t)SampleRate / 4.0;
  }

  if (spectrum_zoom < 5) {
    freq_calc = roundf(freq_calc / 1000);  // round graticule frequency to the nearest kHz
  } else if (spectrum_zoom < 5) {
    freq_calc = roundf(freq_calc / 100) / 10;  // round graticule frequency to the nearest 100Hz
  }

  if (spectrum_zoom != 0)
    centerIdx = 0;
  else
    centerIdx = -2;

  /**************************************************************************************************
    CENTER FREQUENCY PRINT
  **************************************************************************************************/
  ultoa((freq_calc + (centerIdx * grat)), txt, DEC);
  disp_freq = freq_calc + (centerIdx * grat);
  bignum = (int)disp_freq;
  itoa(bignum, txt, DEC);  // Make into a string

  tft.setTextColor(RA8875_GREEN);

  if (spectrum_zoom == 0) {
    tft.setCursor(centerLine - 140, SPEC_BOX_LABELS);
  } else {
    tft.setCursor(centerLine - 20, SPEC_BOX_LABELS);
  }

  tft.print(txt);
  tft.setTextColor(RA8875_WHITE);

  /**************************************************************************************************
     PRINT ALL OTHER FREQUENCIES (NON-CENTER)
   **************************************************************************************************/
  // snprint() extremely memory inefficient. replaced with simple str?? functions JJP
  for (int idx = -4; idx < 5; idx++) {
    pos_help = idx2pos[spectrum_zoom < 3 ? 0 : 1][idx + 4];
    if (idx != centerIdx) {
      ultoa((freq_calc + (idx * grat)), txt, DEC);

      if (idx < 4) {
        tft.setCursor(SPECTRUM_LEFT_X + pos_help * xExpand + 40, SPEC_BOX_LABELS);
      } else {
        tft.setCursor(SPECTRUM_LEFT_X + (pos_help + 9) * xExpand + 59 - strlen(txt)*tft.getFontWidth(), SPEC_BOX_LABELS);
      }

      tft.print(txt);
      if (idx < 4) {
        tft.drawFastVLine((SPECTRUM_LEFT_X + pos_help * xExpand + 60), SPEC_BOX_LABELS - 5, 7, RA8875_YELLOW);  // Tick marks depending on zoom
      } else {
        tft.drawFastVLine((SPECTRUM_LEFT_X + (pos_help + 9) * xExpand + 59), SPEC_BOX_LABELS - 5, 7, RA8875_YELLOW);
      }
    }

    // *** ??? display is messed up for frequencies under 1000 ***
    if (spectrum_zoom > 2 || freq_calc > 1000) {
      idx++;
    }
  }
}

/*****
  Purpose: void ShowAnalogGain()

  Parameter list:
    void

  Return value;
    void
    // This function draws the frequency bar at the bottom of the spectrum scope, putting markers at every graticule and the full frequency
*****/
void ShowAnalogGain() {
  static uint8_t RF_gain_old = 0;
  static uint8_t RF_att_old = 0;
  const uint16_t col = RA8875_GREEN;
  if ((((bands[currentBand].RFgain != RF_gain_old) || (attenuator != RF_att_old)) && twinpeaks_tested == 1) || write_analog_gain) {
    tft.setFontScale((enum RA8875tsize)0);
    tft.setCursor(pos_x_time - 40, pos_y_time + 26);
    tft.print((float)(RF_gain_old * 1.5));
    tft.setTextColor(col);
    tft.print("dB -");

    tft.setTextColor(RA8875_BLACK);
    tft.print("dB -");
    tft.setTextColor(RA8875_BLACK);
    tft.print("dB");
    tft.setTextColor(col);
    tft.print("dB = ");

    tft.setFontScale((enum RA8875tsize)0);

    tft.setTextColor(RA8875_BLACK);
    tft.print("dB");
    tft.setTextColor(RA8875_WHITE);
    tft.print("dB");
    RF_gain_old = bands[currentBand].RFgain;
    RF_att_old = attenuator;
    write_analog_gain = 0;
  }
}

/*****
  Purpose: To display the current transmission frequency, band, mode, and sideband above the spectrum display

  Parameter list:
    void

  Return value;
    void

*****/
void ShowOperatingStats() {
  tft.setFontScale((enum RA8875tsize)0);

  // clear operating stats
  tft.fillRect(OPERATION_STATS_L, OPERATION_STATS_T, OPERATION_STATS_W, tft.getFontHeight(), RA8875_BLACK);

  // print center frequency
  tft.setCursor(OPERATION_STATS_L, OPERATION_STATS_T);
  tft.setTextColor(RA8875_WHITE);
  tft.print("Center Freq");
  tft.setCursor(OPERATION_STATS_CF, OPERATION_STATS_T);
  tft.setTextColor(RA8875_LIGHT_ORANGE);
  if (spectrum_zoom == 0) {
    tft.print(centerFreq + 48000);
  } else {
    tft.print(centerFreq);
  }

  // print band for the active VFO
  tft.setTextColor(RA8875_LIGHT_ORANGE);
  tft.setCursor(OPERATION_STATS_BD, OPERATION_STATS_T);
  if (activeVFO == VFO_A) {
    tft.print(bands[currentBandA].name);  // Show band -- 40M
  } else {
    tft.print(bands[currentBandB].name);  // Show band -- 40M
  }

  tft.setTextColor(RA8875_GREEN);
  tft.setCursor(OPERATION_STATS_MD, OPERATION_STATS_T);

  switch(xmtMode) {
    case CW_MODE:
      tft.print("CW ");
      tft.setCursor(OPERATION_STATS_CWF, OPERATION_STATS_T);
      tft.print(CWFilter[CWFilterIndex]);
      break;

    case SSB_MODE:
      tft.print("SSB");
      break;

#ifdef FT8
    case DATA_MODE:
      tft.print("DATA");
      break;
#endif
  }

  tft.setCursor(OPERATION_STATS_DMD, OPERATION_STATS_T);
  tft.setTextColor(RA8875_WHITE);

  switch (bands[currentBand].mode) {
    case DEMOD_USB:
    case DEMOD_LSB:
    case DEMOD_FT8: // ft8 is USB
    case DEMOD_FT8_WAV: // ft8 is USB
      if (activeVFO == VFO_A) {
        tft.print(DEMOD[bands[currentBandA].mode].text);
      } else {
        tft.print(DEMOD[bands[currentBandB].mode].text);
      }
      break;

    case DEMOD_AM:
      tft.print("(AM)");
      break;

    case DEMOD_NFM:
      tft.print("(NFM)");
      break;

    case DEMOD_SAM:
      tft.print("(SAM) ");
      break;
  }

  ShowCurrentPowerSetting();
}

/*****
  Purpose: Display current power setting

  Parameter list:
    void

  Return value;
    void
*****/
void ShowCurrentPowerSetting() {
  tft.setFontScale((enum RA8875tsize)0);
  tft.fillRect(OPERATION_STATS_PWR, OPERATION_STATS_T, tft.getFontWidth() * 11, tft.getFontHeight(), RA8875_BLACK);
  tft.setCursor(OPERATION_STATS_PWR, OPERATION_STATS_T);
  tft.setTextColor(RA8875_RED);
  tft.print(transmitPowerLevel, 1);  // Power output is a float
  tft.print(" Watts");
}

/*****
  Purpose: Update CW Filter

  Parameter list:
    void

  Return value;
    void
*****/
void UpdateCWFilter() {
  float CWFilterPosition = 85.0; // max filter position

  tft.writeTo(L2);
  if(xmtMode == CW_MODE) {
    switch (CWFilterIndex) {
      case 0:
        CWFilterPosition = 35.7;  // 0.84 * 42.5;
        break;
      case 1:
        CWFilterPosition = 42.5;
        break;
      case 2:
        CWFilterPosition = 55.25;  // 1.3 * 42.5;
        break;
      case 3:
        CWFilterPosition = 76.5;  // 1.8 * 42.5;
        break;
      case 4:
        CWFilterPosition = 85.0;  // 2.0 * 42.5;
        break;
      case 5:
        CWFilterPosition = 0.0;
        break;
    }

    tft.fillRect(AUDIO_SPEC_BOX_L + 2, AUDIO_SPEC_BOX_T, CWFilterPosition, 120, MAROON);
    // this bounding line is confusing given the filter lines already in the audio spectrum box
    //tft.drawFastVLine(AUDIO_SPEC_BOX_L + 2 + CWFilterPosition, AUDIO_SPEC_BOX_T, AUDIO_SPEC_BOX_H, RA8875_LIGHT_GREY);
  } else {
    // clear CW filter
    tft.fillRect(AUDIO_SPEC_BOX_L + 2, AUDIO_SPEC_BOX_T, CWFilterPosition, 120, RA8875_BLACK);
  }

  tft.writeTo(L1);
}

/*****
  Purpose: Show main frequency display at top

  Parameter list:
    void

  Return value;
    void
*****/
FASTRUN void ShowFrequency() {
  char freqBuffer[15];

  // *** do this in the proper place if this is needed ***
  //if (activeVFO == VFO_A) {  // Needed for edge checking
  //  currentBand = currentBandA;
  //} else {
  //  currentBand = currentBandB;
  //}

  FormatFrequency(TxRxFreq, freqBuffer);
  tft.setFontScale(3, 2);
//  tft.fillRect(0, FREQUENCY_Y - 17, SPEC_BOX_W, tft.getFontHeight(), RA8875_BLACK);
  tft.fillRect(0, FREQUENCY_Y - 17, TIME_X - 20, tft.getFontHeight(), RA8875_BLACK);

  if (activeVFO == VFO_A) {
    if (TxRxFreq < bands[currentBandA].fBandLow || TxRxFreq > bands[currentBandA].fBandHigh) {
      tft.setTextColor(RA8875_RED);  // Out of band
    } else {
      tft.setTextColor(RA8875_GREEN); // In US band
    }
    tft.setCursor(0, FREQUENCY_Y - 17);
    tft.print(freqBuffer); // Show VFO_A

    tft.setFontScale(1, 2);
    tft.setTextColor(RA8875_LIGHT_GREY);
    tft.setCursor(FREQUENCY_X_SPLIT + 60, FREQUENCY_Y - 17);
    FormatFrequency(currentFreqB, freqBuffer);
  } else { // VFO_B
    if (TxRxFreq < bands[currentBandB].fBandLow || TxRxFreq > bands[currentBandB].fBandHigh) {
      tft.setTextColor(RA8875_RED);
    } else {
      tft.setTextColor(RA8875_GREEN);
    }
    tft.setCursor(FREQUENCY_X_SPLIT - 60, FREQUENCY_Y - 17);
    tft.print(freqBuffer); // Show VFO_B

    tft.setFontScale(1, 2);
    tft.setTextColor(RA8875_LIGHT_GREY);
    tft.setCursor(20, FREQUENCY_Y - 17);
    FormatFrequency(currentFreqA, freqBuffer);
  }

  tft.print(freqBuffer); // Show the other one
}

//DB2OO, this variable determines the pixels per S step. In the original code it was 12.2 pixels !?
#ifdef TCVSDR_SMETER
const float pixels_per_s = 12;
#else
const float pixels_per_s = 12.2;
#endif
/*****
  Purpose: Display dBm

  Parameter list:
    void

  Return value;
    void
*****/
void DrawSmeterBar() {
  char buff[10];
  const char *unit_label;
  int16_t smeterPad;
  float32_t dbm;
  float32_t dbm_calibration = 22.0;
#ifdef TCVSDR_SMETER
  const float32_t slope = 10.0;
  const float32_t cons = -92;
#else
  float32_t audioLogAveSq;
#endif

  //DB2OO, 30-AUG-23: the S-Meter bar and the dBm value were inconsistent, as they were using different base values.
  // Moreover the bar could go over the limits of the S-meter box, as the map() function, does not constrain the values
  // with TCVSDR_SMETER defined the S-Meter bar will be consistent with the dBm value and the S-Meter bar will always be restricted to the box
  tft.fillRect(SMETER_X + 1, SMETER_Y + 1, SMETER_BAR_LENGTH, SMETER_BAR_HEIGHT, RA8875_BLACK); // Erase old bar
#ifdef TCVSDR_SMETER
  //DB2OO, 9-OCT_23: dbm_calibration set to -22 above; gainCorrection is a value between -2 and +6 to compensate the frequency dependant pre-Amp gain
  // attenuator is 0 and could be set in a future HW revision; RFgain is initialized to 1 in the bands[] init in SDT.ino; cons=-92; slope=10
  dbm = dbm_calibration + bands[currentBand].gainCorrection + (float32_t)attenuator + slope * log10f_fast(audioMaxSquaredAve) + 
        cons - (float32_t)bands[currentBand].RFgain * 1.5 - rfGainAllBands; //DB2OO, 08-OCT-23; added rfGainAllBands
#else
  //DB2OO, 9-OCT-23: audioMaxSquaredAve is proportional to the input power. With rfGainAllBands=0 it is approx. 40 for -73dBm @ 14074kHz with the V010 boards and the pre-Amp fed by 12V
  // for audioMaxSquaredAve=40 audioLogAveSq will be 26
  audioLogAveSq = 10 * log10f_fast(audioMaxSquaredAve) + 10;
  //DB2OO, 9-OCT-23: calculate dBm value from audioLogAveSq and ignore band gain differences and a potential attenuator like in original code
  dbm = audioLogAveSq - 100;

//DB2OO, 9-OCT-23: this is the orginal code, that will map a 30dB difference (35-5) to 60 (635-575) pixels, i.e. 5 S steps to 5*12 pixels
// SMETER_X is 528 --> X=635 would be 107 pixels / 12pixels per S step --> approx. S9
//  smeterPad = map(audioLogAveSq, 5, 35, 575, 635);
//  tft.fillRect(SMETER_X + 1, SMETER_Y + 1, smeterPad - SMETER_X, SMETER_BAR_HEIGHT, RA8875_RED);
#endif
  // determine length of S-meter bar, limit it to the box and draw it
  smeterPad = map(dbm, -73.0-9*6.0 /*S1*/, -73.0 /*S9*/, 0, 9*pixels_per_s);
  //DB2OO; make sure, that it does not extend beyond the field
  smeterPad = max(0, smeterPad);
  smeterPad = min(SMETER_BAR_LENGTH, smeterPad);
  tft.fillRect(SMETER_X + 1, SMETER_Y + 2, smeterPad, SMETER_BAR_HEIGHT-2, RA8875_RED); //DB2OO: bar 2*1 pixel smaller than the field

  tft.setTextColor(RA8875_WHITE);

  //DB2OO, 17-AUG-23: create PWM analog output signal on the "HW_SMETER" output. This is scaled for a 250uA  S-meter full scale, 
  // connected to HW_SMTER output via a 8.2kOhm resistor and a 4.7kOhm resistor and 10uF capacitor parallel to the S-Meter
#ifdef HW_SMETER
  { int hw_s;
    hw_s = map((int)dbm-3, -73-(8*6), -73+60, 0, 228);
    hw_s = max(0, min(hw_s, 255));
    analogWrite(HW_SMETER, hw_s);
  }
#endif

  unit_label = "dBm";
  tft.setFontScale((enum RA8875tsize)0);

  tft.fillRect(SMETER_X + 185, SMETER_Y, 80, tft.getFontHeight(), RA8875_BLACK);  // The dB figure at end of S 
  //DB2OO, 29-AUG-23: consider no decimals in the S-meter dBm value as it is very busy with decimals
  MyDrawFloat(dbm, /*0*/ 1, SMETER_X + 184, SMETER_Y, buff);
  tft.setTextColor(RA8875_GREEN);
  tft.print(unit_label);
}

/*****
  Purpose: format a floating point number

  Parameter list:
    float val         the value to format
    int decimals      the number of decimal places
    int x             the x coordinate for display
    int y                 y          "

  Return value;
    void
*****/
void MyDrawFloat(float val, int decimals, int x, int y, char *buff) {
  MyDrawFloatP(val, decimals, x, y, buff, FLOAT_PRECISION);
}

void MyDrawFloatP(float val, int decimals, int x, int y, char *buff, int width) {
  dtostrf(val, width, decimals, buff);  // Use 8 as that is the max prevision on a float

  tft.fillRect(x, y, width * tft.getFontWidth(), 15, RA8875_BLACK);
  tft.setCursor(x, y);

  tft.print(buff);
}

/*****
  Purpose: This function redraws the entire display screen where the equalizers appeared
  *** this function does much more than redraw the display and some of it is repetitive ***
  Parameter list:
    void

  Return value;
    void
*****/
void RedrawDisplayScreen() {
  // clear display
  tft.fillWindow();
  
  //AGCPrep();
  //SetBand();

  DrawStaticDisplayItems();

  // update display left to right, top to bottom
  // draw top of display
  ShowFrequency();
  ShowName();
  ShowOperatingStats();

  // draw spectrum area
  ShowSpectrumdBScale();
  ShowBandwidthBarValues();
  DrawBandwidthBar();
  ShowSpectrumFreqValues();

  ShowTransmitReceiveStatus();

  //SpectralNoiseReductionInit();

  UpdateInfoBox();
}

/*****
  Purpose: Draw Tuned Bandwidth on Spectrum Plot

  Parameter list:
    void

  Return value;
    void
*****/
FASTRUN void DrawBandwidthBar() {
  float zoomMultFactor = 0.0;
  float Zoom1Offset = 0.0;
  float32_t pixel_per_khz;
  int filterWidth;

  switch (spectrum_zoom) {
    case 0:
      zoomMultFactor = 0.5;
      Zoom1Offset = 24000 * 0.0053333;
      break;

    case 1:
      zoomMultFactor = 1.0;
      Zoom1Offset = 0;
      break;

    case 2:
      zoomMultFactor = 2.0;
      Zoom1Offset = 0;
      break;

    case 3:
      zoomMultFactor = 4.0;
      Zoom1Offset = 0;
      break;

    case 4:
      zoomMultFactor = 8.0;
      Zoom1Offset = 0;
      break;
  }
  newCursorPosition = (int)(NCOFreq * 0.0053333) * zoomMultFactor - Zoom1Offset;

  pixel_per_khz = ((1 << spectrum_zoom) * SPECTRUM_RES * 1000.0 / SampleRate);
  filterWidth = (int)(((bands[currentBand].FHiCut - bands[currentBand].FLoCut) / 1000.0) * pixel_per_khz * 1.06);

  // make sure bandwidth is within zoom range
  switch (bands[currentBand].mode) {
    case DEMOD_USB:
    case DEMOD_FT8: // ft8 is USB
    case DEMOD_FT8_WAV: // ft8 is USB
      if(centerLine + newCursorPosition + filterWidth > SPECTRUM_RES) {
        resetTuningFlag = true;
      }
      break;

    case DEMOD_LSB:
      if(centerLine + newCursorPosition - filterWidth < 0) {
        resetTuningFlag = true;
      }
      break;

    case DEMOD_NFM:
      filterWidth = (int)((nfmFilterBW / 1000.0) * pixel_per_khz * 1.06);

    case DEMOD_AM:
    case DEMOD_SAM:
      if((centerLine - (filterWidth / 2) * 0.93 + newCursorPosition < 0) || (centerLine + (filterWidth / 2) * 0.93 + newCursorPosition > SPECTRUM_RES)) {
        resetTuningFlag = true;
      }
      break;
  }

  // erase old bar
  tft.writeTo(L2);
  tft.fillRect(SPECTRUM_LEFT_X, SPECTRUM_TOP_Y + 20, SPECTRUM_RES, SPECTRUM_HEIGHT - 20, RA8875_BLACK);

  // update bar if we haven't reset tuning, otherwise this gets recalled by that routine
  if(!resetTuningFlag) {
    switch (bands[currentBand].mode) {
      case DEMOD_USB:
      case DEMOD_FT8: // ft8 is USB
      case DEMOD_FT8_WAV: // ft8 is USB
        tft.fillRect(centerLine + newCursorPosition, SPECTRUM_TOP_Y + 20, filterWidth, SPECTRUM_HEIGHT - 20, FILTER_WIN);
        break;

      case DEMOD_LSB:
        tft.fillRect(centerLine - filterWidth + newCursorPosition, SPECTRUM_TOP_Y + 20, filterWidth, SPECTRUM_HEIGHT - 20, FILTER_WIN);
        break;

      case DEMOD_NFM:
        filterWidth = (int)((nfmFilterBW / 1000.0) * pixel_per_khz * 1.06);
        tft.fillRect(centerLine - (filterWidth / 2) * 0.93 + newCursorPosition, SPECTRUM_TOP_Y + 20, filterWidth * 0.95, SPECTRUM_HEIGHT - 20, FILTER_WIN);
        break;

      case DEMOD_AM:
      case DEMOD_SAM:
        tft.fillRect(centerLine - (filterWidth / 2) * 0.93 + newCursorPosition, SPECTRUM_TOP_Y + 20, filterWidth * 0.95, SPECTRUM_HEIGHT - 20, FILTER_WIN);
        break;
    }

    tft.drawFastVLine(centerLine + newCursorPosition, SPECTRUM_TOP_Y + 20, SPECTRUM_HEIGHT - 20, RA8875_CYAN);

    oldCursorPosition = newCursorPosition;
  }

  tft.writeTo(L1);
}

/*****
  Purpose: This function draws spectrum display container

  Parameter list:
    void

  Return value;
    void
*****/
void DrawSpectrumFrame() {
  tft.drawRect(SPEC_BOX_L, SPEC_BOX_T, SPEC_BOX_W, SPEC_BOX_H, RA8875_YELLOW);
}

/*****
  Purpose: This function removes the spectrum display container

  Parameter list:
    void

  Return value;
    void
*****/
void EraseSpectrumDisplayContainer() {
  tft.fillRect(SPECTRUM_LEFT_X - 2, SPECTRUM_TOP_Y - 1, SPECTRUM_RES + 6, SPECTRUM_HEIGHT + 8, RA8875_BLACK);  // Spectrum box
}

/*****
  Purpose: This function erases the contents of the spectrum display

  Parameter list:
    void

  Return value;
    void
*****/
void EraseSpectrumWindow() {
  newSpectrumFlag = 0; // old noise floor needs reset
  tft.fillRect(SPECTRUM_LEFT_X, SPECTRUM_TOP_Y, SPECTRUM_RES, SPECTRUM_HEIGHT, RA8875_BLACK);  // Spectrum box
}

/*****
  Purpose: Draw S-Meter container

  Parameter list:
    void

  Return value;
    void
*****/
void DrawSMeterContainer() {
  int i;
  // DB2OO, 30-AUG-23: the white line must only go till S9
  tft.drawFastHLine(SMETER_X, SMETER_Y - 1, 9 * pixels_per_s, RA8875_WHITE);
  tft.drawFastHLine(SMETER_X, SMETER_Y + SMETER_BAR_HEIGHT+2, 9 * pixels_per_s, RA8875_WHITE);  // changed 6 to 20

  for (i = 0; i < 10; i++) {                                                // Draw tick marks for S-values
#ifdef TCVSDR_SMETER
    //DB2OO, 30-AUG-23: draw wider tick marks in the style of the Teensy Convolution SDR
    tft.drawRect(SMETER_X + i * pixels_per_s, SMETER_Y - 6-(i%2)*2, 2, 6+(i%2)*2, RA8875_WHITE);
#else      
    tft.drawFastVLine(SMETER_X + i * 12.2, SMETER_Y - 6, 7, RA8875_WHITE);
#endif    
  }

  // DB2OO, 30-AUG-23: the green line must start at S9
  tft.drawFastHLine(SMETER_X + 9*pixels_per_s, SMETER_Y - 1, SMETER_BAR_LENGTH+2-9*pixels_per_s, RA8875_GREEN);
  tft.drawFastHLine(SMETER_X + 9*pixels_per_s, SMETER_Y + SMETER_BAR_HEIGHT+2, SMETER_BAR_LENGTH+2-9*pixels_per_s, RA8875_GREEN);

  for (i = 1; i <= 3; i++) {                                                     // Draw tick marks for s9+ values in 10dB steps
#ifdef TCVSDR_SMETER
    //DB2OO, 30-AUG-23: draw wider tick marks in the style of the Teensy Convolution SDR
    tft.drawRect(SMETER_X + 9*pixels_per_s + i * pixels_per_s*10.0/6.0, SMETER_Y - 8+(i%2)*2, 2, 8-(i%2)*2, RA8875_GREEN);
#else      
    tft.drawFastVLine(SMETER_X + 9*pixels_per_s + i * pixels_per_s*10.0/6.0, SMETER_Y - 6, 7, RA8875_GREEN);  
#endif
  }

  tft.drawFastVLine(SMETER_X, SMETER_Y - 1, SMETER_BAR_HEIGHT+3, RA8875_WHITE);  
  tft.drawFastVLine(SMETER_X + SMETER_BAR_LENGTH+2, SMETER_Y - 1, SMETER_BAR_HEIGHT+3, RA8875_GREEN);

  tft.setFontScale((enum RA8875tsize)0);

  tft.setTextColor(RA8875_WHITE);
  //DB2OO, 30-AUG-23: moved single digits a bit to the right, to align 
  tft.setCursor(SMETER_X - 8, SMETER_Y - 25);
  tft.print("S");
  tft.setCursor(SMETER_X + 8, SMETER_Y - 25);
  tft.print("1");
  tft.setCursor(SMETER_X + 32, SMETER_Y - 25);  // was 28, 48, 68, 88, 120 and -15 changed to -20
  tft.print("3");
  tft.setCursor(SMETER_X + 56, SMETER_Y - 25);
  tft.print("5");
  tft.setCursor(SMETER_X + 80, SMETER_Y - 25);
  tft.print("7");
  tft.setCursor(SMETER_X + 104, SMETER_Y - 25);
  tft.print("9");
  //DB2OO, 30-AUG-23 +20dB needs to get more left
  tft.setCursor(SMETER_X + 133, SMETER_Y - 25);
  tft.print("+20dB");

  //ShowSpectrumFreqValues();
  //ShowSpectrumdBScale();
}

/*****
  Purpose: Draw audio spectrum box

  Parameter list:

  Return value;
    void
*****/
// old factor 43.8
void DrawAudioSpectContainer() {
  tft.drawRect(AUDIO_SPEC_BOX_L, AUDIO_SPEC_BOX_T, AUDIO_SPEC_BOX_W, AUDIO_SPEC_BOX_H, RA8875_WHITE);
  for (int k = 0; k < 6; k++) {
    tft.drawFastVLine(AUDIO_SPEC_BOX_L + k * 43, AUDIO_SPEC_BOTTOM, 15, RA8875_WHITE);
    tft.setCursor(AUDIO_SPEC_BOX_L - 4 + k * 43, AUDIO_SPEC_BOTTOM + 16);
    tft.print(k);
    tft.print("k");
  }
  tft.drawFastVLine(AUDIO_SPEC_BOX_L + 6 * 43, AUDIO_SPEC_BOTTOM, 15, RA8875_WHITE);
  tft.setCursor(AUDIO_SPEC_BOX_L + 6 * 43 - tft.getFontWidth(), AUDIO_SPEC_BOTTOM + 16);
  tft.print(6);
  tft.print("k");
}

/*****
  Purpose: To erase both primary and secondary menus from display

  Parameter list:

  Return value;
    void
*****/
void EraseMenus() {
  tft.fillRect(PRIMARY_MENU_X, MENUS_Y, BOTH_MENU_WIDTHS, CHAR_HEIGHT + 1, RA8875_BLACK);  // Erase menu choices
//  menuStatus = NO_MENUS_ACTIVE;                                                            // Change menu state
}

/*****
  Purpose: To erase primary menu from display

  Parameter list:

  Return value;
    void
*****/
void ErasePrimaryMenu() {
  tft.fillRect(PRIMARY_MENU_X, MENUS_Y, EACH_MENU_WIDTH, CHAR_HEIGHT + 1, RA8875_BLACK);  // Erase menu choices
//  menuStatus = NO_MENUS_ACTIVE;                                                           // Change menu state
}

/*****
  Purpose: To erase secondary menu from display

  Parameter list:

  Return value;
    void
*****/
void EraseSecondaryMenu() {
  tft.fillRect(SECONDARY_MENU_X, MENUS_Y, EACH_MENU_WIDTH, CHAR_HEIGHT + 1, RA8875_BLACK);  // Erase menu choices
//  menuStatus = NO_MENUS_ACTIVE;                                                             // Change menu state
}

/*****
  Purpose: Shows transmit (red) and receive (green) mode

  Parameter list:

  Return value;
    void
*****/
void ShowTransmitReceiveStatus() {
  tft.setFontScale((enum RA8875tsize)1);
  tft.setTextColor(RA8875_BLACK);
  if (xrState == TRANSMIT_STATE) {
    tft.fillRect(X_R_STATUS_X, X_R_STATUS_Y, 55, 25, RA8875_RED);
    tft.setCursor(X_R_STATUS_X + 4, X_R_STATUS_Y - 5);
    tft.print("XMT");
  } else {
    tft.fillRect(X_R_STATUS_X, X_R_STATUS_Y, 55, 25, RA8875_GREEN);
    tft.setCursor(X_R_STATUS_X + 4, X_R_STATUS_Y - 5);
    tft.print("REC");
  }
}

/*****
  Purpose: Set frequency display to next spectrum_zoom level

  Parameter list:
    void

  Return value;
    void

*****/
void SetZoom() {
  ZoomFFTPrep();
  UpdateInfoBoxItem(&infoBox[IB_ITEM_ZOOM]);
  DrawBandwidthBar();
  ShowSpectrumFreqValues();
}

/*****
  Purpose: Draw static items on display

  Parameter list:
    void

  Return value;
    void

*****/
void DrawStaticDisplayItems() {
  ShowName();
  DrawSpectrumFrame();
  DrawSMeterContainer();
  DrawAudioSpectContainer();
  DrawInfoBoxFrame();
}
