#include <Audio.h>
#include <Metro.h>

#include "SDT.h"

#include "Bearing.h"
#include "Button.h"
#include "ButtonProc.h"
#include "CW_Excite.h"
#include "CWProcessing.h"
#include "Display.h"
#include "Encoders.h"
//#include "EEPROM.h"
#include "Exciter.h"
#include "FFT.h"
#include "Filter.h"
#include "ft8.h"
#include "InfoBox.h"
#include "Menu.h"
#include "MenuProc.h"
#include "mouse.h"
#include "Noise.h"
#include "Process.h"
#include "Tune.h"
#include "t41Control.h"
//#include "t41USBHost.h"
#include "Utility.h"

#include "keyboard.h"

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

  The original T41 software writes to the display automatically during operation.  This prevents having
  alternate displays, like a beacon monitor, while the radio is operating.  The following items are the issue
  during normal operation (other items, like the menus, might also have to be considered):
    VFO frequencies, op stats, freq spectrum, bandwidth bar and values, spectrum values, waterfall,
    clock, xmit indicator, s-meter, audio spectrum, filter lines, infobox items
  The wrinkle is that T41 radio operation is driven by a key element of the display update process,
  ShowSpectrum() and the timing of the operating loop is determined in part based on updates to
  the display happening in ShowSpectrum().  This function is only called in two places in the main
  operating loop so one method to operate without the normal display is to create a separete ShowXXX() function
  to drive radio operations for each display, keeping the needed elements of ShowSpectrum() and discarding those
  not needed.  Doing this we find that the display is updated for other actions, changing bands for example.
  Here's a list of additional functions for the beacon monitor (ignoring for now changes caused by user interaction
  with the T41 buttons or encoders): SetBand, SetTxRxFreq, ChangeDemodMode, ChangeMode, DrawSmeterBar (tricky as we
  need the dBm calc) and UpdateInfoBoxItem (could be breaking for any followup items that do more than update the
  display, mouse routines perhaps?). And of course layer 2 needs cleared.

  We can clear these as follows:
  For all of these items, I've added switch statements to allow for alernate screens while the radio
  is operating. The switch statement with on the "displayState" variable.

  With this, the transmit indicator is the only remaining normal operating element on the display.  I've left that for now.
*/
//-------------------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

#define NEW_SI5351_FREQ_MULT  1UL
#define FLOAT_PRECISION         6             // Assumed precision for a float
#define RIGNAME_X_OFFSET      570             // Pixel count to rig name field

//------------------------- Global Variables ----------

int displayState = DISPLAY_T41;

int centerLine = SPECTRUM_RES / 2 + SPECTRUM_LEFT_X;

int16_t pixelnew[SPECTRUM_RES];
int nf2PC;

int wfRows = WATERFALL_H;

#ifdef RA8875_DISPLAY
#define RA8875_CS TFT_CS
#define RA8875_RESET TFT_DC  // any pin or nothing!
#ifdef PROJECTSYSTEM
RA8875 tft = RA8875(RA8875_CS, RA8875_RESET, TFT_MOSI, TFT_SCLK, TFT_MISO);
#else
RA8875 tft = RA8875(RA8875_CS, RA8875_RESET);
#endif
#endif
#ifdef ILI9488_DISPLAY
ILI9488_t3 tft = ILI9488_t3(&SPI, TFT_CS, TFT_DC, TFT_RST);
#endif
#ifdef NO_DISPLAY
RA8875 tft = RA8875();
#endif

dispSc displayScale[] =
{
  // *dbText,dBScale, pixelsPerDB, baseOffset, offsetIncrement
  { "20 dB/", 10.0,  2,  24, 1.00 },
  { "10 dB/", 20.0,  4,  10, 0.50 },
  { "5 dB/",  40.0,  8,  58, 0.25 },
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

/* PROGMEM */ const uint16_t gradient[] = {  // Color array for waterfall background
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

int maxYPlot;
int filterWidthX;  // The current filter X.

struct DEMOD_Descriptor
{ const uint8_t DEMOD_n;
  const char* const text;
};
const DEMOD_Descriptor DEMOD[8] = {
  //   DEMOD_n, name
  { DEMOD_USB, "USB" },
  { DEMOD_LSB, "LSB" },
  { DEMOD_AM, "AM" },
  { DEMOD_NFM, "NFM" },
  { DEMOD_PSK31_WAV, "PSK31.wav" },
  { DEMOD_PSK31, "PSK31" },
  { DEMOD_FT8_WAV, "FT8.wav" },
  { DEMOD_FT8, "FT8" },
};

//-------------------------------------------------------------------------------------------------------------
// Forwards
//-------------------------------------------------------------------------------------------------------------

void DrawSMeterContainer();
void DrawAudioSpectContainer();

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

void InitDisplay() {
  // set up display
  pinMode(TFT_MOSI, OUTPUT);
  digitalWrite(TFT_MOSI, HIGH);
  pinMode(TFT_SCLK, OUTPUT);
  digitalWrite(TFT_SCLK, HIGH);
  pinMode(TFT_CS, OUTPUT);
  digitalWrite(TFT_CS, HIGH);

  // *** is this setting the spi speed??? ***
  uint32_t iospeed_display = IOMUXC_PAD_DSE(3) | IOMUXC_PAD_SPEED(1);
  *(digital_pin_to_info_PGM + TFT_SCLK)->pad = iospeed_display;
  *(digital_pin_to_info_PGM + TFT_MOSI)->pad = iospeed_display;
  *(digital_pin_to_info_PGM + TFT_CS)->pad = iospeed_display;

  tft.begin(RA8875_800x480, 8, 20000000UL, 4000000UL);  // parameter list from library code
#ifdef FOURSQRP
  tft.setRotation(0);
#endif
#ifdef PROJECTSYSTEM
  tft.setRotation(2);
#endif

  // Setup for scrolling attributes. Part of initSpectrum_RA8875() call written by Mike Lewis
  tft.useLayers(true); // mainly used to turn on layers
  tft.layerEffect(OR); // overlay layers
  tft.writeTo(L2);
  tft.clearMemory();
  tft.writeTo(L1);
  tft.clearMemory();
}

/*****
  Purpose: Show the program name and version number
*****/
FLASHMEM void ShowName() {
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

int filterLoPosition;
int filterHiPosition;

void CalcAudioFilterLinePositions() {
  // map filter position to audio spectrum box
  filterLoPosition = map(currentFilterLoCut, 0, AUDIO_SPEC_SPAN, 0, AUDIO_SPEC_RES);
  filterHiPosition = map(currentFilterHiCut, 0, AUDIO_SPEC_SPAN, 0, AUDIO_SPEC_RES);
}

// *** pulling this out of ShowSpectrum allows the screen to update about 35% faster
//     Waterfall Time before update 54s, after 35s ***
void DrawAudioFilterLines() {
  int filterLoColor, filterHiColor;
  static int oldFilterLoPosition;
  static int oldFilterHiPosition;

  CalcAudioFilterLinePositions();

  // draw fiter indicator lines on the audio spectrum

  // set color of active filter bar to green
  switch(bands[currentBand].demod) {
    case DEMOD_USB:
    case DEMOD_LSB:
    case DEMOD_PSK31_WAV:
    case DEMOD_PSK31:
    case DEMOD_FT8_WAV:
    case DEMOD_FT8:
      if(lowerAudioFilterActive) {
        filterLoColor = RA8875_GREEN;
        filterHiColor = RA8875_LIGHT_GREY;
      } else {
        if(ft8MsgSelectActive) {
          filterLoColor = RA8875_LIGHT_GREY;
          filterHiColor = RA8875_LIGHT_GREY;
        } else {
          filterLoColor = RA8875_LIGHT_GREY;
          filterHiColor = RA8875_GREEN;
        }
      }
      break;

    case DEMOD_NFM:
      if(nfmBWFilterActive) {
        filterLoColor = RA8875_LIGHT_GREY;
        filterHiColor = RA8875_LIGHT_GREY;
      } else {
        if(lowerAudioFilterActive) {
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

  // erase old and draw new filter lines
  // limit the filter line from going out of the spectrum box to the right
  if(oldFilterLoPosition > 0 && oldFilterLoPosition < AUDIO_SPEC_W) {
    tft.drawFastVLine(AUDIO_SPEC_L + oldFilterLoPosition, AUDIO_SPEC_T, AUDIO_SPEC_H, RA8875_BLACK);
  }
  if(oldFilterHiPosition > 0 && oldFilterHiPosition < AUDIO_SPEC_W) {
    tft.drawFastVLine(AUDIO_SPEC_L + oldFilterHiPosition, AUDIO_SPEC_T, AUDIO_SPEC_H, RA8875_BLACK);
  }
  if(filterLoPosition > 0 && filterLoPosition < AUDIO_SPEC_W) {
    tft.drawFastVLine(AUDIO_SPEC_L + filterLoPosition, AUDIO_SPEC_T, AUDIO_SPEC_H, filterLoColor);
  }
  if(filterHiPosition > 0 && filterHiPosition < AUDIO_SPEC_W) {
    tft.drawFastVLine(AUDIO_SPEC_L + filterHiPosition, AUDIO_SPEC_T, AUDIO_SPEC_H, filterHiColor);
  }

  oldFilterLoPosition = filterLoPosition;
  oldFilterHiPosition = filterHiPosition;
}

/*****
  Purpose: Update spectrum and waterfall on T41 display
            This is is a long running process.  It yields periodically to allow normal
            radio operations to continue.
*****/
FASTRUN void ShowSpectrum() {
  int yPlot, y1Plot;
  int hLo = 0, hHi = 0;
  int wfGradIndex;
  static uint16_t yOldPlot[SPECTRUM_RES];
  static int yOldAudioPlot[AUDIO_SPEC_RES];
  static int currentNF = 0;
  uint16_t waterfall[WATERFALL_W];

  YieldToProcess(true);

  // set current noise flow level for this loop
  // noise floor is constant for each spectrum update
  // this allows live noise floor updates
  if(liveNoiseFloorFlag != 1) {
    currentNF = currentNoiseFloor[currentBand];
  }

  // initialize yOldPlot if this is a new spectrum
  // otherwise copy y values from last loop
  if(newSpectrumFlag == 0) {
    memset(yOldPlot, SPECTRUM_BOTTOM, SPECTRUM_RES * sizeof(uint16_t));
    newSpectrumFlag = 1;
  }

  // Draw the frequency and audio spectrums, gather data for waterfall
  for(int x1 = 0; x1 < SPECTRUM_RES - 1; x1++) {
    bool drawSpec = true, eraseSpec = true, inBoxLow = true, inBoxHigh = true;

    // calculate the freq spectrum plot value; pixelnew spectrum is calculated in ZoomFFTExe
    yPlot = spectrumNoiseFloor - pixelnew[x1] - currentNF;
    y1Plot = spectrumNoiseFloor - pixelnew[x1 + 1] - currentNF;

    // create rough spectrum histogram if auto noise floor is active
    // the frequency spectrum is 150 pixels high, let's create
    // rough histogram 30 bins wide (or 5 pixels each, ie, divide by 5)
    // you might think divide by 4 would be more efficient as 2 right shifts
    // but right shift of a negative number is implimentation specific
    // and I want to keep the negative numbers here
    if(liveNoiseFloorFlag == 1) {
      int specPlotY = spectrumNoiseFloor - yPlot; // actual spectrum value at current noise floor
      int bin = specPlotY / 5;                    // divide by 5 to get histogram bin

      // hLo and hHi capture spectrum at or outside the spectrum display extremes
      // this is all we need to automatically set the noise floor
      // *** TODO: consider using other histogram bins to more rapidly set noise flow ***
      if(bin < 1) {
        hLo += 1;
      } else if(bin >= 29) {
        hHi += 1;
      }
    }

    // clear erase flag if we don't need to erase anything
    if((yOldPlot[x1] == SPECTRUM_BOTTOM) && (yOldPlot[x1 + 1] == SPECTRUM_BOTTOM)) {
      eraseSpec = false;
    }
    if((yOldPlot[x1] == SPECTRUM_TOP_Y) && (yOldPlot[x1 + 1] == SPECTRUM_TOP_Y)) {
      eraseSpec = false;
    }

    // erase the old spectrum if needed
    if(eraseSpec && (displayState == DISPLAY_T41)) {
      tft.drawLine(SPECTRUM_LEFT_X + x1, yOldPlot[x1 + 1], SPECTRUM_LEFT_X + x1, yOldPlot[x1], RA8875_BLACK);
    }

    // prevent drawing spectrum outside of the spectrum area
    // also clear draw flag if we don't need to draw anything
    if(yPlot > SPECTRUM_BOTTOM) {
      yPlot = SPECTRUM_BOTTOM;
      inBoxLow = false;
    }
    if(y1Plot > SPECTRUM_BOTTOM) {
      y1Plot = SPECTRUM_BOTTOM;
      drawSpec = inBoxLow ? true : false;
    }
    if(yPlot < SPECTRUM_TOP_Y) {
      yPlot = SPECTRUM_TOP_Y;
      inBoxHigh = drawSpec ? false : true;
    }
    if(y1Plot < SPECTRUM_TOP_Y) {
      y1Plot = SPECTRUM_TOP_Y;
      drawSpec = inBoxHigh ? true : false;
    }

    // draw the new spectrum if needed
    if(drawSpec && (displayState == DISPLAY_T41)) {
      tft.drawLine(SPECTRUM_LEFT_X + x1, y1Plot, SPECTRUM_LEFT_X + x1, yPlot, RA8875_YELLOW);
    }

    // save pot value to erase spectrum next loop
    yOldPlot[x1] = yPlot;

#ifdef T41_REMOTE_DISPLAY
    if(connected) {
      freqData[x1] = yPlot;
    }
#endif

    // update audio spectrum if within range
    // don't overwrite audio filter lines
    if(x1 < AUDIO_SPEC_RES && (x1 != filterLoPosition) && (x1 != filterHiPosition)) {
      // *** TODO: consider adding audio spectrum for transmission ***
      // erase old audio spectrum line at this position if present
      if(yOldAudioPlot[x1] != 0) {
        tft.drawFastVLine(AUDIO_SPEC_L + x1, AUDIO_SPEC_BOTTOM - yOldAudioPlot[x1], yOldAudioPlot[x1], RA8875_BLACK);
      }

      // draw current audio spectrum line at this position
      if(audioYPixel[x1] != 0) {
        // maintain spectrum within box
        if(audioYPixel[x1] > CLIP_AUDIO_PEAK)
        {
          audioYPixel[x1] = CLIP_AUDIO_PEAK;
        }
        tft.drawFastVLine(AUDIO_SPEC_L + x1, AUDIO_SPEC_BOTTOM - audioYPixel[x1], audioYPixel[x1], RA8875_MAGENTA);  // draw new AUDIO spectrum line
      }

      // save data to erase next loop
      yOldAudioPlot[x1] = audioYPixel[x1];
    }

    // create data for waterfall
    wfGradIndex = -yPlot + 230;  // Nudged waterfall towards blue
    //wfGradIndex = (int)(x1 / 50) + currentNF * 10; // test color gradient
    if(wfGradIndex < 0) wfGradIndex = 0;
    //if(wfGradIndex > 117) wfGradIndex = 117;
    if(wfGradIndex > 116) wfGradIndex = 116; // *** above is out of range of gradient
    waterfall[x1] = gradient[wfGradIndex];  // Try to put pixel values in middle of gradient array

#ifdef NO_DISPLAY
    // along with the delay in the main loop this duplicates overall loop timing
    // with a display.  These are needed to regulate the flow of messages to the
    // PC control app.  These may not be needed if that app isn't used.
    delayMicroseconds(147);
#endif

    YieldToProcess();
  }

  // save last plot value for erasing on next loop
  yOldPlot[SPECTRUM_RES - 1] = y1Plot;

  // update S-meter once per loop
  DrawSmeterBar();

#ifdef T41_REMOTE_DISPLAY
  if(connected) {
    freqData[511] = pixelnew[SPECTRUM_RES - 1];
  }
#endif

  // adjust noise floor if auto noise floor is active
  if(liveNoiseFloorFlag == 1) {
    // auto noise floor give priority to ensuring the noise floor is visible in the lower portion of the spectrum display
    // the spectrum is 512 pixels wide, the noise floor is adjusted as follows (in order of priority):
    //    1) increased if more than a 20% of the spectrum is the bottom bin
    //    2) decreased if more than 5% is in the top bin
    //    3) decrease if less than 10% is in bottom bin
    // *** TODO: consider using other histogram bins to more rapidly set noise flow ***
    if(hLo > 102) {
      currentNF += 1;
    } else if((hHi > 25) || (hLo < 51)) {
      currentNF -= 1;
    }
  }

  // update noise floor sent to PC control app
  if(controlDataFlag) {
    nf2PC = currentNF;
  }

  // scroll the waterfall display
  // Use the Block Transfer Engine (BTE) to move waterfall down a line
  // copy the waterfall to layer 2, moving it down to row 2
  //
  // *** The waterfall update takes ~20ms or more in total.
  //     The process depends on this in part to ensure that the IQ input
  //     buffers have sufficient data to process at the start of the next
  //     loop.  Spectrum updates are skipped if this isn't the case.
  if(displayState == DISPLAY_T41) {
    tft.BTE_move(WATERFALL_L, WATERFALL_T, WATERFALL_W, wfRows, WATERFALL_L, WATERFALL_T + 1, 1, 2);
    tft.readStatus(); // Make sure it is done.  Memory moves can take time. This is blocking. *** might need to be changed back to original if blocking nature is modified ***

    YieldToProcess();

    // copy the waterfall back to layer 1, row 2
    tft.BTE_move(WATERFALL_L, WATERFALL_T + 1, WATERFALL_W, wfRows, WATERFALL_L, WATERFALL_T + 1, 2);
    tft.readStatus(); // Make sure it's done.

    // write new row of data into the top row to finish the scrolling effect
    tft.writeRect(WATERFALL_L, WATERFALL_T, WATERFALL_W, 1, waterfall);

    // update FT8 msg if appropriate
    //if(ft8MsgSelectActive) {
    if(ft8MsgSelectActive) {
      DisplayMessages();
    }
  }
}

/*****
  Purpose: this routine prints the frequency bars under the spectrum display
           and displays the bandwidth bar indicating demodulation bandwidth
*****/
FLASHMEM void ShowBandwidthBarValues() {
  char buff[10];
  int posLeft, posRight;
  int loColor = RA8875_LIGHT_GREY;
  int hiColor = RA8875_LIGHT_GREY;
  float loValue = (float)currentFilterLoCut * 0.001;
  float hiValue = (float)currentFilterHiCut * 0.001;
  float tmp;

  tft.writeTo(L2); // switch to layer 2

  tft.setFontScale((enum RA8875tsize)0);
  posLeft = centerLine - tft.getFontWidth() * 9 - 10;
  posRight = centerLine - 10; // MyDrawFloat provides some padding I guess is intended to erase old value

  // erase old values (needed when mode changes, could just do it there)
  tft.fillRect(posLeft, FILTER_PARAMETERS_Y, 200, tft.getFontHeight(), RA8875_BLACK);

  // set color of active filter value to green
  switch(bands[currentBand].demod) {
    case DEMOD_USB:
    case DEMOD_PSK31_WAV:
    case DEMOD_PSK31:
    case DEMOD_FT8_WAV:
    case DEMOD_FT8:
      if(!ft8MsgSelectActive) {
        if(lowerAudioFilterActive) {
          loColor = RA8875_GREEN;
        } else {
          hiColor = RA8875_GREEN;
        }
      }
      break;

    case DEMOD_LSB:
      tmp = hiValue;
      hiValue = -loValue;
      loValue = -tmp;
      if(lowerAudioFilterActive) {
        hiColor = RA8875_GREEN;
      } else {
        loColor = RA8875_GREEN;
      }
      break;

    case DEMOD_NFM:
      if(nfmBWFilterActive) {
        hiColor = RA8875_GREEN;
      }
      hiValue = (float)(nfmFilterBW / 1000.0f);
      posRight = centerLine - tft.getFontWidth() * 5 - 4;
      break;

    case DEMOD_AM:
    case DEMOD_SAM:
      loValue = -hiValue;
      loColor = RA8875_GREEN;
      hiColor = RA8875_GREEN;
      break;

    default:
      loColor = RA8875_GREEN;
      hiColor = RA8875_GREEN;
      break;
  }

  if(bands[currentBand].demod != DEMOD_NFM) {
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
  Purpose: ShowSpectrumdBScale()*****/
FLASHMEM void ShowSpectrumdBScale() {
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
*****/
FLASHMEM void ShowSpectrumFreqValues() {
  char txt[16];

  int bignum;
  int centerIdx;
  int pos_help;
  float disp_freq;

  float freq_calc;
  float grat;
  // positions for graticules: first for spectrumZoom < 3, then for spectrumZoom > 2
  const static int idx2pos[2][9] = {
    { -43, 21, 50, 250, 140, 250, 232, 250, 315 },
    { -43, 21, 50, 85, 200, 200, 232, 218, 315 }
  };

  grat = 24.0 / (float)(1 << spectrumZoom);  // 24 = sample rate / 8

  tft.setTextColor(RA8875_WHITE);
  tft.setFontScale((enum RA8875tsize)0);

  // erase frequency bar values
  tft.fillRect(SPECTRUM_LEFT_X, SPEC_BOX_LABELS, SPECTRUM_RES + 5, tft.getFontHeight(), RA8875_BLACK);

  freq_calc = (float)(centerFreq / NEW_SI5351_FREQ_MULT);  // get current frequency in Hz

  // TODO: *** this is misplaced *** shows problem with original code transitioning between VFOs
  //if(activeVFO == VFO_A) {
  //  currentFreqA = TxRxFreq;
  //} else {
  //  currentFreqB = TxRxFreq;
  //}

  if(spectrumZoom == 0) {
    freq_calc += 48000.0; // intermediate freq
  }

  // TODO: *** these are the same ***
  if(spectrumZoom < 5) {
    freq_calc = roundf(freq_calc / 1000);  // round graticule frequency to the nearest kHz
  //} else if(spectrumZoom < 5) {
  //  freq_calc = roundf(freq_calc / 100) / 10;  // round graticule frequency to the nearest 100Hz
  }

  if(spectrumZoom != 0)
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

  if(spectrumZoom == 0) {
    tft.setCursor(centerLine - 140, SPEC_BOX_LABELS);
  } else {
    tft.setCursor(centerLine - 20, SPEC_BOX_LABELS);
  }

  tft.print(txt);
  tft.setTextColor(RA8875_WHITE);

  /**************************************************************************************************
     PRINT ALL OTHER FREQUENCIES (NON-CENTER)
   **************************************************************************************************/
  // snprint() extremely memory inefficient. replaced with simple str?? functions
  for(int idx = -4; idx < 5; idx++) {
    pos_help = idx2pos[spectrumZoom < 3 ? 0 : 1][idx + 4];
    if(idx != centerIdx) {
      ultoa((freq_calc + (idx * grat)), txt, DEC);

      if(idx < 4) {
        tft.setCursor(SPECTRUM_LEFT_X + pos_help * xExpand + 40, SPEC_BOX_LABELS);
      } else {
        tft.setCursor(SPECTRUM_LEFT_X + (pos_help + 9) * xExpand + 59 - strlen(txt)*tft.getFontWidth(), SPEC_BOX_LABELS);
      }

      tft.print(txt);
      if(idx < 4) {
        tft.drawFastVLine((SPECTRUM_LEFT_X + pos_help * xExpand + 60), SPEC_BOX_LABELS - 5, 7, RA8875_YELLOW);  // Tick marks depending on zoom
      } else {
        tft.drawFastVLine((SPECTRUM_LEFT_X + (pos_help + 9) * xExpand + 59), SPEC_BOX_LABELS - 5, 7, RA8875_YELLOW);
      }
    }

    // TODO: *** ??? display is messed up for frequencies under 1000 ***
    if(spectrumZoom > 2 || freq_calc > 1000) {
      idx++;
    }
  }
}

/*****
  Purpose: To display the current transmission frequency, band, mode, and sideband above the spectrum display
*****/
FLASHMEM void ShowOperatingStats() {
  tft.setFontScale((enum RA8875tsize)0);

  // clear operating stats
  tft.fillRect(OPERATION_STATS_L, OPERATION_STATS_T, OPERATION_STATS_W, tft.getFontHeight(), RA8875_BLACK);

  // print center frequency
  tft.setCursor(OPERATION_STATS_L, OPERATION_STATS_T);
  tft.setTextColor(RA8875_WHITE);
  tft.print("Center Freq");
  tft.setCursor(OPERATION_STATS_CF, OPERATION_STATS_T);
  tft.setTextColor(RA8875_LIGHT_ORANGE);
  if(spectrumZoom == 0) {
    tft.print(centerFreq + 48000);
  } else {
    tft.print(centerFreq);
  }

  // print band for the active VFO
  tft.setTextColor(RA8875_LIGHT_ORANGE);
  tft.setCursor(OPERATION_STATS_BD, OPERATION_STATS_T);
  if(activeVFO == VFO_A) {
    tft.print(bands[currentBandA].name);  // Show band -- 40M
  } else {
    tft.print(bands[currentBandB].name);  // Show band -- 40M
  }

  tft.setTextColor(RA8875_GREEN);
  tft.setCursor(OPERATION_STATS_MD, OPERATION_STATS_T);

  switch(radioMode) {
    case CW_MODE:
      tft.print("CW ");
      tft.setCursor(OPERATION_STATS_CWF, OPERATION_STATS_T);
      tft.print(menuOptions[1][CWFilterIndex]);
      break;

    case SSB_MODE:
      tft.print("SSB");
      break;

    case DATA_MODE:
      tft.print("DATA");
      break;
  }

  tft.setCursor(OPERATION_STATS_DMD, OPERATION_STATS_T);
  tft.setTextColor(RA8875_WHITE);

  switch(bands[currentBand].demod) {
    case DEMOD_USB:
    case DEMOD_LSB:
    case DEMOD_PSK31_WAV:
    case DEMOD_PSK31:
    case DEMOD_FT8:
    case DEMOD_FT8_WAV:
      if(activeVFO == VFO_A) {
        tft.print(DEMOD[bands[currentBandA].demod].text);
      } else {
        tft.print(DEMOD[bands[currentBandB].demod].text);
      }
      break;

    case DEMOD_AM:
      tft.print("AM");
      break;

    case DEMOD_NFM:
      tft.print("NFM");
      break;

    case DEMOD_SAM:
      tft.print("SAM ");
      break;
  }

  ShowCurrentPowerSetting();
}

/*****
  Purpose: Display current power setting
*****/
FLASHMEM void ShowCurrentPowerSetting() {
  tft.setFontScale((enum RA8875tsize)0);
  tft.fillRect(OPERATION_STATS_PWR, OPERATION_STATS_T, tft.getFontWidth() * 11, tft.getFontHeight(), RA8875_BLACK);
  tft.setCursor(OPERATION_STATS_PWR, OPERATION_STATS_T);
  tft.setTextColor(RA8875_RED);
  tft.print(transmitPowerLevel, 1);  // Power output is a float
  tft.print(" Watts");
}

/*****
  Purpose: Update CW Filter
*****/
FLASHMEM void UpdateCWFilter() {
  float CWFilterPosition = 85.0; // max filter position

  tft.writeTo(L2);
  if(radioMode == CW_MODE) {
    switch(CWFilterIndex) {
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

    // *** TODO: drawing and clearing filter lines needs updated ***
    tft.fillRect(AUDIO_SPEC_L, AUDIO_SPEC_T, CWFilterPosition, 120, MAROON);
    // this bounding line is confusing given the filter lines already in the audio spectrum box
    //tft.drawFastVLine(AUDIO_SPEC_BOX_L + 2 + CWFilterPosition, AUDIO_SPEC_BOX_T, AUDIO_SPEC_BOX_H, RA8875_LIGHT_GREY);
  } else {
    // clear CW filter
    tft.fillRect(AUDIO_SPEC_L, AUDIO_SPEC_T, CWFilterPosition, 120, RA8875_BLACK);
  }

  tft.writeTo(L1);
}

/*****
  Purpose: Show main frequency display at top
*****/
FASTRUN void ShowFrequency() {
  char freqBuffer[15];

  // *** do this in the proper place if this is needed ***
  //if(activeVFO == VFO_A) {  // Needed for edge checking
  //  currentBand = currentBandA;
  //} else {
  //  currentBand = currentBandB;
  //}

  FormatFrequency(TxRxFreq, freqBuffer);
  tft.setFontScale(3, 2);
//  tft.fillRect(0, FREQUENCY_Y, SPEC_BOX_W, tft.getFontHeight(), RA8875_BLACK);
  tft.fillRect(FREQUENCY_X, FREQUENCY_Y, TIME_X - 20, tft.getFontHeight(), RA8875_BLACK);

  if(activeVFO == VFO_A) {
    if(TxRxFreq < bands[currentBandA].fBandLow || TxRxFreq > bands[currentBandA].fBandHigh) {
      tft.setTextColor(RA8875_RED);  // Out of band
    } else {
      tft.setTextColor(RA8875_GREEN); // In US band
    }
    tft.setCursor(FREQUENCY_X, FREQUENCY_Y);
    tft.print(freqBuffer); // Show VFO_A

    tft.setFontScale(1, 2);
    tft.setTextColor(RA8875_LIGHT_GREY);
    tft.setCursor(VFO_B_INACTIVE_OFFSET, FREQUENCY_Y);
    FormatFrequency(currentFreqB, freqBuffer);
  } else { // VFO_B
    if(TxRxFreq < bands[currentBandB].fBandLow || TxRxFreq > bands[currentBandB].fBandHigh) {
      tft.setTextColor(RA8875_RED);
    } else {
      tft.setTextColor(RA8875_GREEN);
    }
    tft.setCursor(VFO_B_ACTIVE_OFFSET, FREQUENCY_Y);
    tft.print(freqBuffer); // Show VFO_B

    tft.setFontScale(1, 2);
    tft.setTextColor(RA8875_LIGHT_GREY);
    tft.setCursor(FREQUENCY_X + 20, FREQUENCY_Y);
    FormatFrequency(currentFreqA, freqBuffer);
  }

  tft.print(freqBuffer); // Show the other one
}

// this variable determines the pixels per S step. In the original code it was 12.2 pixels !?
const float pixels_per_s = 12;
/*****
  Purpose: Display dBm
*****/
FASTRUN void DrawSmeterBar() {
  char buff[10];
  //const char *unit_label;
  int16_t smeterPad;
  float32_t dbm;

  // the S-Meter bar and the dBm value were inconsistent, as they were using different base values.
  // Moreover the bar could go over the limits of the S-meter box, as the map() function, does not constrain the values
  // S-Meter bar is consistent with the dBm value and the S-Meter bar will always be restricted to the box
  tft.fillRect(SMETER_X + 1, SMETER_Y + 1, SMETER_BAR_LENGTH, SMETER_BAR_HEIGHT, RA8875_BLACK); // Erase old bar

  dbm = CalcSignalStrength();

  // determine length of S-meter bar, limit it to the box and draw it
  smeterPad = map(dbm, -73.0-9*6.0 /*S1*/, -73.0 /*S9*/, 0, 9*pixels_per_s);

  // make sure, that it does not extend beyond the field
  smeterPad = max(0, smeterPad);
  smeterPad = min(SMETER_BAR_LENGTH, smeterPad);
  tft.fillRect(SMETER_X + 1, SMETER_Y + 2, smeterPad, SMETER_BAR_HEIGHT-2, RA8875_RED); // bar 2*1 pixel smaller than the field

  tft.setTextColor(RA8875_WHITE);
  //unit_label = "dBm";
  tft.setFontScale((enum RA8875tsize)0);

  tft.fillRect(SMETER_X + 185, SMETER_Y, 80, tft.getFontHeight(), RA8875_BLACK);  // The dB figure at end of S

  // consider no decimals in the S-meter dBm value as it is very busy with decimals
  MyDrawFloat(dbm, /*0*/ 1, SMETER_X + 184, SMETER_Y, buff);
  tft.setTextColor(RA8875_GREEN);
  tft.print("dBm");

  if(controlDataFlag) {
    SendSmeter(smeterPad, dbm);
  }
}

/*****
  Purpose: format a floating point number

  Parameter list:
    float val         the value to format
    int decimals      the number of decimal places
    int x             the x coordinate for display
    int y                 y          "
*****/
FLASHMEM void MyDrawFloat(float val, int decimals, int x, int y, char *buff) {
  MyDrawFloatP(val, decimals, x, y, buff, FLOAT_PRECISION);
}

FLASHMEM void MyDrawFloatP(float val, int decimals, int x, int y, char *buff, int width) {
  dtostrf(val, width, decimals, buff);  // Use 8 as that is the max prevision on a float

  tft.fillRect(x, y, width * tft.getFontWidth(), 15, RA8875_BLACK);
  tft.setCursor(x, y);

  tft.print(buff);
}

/*****
  Purpose: This function redraws the entire display screen where the equalizers appeared
*****/
FLASHMEM void RedrawDisplayScreen() {
  // clear display
  tft.fillWindow();

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

  UpdateInfoBox();
}

/*****
  Purpose: Draw Tuned Bandwidth on Spectrum Plot
*****/
FASTRUN void DrawBandwidthBar() {
  float zoomMultFactor = 0.0;
  float Zoom1Offset = 0.0;
  float32_t pixel_per_khz;
  int NCOFreqX;
  int newFilterX = 0;
  int newFilterWidth = 0;
  static int oldFilterX = 0;
  static int oldFilterWidth = 0;
  static int oldTuneLine = 0;

  switch(spectrumZoom) {
    case 0:
      zoomMultFactor = 0.5;
      Zoom1Offset = 24000 * 0.0053333;
      break;

    case 1:
      zoomMultFactor = 1.0;
      break;

    case 2:
      zoomMultFactor = 2.0;
      break;

    case 3:
      zoomMultFactor = 4.0;
      break;

    case 4:
      zoomMultFactor = 8.0;
      break;
  }
  NCOFreqX = (int)(NCOFreq * 0.0053333) * zoomMultFactor - Zoom1Offset;

  pixel_per_khz = ((1 << spectrumZoom) * SPECTRUM_RES / 192.0);
  newFilterWidth = (int)(((float)(currentFilterHiCut - currentFilterLoCut) / 1000.0) * pixel_per_khz * 1.06);

  // make sure bandwidth is within zoom range
  switch(bands[currentBand].demod) {
    case DEMOD_USB:
    case DEMOD_PSK31_WAV:
    case DEMOD_PSK31:
    case DEMOD_FT8:
    case DEMOD_FT8_WAV:
      if(centerLine + NCOFreqX + newFilterWidth > SPECTRUM_RES) {
        resetTuningFlag = true;
      }
      break;

    case DEMOD_LSB:
      if(centerLine + NCOFreqX - newFilterWidth < 0) {
        resetTuningFlag = true;
      }
      break;

    case DEMOD_NFM:
      newFilterWidth = (int)((nfmFilterBW / 1000.0) * pixel_per_khz * 1.06);

    case DEMOD_AM:
    case DEMOD_SAM:
      if((centerLine - (newFilterWidth / 2) * 0.93 + NCOFreqX < 0) || (centerLine + (newFilterWidth / 2) * 0.93 + NCOFreqX > SPECTRUM_RES)) {
        resetTuningFlag = true;
      }
      break;
  }

  // erase old bar
  tft.writeTo(L2);
  //tft.fillRect(SPECTRUM_LEFT_X, SPECTRUM_TOP_Y + 20, SPECTRUM_RES, SPECTRUM_HEIGHT - 20, RA8875_BLACK);
  tft.fillRect(oldFilterX, SPECTRUM_TOP_Y + 20, oldFilterWidth + 1, SPECTRUM_HEIGHT - 20, RA8875_BLACK);
  tft.drawFastVLine(oldTuneLine, SPECTRUM_TOP_Y + 20, SPECTRUM_HEIGHT - 20, RA8875_BLACK);

  // update bar if we haven't reset tuning, otherwise this gets recalled by that routine
  if(!resetTuningFlag) {
    switch(bands[currentBand].demod) {
      case DEMOD_USB:
      case DEMOD_PSK31_WAV:
      case DEMOD_PSK31:
      case DEMOD_FT8:
      case DEMOD_FT8_WAV:
        newFilterX = centerLine + NCOFreqX + (float)currentFilterLoCut / 1000.0 * pixel_per_khz;
        break;

      case DEMOD_LSB:
        newFilterX = centerLine - newFilterWidth + NCOFreqX - (float)currentFilterLoCut / 1000.0 * pixel_per_khz;
        break;

      case DEMOD_NFM:
        newFilterWidth = (int)((nfmFilterBW / 1000.0) * pixel_per_khz * 1.06);
        newFilterX = centerLine - (newFilterWidth / 2) * 0.93 + NCOFreqX;
        newFilterWidth *= 0.95;
        break;

      case DEMOD_AM:
      case DEMOD_SAM:
        newFilterX = centerLine - (newFilterWidth / 2) * 0.93 + NCOFreqX;
        newFilterWidth *= 0.95;
        break;
    }

    // draw bandwidth bar and centerline
    tft.fillRect(newFilterX, SPECTRUM_TOP_Y + 20, newFilterWidth, SPECTRUM_HEIGHT - 20, FILTER_WIN);
    tft.drawFastVLine(centerLine + NCOFreqX, SPECTRUM_TOP_Y + 20, SPECTRUM_HEIGHT - 20, RA8875_CYAN);

    oldFilterX = newFilterX;
    oldFilterWidth = newFilterWidth;
    oldTuneLine = centerLine + NCOFreqX;
  }

  tft.writeTo(L1);
}

/*****
  Purpose: This function draws spectrum display container
*****/
FLASHMEM void DrawSpectrumFrame() {
  tft.drawRect(SPEC_BOX_L, SPEC_BOX_T, SPEC_BOX_W, SPEC_BOX_H, RA8875_YELLOW);
}

/*****
  Purpose: This function removes the spectrum display container
*****/
FLASHMEM void EraseSpectrumDisplayContainer() {
  tft.fillRect(SPECTRUM_LEFT_X - 2, SPECTRUM_TOP_Y - 1, SPECTRUM_RES + 6, SPECTRUM_HEIGHT + 8, RA8875_BLACK);  // Spectrum box
}

/*****
  Purpose: This function erases the contents of the spectrum display
*****/
FLASHMEM void EraseSpectrumWindow() {
  newSpectrumFlag = 0; // old noise floor needs reset
  tft.fillRect(SPECTRUM_LEFT_X, SPECTRUM_TOP_Y, SPECTRUM_RES, SPECTRUM_HEIGHT, RA8875_BLACK);  // Spectrum box
}

/*****
  Purpose: Draw S-Meter container
*****/
FLASHMEM void DrawSMeterContainer() {
  int i;
  //  the white line must only go till S9
  tft.drawFastHLine(SMETER_X, SMETER_Y - 1, 9 * pixels_per_s, RA8875_WHITE);
  tft.drawFastHLine(SMETER_X, SMETER_Y + SMETER_BAR_HEIGHT+2, 9 * pixels_per_s, RA8875_WHITE);  // changed 6 to 20

  for(i = 0; i < 10; i++) {                                                // Draw tick marks for S-values
    // draw wider tick marks in the style of the Teensy Convolution SDR
    tft.drawRect(SMETER_X + i * pixels_per_s, SMETER_Y - 6-(i%2)*2, 2, 6+(i%2)*2, RA8875_WHITE);
  }

  //  the green line must start at S9
  tft.drawFastHLine(SMETER_X + 9*pixels_per_s, SMETER_Y - 1, SMETER_BAR_LENGTH+2-9*pixels_per_s, RA8875_GREEN);
  tft.drawFastHLine(SMETER_X + 9*pixels_per_s, SMETER_Y + SMETER_BAR_HEIGHT+2, SMETER_BAR_LENGTH+2-9*pixels_per_s, RA8875_GREEN);

  for(i = 1; i <= 3; i++) {                                                     // Draw tick marks for s9+ values in 10dB steps
    // draw wider tick marks in the style of the Teensy Convolution SDR
    tft.drawRect(SMETER_X + 9*pixels_per_s + i * pixels_per_s*10.0/6.0, SMETER_Y - 8+(i%2)*2, 2, 8-(i%2)*2, RA8875_GREEN);
  }

  tft.drawFastVLine(SMETER_X, SMETER_Y - 1, SMETER_BAR_HEIGHT+3, RA8875_WHITE);
  tft.drawFastVLine(SMETER_X + SMETER_BAR_LENGTH+2, SMETER_Y - 1, SMETER_BAR_HEIGHT+3, RA8875_GREEN);

  tft.setFontScale((enum RA8875tsize)0);

  tft.setTextColor(RA8875_WHITE);
  // moved single digits a bit to the right, to align
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
  tft.setCursor(SMETER_X + 133, SMETER_Y - 25);
  tft.print("+20dB");

  //ShowSpectrumFreqValues();
  //ShowSpectrumdBScale();
}

/*****
  Purpose: Draw audio spectrum box

  Parameter list:
*****/
FLASHMEM void DrawAudioSpectContainer() {
  float ticks = (float)(AUDIO_SPEC_RES) / AUDIO_SPEC_SPAN * 1000.0;

  tft.drawRect(AUDIO_SPEC_BOX_L, AUDIO_SPEC_BOX_T, AUDIO_SPEC_BOX_W, AUDIO_SPEC_BOX_H, RA8875_WHITE);
  tft.drawFastVLine(AUDIO_SPEC_BOX_L + 1, AUDIO_SPEC_BOX_BOTTOM, 15, RA8875_WHITE);
  tft.setCursor(AUDIO_SPEC_BOX_L - 3, AUDIO_SPEC_BOX_BOTTOM + 16);
  tft.print(0);
  tft.print("k");
  for(int k = 1; k < 7; k++) {
    tft.drawFastVLine(AUDIO_SPEC_BOX_L + 1 + ((float)k * ticks), AUDIO_SPEC_BOX_BOTTOM, 15, RA8875_WHITE);
    tft.setCursor(AUDIO_SPEC_BOX_L - 3 + ((float)k * ticks), AUDIO_SPEC_BOX_BOTTOM + 16);
    tft.print(k);
    tft.print("k");
  }
}

/*****
  Purpose: To erase both primary and secondary menus from display

  Parameter list:
*****/
FLASHMEM void EraseMenus() {
  tft.fillRect(PRIMARY_MENU_X, MENUS_Y, BOTH_MENU_WIDTHS, CHAR_HEIGHT + 1, RA8875_BLACK);  // Erase menu choices
//  menuStatus = NO_MENUS_ACTIVE;                                                            // Change menu state
}

/*****
  Purpose: To erase primary menu from display

  Parameter list:
*****/
FLASHMEM void ErasePrimaryMenu() {
  tft.fillRect(PRIMARY_MENU_X, MENUS_Y, EACH_MENU_WIDTH, CHAR_HEIGHT + 1, RA8875_BLACK);  // Erase menu choices
//  menuStatus = NO_MENUS_ACTIVE;                                                           // Change menu state
}

/*****
  Purpose: To erase secondary menu from display

  Parameter list:
*****/
FLASHMEM void EraseSecondaryMenu() {
  tft.fillRect(SECONDARY_MENU_X, MENUS_Y, EACH_MENU_WIDTH, CHAR_HEIGHT + 1, RA8875_BLACK);  // Erase menu choices
//  menuStatus = NO_MENUS_ACTIVE;                                                             // Change menu state
}

/*****
  Purpose: Shows transmit (red) and receive (green) mode

  Parameter list:
*****/
FLASHMEM void ShowTransmitReceiveStatus() {
  tft.setFontScale((enum RA8875tsize)1);
  tft.setTextColor(RA8875_BLACK);

  switch(radioState) {
    case SSB_TRANSMIT_STATE:
    case CW_TRANSMIT_STRAIGHT_STATE:
    case CW_TRANSMIT_KEYER_STATE:
      tft.fillRect(X_R_STATUS_X, X_R_STATUS_Y, 55, 25, RA8875_RED);
      tft.setCursor(X_R_STATUS_X + 4, X_R_STATUS_Y - 5);
      tft.print("XMT");
      break;

    default:
      tft.fillRect(X_R_STATUS_X, X_R_STATUS_Y, 55, 25, RA8875_GREEN);
      tft.setCursor(X_R_STATUS_X + 4, X_R_STATUS_Y - 5);
      tft.print("REC");
      break;
  }
}

/*****
  Purpose: Set frequency display to specified level
    *** TODO: needs reset tuning if bandwidth
*****/
FLASHMEM void SetZoom(int zoom) {
  spectrumZoom = zoom;

  if(spectrumZoom == MAX_ZOOM_ENTRIES) {
    spectrumZoom = 0;
  }
  if(spectrumZoom < 0) {
    spectrumZoom = MAX_ZOOM_ENTRIES - 1;
  }

  ZoomFFTPrep();
  UpdateInfoBoxItem(IB_ITEM_ZOOM);
  DrawBandwidthBar();
  ShowSpectrumFreqValues();
}

/*****
  Purpose: Draw static items on display
*****/
FLASHMEM void DrawStaticDisplayItems() {
  ShowName();
  DrawSpectrumFrame();
  DrawSMeterContainer();
  DrawAudioSpectContainer();
  ClearInfoBox();
}

#ifdef HOST_KEYBOARD_MOUSE_SUPPORT
// for testing only
FLASHMEM void PrintKeyboardBuffer() {
  tft.setFontScale(0,1);
  tft.setCursor(WATERFALL_L, YPIXELS - 32); // this is minimum above bottom of display to get lower case characters to fully show
  tft.setTextColor(RA8875_WHITE);

  kbBuffer[kbIndexIn] = 0; // terminate buffer
  tft.print((char *)kbBuffer);
}
#endif
