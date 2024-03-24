#include "SDT.h"
#include "ButtonProc.h"
#include "CWProcessing.h"
#include "Display.h"
#include "EEPROM.h"
#include "InfoBox.h"
#include "Menu.h"
#include "Process.h"
#include "Utility.h"

//-------------------------------------------------------------------------------------------------------------
// Forwards
//-------------------------------------------------------------------------------------------------------------

void IBDecoderFollowup();
void IBCompressionFollowup();
void IBWPMFollowup();
void IBVolFollowup();
void IBEQFollowup();
void IBTempFollowup();
void IBLoadFollowup();
void DrawInfoBoxFrame();

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

#define IB_COL_1_X        INFO_BOX_L + 90  // X coordinate for info box 1st column field
#define IB_COL_2_X        INFO_BOX_L + 220 // X coordinate for info box 2nd column field
#define IB_COL_2L_X       INFO_BOX_L + 205 // X coordinate for info box 2nd column field
#define IB_ROW_1_Y        INFO_BOX_T + 1
#define IB_ROW_2_Y        IB_ROW_1_Y + 12
#define IB_ROW_3_Y        IB_ROW_2_Y + 20
#define IB_ROW_4_Y        IB_ROW_3_Y + 20
#define IB_ROW_5_Y        IB_ROW_4_Y + 20
#define IB_ROW_6_Y        IB_ROW_5_Y + 20
#define IB_ROW_7_Y        IB_ROW_6_Y + 20
#define IB_ROW_8_Y        IB_ROW_7_Y + 20
#define IB_ROW_9_Y        IB_ROW_8_Y + 20
#define IB_ROW_10_Y       IB_ROW_9_Y + 20

#define DECODER_WPM_X     IB_COL_1_X + 37

const char *agcOpts[] = { "Off", "L", "S", "M", "F" };
const char *tuneValues[] = { "10", "50", "100", "250", "1000", "10000", "100000", "1000000" };
const char *ftValues[] = { "10", "50", "250", "500" };
const char *filter[] = { "Off", "Kim", "Spectral", "LMS" };
const char *onOff[2] = { "Off", "On" };
const char *optionsWPM[2] = { "Straight Key", "Paddles " };
const char *zoomOptions[] = { "1x ", "2x ", "4x ", "8x ", "16x" }; // combine with MAX_ZOOM_ENTRIES somewhere

#define IB_NUM_ITEMS 13
PROGMEM const infoBoxItem infoBox[] = 
{ //                                                     font    # chars
  // label         Options      option                   size    to erase  flag  col            row,           follow-up function
  { "Vol:",        NULL,        NULL,                     1,        3,      0,   IB_COL_1_X,    IB_ROW_1_Y,    &IBVolFollowup         }, // Tune Inc
  { "AGC",         agcOpts,     &AGCMode,                 1,        3,      1,   IB_COL_2L_X,   IB_ROW_1_Y,    NULL                   }, // Tune Inc
  { "Increment:",  tuneValues,  &tuneIndex,               0,        7,      0,   IB_COL_1_X,    IB_ROW_3_Y,    NULL                   }, // Tune Inc
  { "FT Inc:",     ftValues,    &ftIndex,                 0,        3,      0,   IB_COL_2_X,    IB_ROW_3_Y,    NULL                   }, // FT Inc
  { "AutoNotch:",  onOff,       (int*)&ANR_notchOn,       0,        3,      1,   IB_COL_1_X,    IB_ROW_4_Y,    NULL                   }, // Auto Notch
  { "Noise:",      filter,      &nrOptionSelect,          0,        8,      1,   IB_COL_1_X,    IB_ROW_5_Y,    NULL                   }, // Noise Filter
  { "Zoom:",       zoomOptions, (int*)&spectrum_zoom,     0,        3,      0,   IB_COL_1_X,    IB_ROW_6_Y,    NULL                   }, // Zoom
  { "Compress:",   onOff,       &compressorFlag,          0,        6,      1,   IB_COL_1_X,    IB_ROW_7_Y,    &IBCompressionFollowup }, // Compress
  { "Keyer:",      optionsWPM,  &EEPROMData.keyType,      0,       12,      0,   IB_COL_1_X,    IB_ROW_8_Y,    &IBWPMFollowup         }, // Keyer
  { "Decoder:",    onOff,       &decoderFlag,             0,        3,      1,   IB_COL_1_X,    IB_ROW_9_Y,    NULL                   }, // Decoder
  { "NF Set:",     onOff,       &liveNoiseFloorFlag,      0,        3,      1,   IB_COL_2_X,    IB_ROW_6_Y,    NULL                   }, // Noise Floor
  //{ "Equalizers:", NULL,        NULL,                     0,       10,      1,   IB_COL_1_X,    IB_ROW_10_Y,   &IBEQFollowup          }  // Equalizers
  { "Temp:",       NULL,        NULL,                     0,        3,      1,   IB_COL_1_X,    IB_ROW_10_Y,   &IBTempFollowup          }, // Teensy Temp
  { "Load:",       NULL,        NULL,                     0,        3,      1,   IB_COL_2_X,    IB_ROW_10_Y,   &IBLoadFollowup          }  // Teensy Load

  //{ "Vol:",       NULL,        NULL,                      1,        2,      0,   IB_COL_1_X,    IB_ROW_1_Y,    &IBVolFollowup         }, // Tune Inc
  //{ "AGC",        agcOpts,     &AGCMode,                  1,        3,      1,   IB_COL_2L_X,   IB_ROW_1_Y,    NULL                   }, // Tune Inc
  //{ "Increment:", tuneValues,  &tuneIndex,                0,        7,      0,   IB_COL_1_X,    IB_ROW_3_Y,    NULL                   }, // Tune Inc
  //{ "FT Inc:",    ftValues,    &ftIndex,                  0,        3,      0,   IB_COL_2_X,    IB_ROW_3_Y,    NULL                   }, // FT Inc
  //{ "AutoNotch:", onOff,       (int*)&ANR_notchOn,        0,        3,      1,   IB_COL_1_X,    IB_ROW_5_Y,    NULL                   }, // Auto Notch
  //{ "Noise:",     filter,      &nrOptionSelect,           0,        8,      1,   IB_COL_1_X,    IB_ROW_4_Y,    NULL                   }, // Noise Filter
  //{ "Zoom:",      zoomOptions, (int*)&spectrum_zoom,      0,        3,      0,   IB_COL_2_X,    IB_ROW_4_Y,    NULL                   }, // Zoom
  //{ "Compress:",  onOff,       &compressorFlag,           0,        6,      1,   IB_COL_1_X,    IB_ROW_6_Y,    &IBCompressionFollowup }, // Compress
  //{ "Keyer:",     optionsWPM,  &EEPROMData.keyType,       0,       12,      0,   IB_COL_1_X,    IB_ROW_8_Y,    &IBWPMFollowup         }, // Keyer
  //{ "Decoder:",   onOff,       &decoderFlag,              0,        3,      1,   IB_COL_1_X,    IB_ROW_9_Y,    NULL                   }, // Decoder
  //{ "NF Set:",    onOff,       &liveNoiseFloorFlag,       0,        3,      1,   IB_COL_2_X,    IB_ROW_5_Y,    NULL                   }  // Noise Floor

};

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

/*****
  Purpose: Updates the specified information box item

  Parameter list:
    infoBoxItem *item   Pointer to the info box item to update

  Return value:
    void
*****/
void UpdateInfoBoxItem(const infoBoxItem *item) {
  int label_x;
//  int xOffset = item->col == 1 ? IB_COL_1_X : IB_COL_2_X;
//  int yOffset = IB_ROW_1_Y + (item->row - 1) * 20;
  int xOffset = item->col;
  int yOffset = item->row;

  tft.setFontScale((enum RA8875tsize)item->fontSize);
  tft.fillRect(xOffset, yOffset, tft.getFontWidth() * item->clearWidth, tft.getFontHeight(), RA8875_BLACK);
  tft.setTextColor(RA8875_WHITE);
  label_x = xOffset - 5 - strlen(item->label) * tft.getFontWidth();
  tft.setCursor(label_x, yOffset);
  tft.print(item->label);

  if(item->Options != NULL) {
    if(item->highlightFlag && (*item->option == 0) ) {
      tft.setTextColor(RA8875_WHITE);
    } else {
      tft.setTextColor(RA8875_GREEN);
    }

    tft.setCursor(xOffset, yOffset);
    tft.print(item->Options[*item->option]);
  }

  if(item->followFnPtr != NULL) {
    item->followFnPtr();
  }
}

/*****
  Purpose: Updates the information box

  Parameter list:
    void

  Return value:
    void
*****/
void UpdateInfoBox() {
  DrawInfoBoxFrame();

  // you can update each item individually if they need done in a particular order ...
  //UpdateInfoBoxItem(&infoBox[IB_ITEM_VOL]);
  //UpdateInfoBoxItem(&infoBox[IB_ITEM_AGC]);
  //UpdateInfoBoxItem(&infoBox[IB_ITEM_TUNE]);
  //UpdateInfoBoxItem(&infoBox[IB_ITEM_FINE]);
  //UpdateInfoBoxItem(&infoBox[IB_ITEM_COMPRESS]);
  //UpdateInfoBoxItem(&infoBox[IB_ITEM_DECODER]);
  //UpdateInfoBoxItem(&infoBox[IB_ITEM_FILTER]);
  //UpdateInfoBoxItem(&infoBox[IB_ITEM_FLOOR]);
  //UpdateInfoBoxItem(&infoBox[IB_ITEM_NOTCH]);
  //UpdateInfoBoxItem(&infoBox[IB_ITEM_KEY]);
  //UpdateInfoBoxItem(&infoBox[IB_ITEM_ZOOM]);

  // ... or update them in order
  for(int i = 0; i < IB_NUM_ITEMS; i++) {
    UpdateInfoBoxItem(&infoBox[i]);
  }
}

/*****
  Purpose: Information box follow up function for the Compression item
           Assumes this is only called as part of updating Compression item

  Parameter list:
    void

  Return value:
    void
*****/
void IBCompressionFollowup() {
  if (compressorFlag == 1) {
    tft.print(" ");
    tft.print(currentMicThreshold);
  }
}

/*****
  Purpose: Information box follow up function for the Keyer item
           Assumes this is only called as part of updating Keyer item

  Parameter list:
    void

  Return value:
    void
*****/
void IBWPMFollowup() {
  if (EEPROMData.keyType == 1) { // 1 = paddles
    if (paddleFlip == 0) {
      tft.print("R");
    } else {
      tft.print("L");
    }
    tft.print(" ");
    tft.print(EEPROMData.currentWPM);
  }
}

/*****
  Purpose: Information box follow up function for the Volume item
           Assumes volume is in column 1 row 1, with large font

  Parameter list:
    void

  Return value:
    void
*****/
void IBVolFollowup() {
  tft.setFontScale((enum RA8875tsize)1);
  tft.setTextColor(RA8875_GREEN);
  tft.setCursor(IB_COL_1_X, IB_ROW_1_Y);
  tft.print(audioVolume);
}

/*****
  Purpose: Information box follow up function for the Equalizers item
           Assumes Equalizers are in column 1 row 10

  Parameter list:
    void

  Return value:
    void
*****/
void IBEQFollowup() {
  tft.setCursor(IB_COL_1_X, IB_ROW_10_Y);
  if (receiveEQFlag) {
    tft.setTextColor(RA8875_RED);
    tft.print("Rx");
    tft.setTextColor(RA8875_GREEN);
    tft.setCursor(IB_COL_1_X + 25, IB_ROW_10_Y);
    tft.print("On");
  } else {
    tft.setTextColor(RA8875_RED);
    tft.print("Rx");
    tft.setCursor(IB_COL_1_X + 25, IB_ROW_10_Y);
    tft.setTextColor(RA8875_WHITE);
    tft.print("Off");
  }
  tft.setCursor(IB_COL_1_X + 55, IB_ROW_10_Y);
  if (xmitEQFlag) {
    tft.setTextColor(RA8875_RED);
    tft.print("Tx");
    tft.setTextColor(RA8875_GREEN);
    tft.setCursor(IB_COL_1_X + 80, IB_ROW_10_Y);
    tft.print("On");
  } else {
    tft.setTextColor(RA8875_RED);
    tft.print("Tx");
    tft.setTextColor(RA8875_WHITE);
    tft.setCursor(IB_COL_1_X + 80, IB_ROW_10_Y);
    tft.print("Off");
  }
}

/*****
  Purpose: Information box follow up function for the Temp item
           Assumes temp is in column 1 row 10

  Parameter list:
    void

  Return value:
    void
*****/
void IBTempFollowup() {
  char buff[10];

  if (elapsed_micros_idx_t > (SampleRate / 960)) {
    tft.setFontScale((enum RA8875tsize)0);
    tft.setTextColor(RA8875_GREEN);
    MyDrawFloatP(TGetTemp(), 0, IB_COL_1_X, IB_ROW_10_Y, buff, 2);
    tft.drawCircle(IB_COL_1_X + 22, IB_ROW_10_Y + 5, 3, RA8875_GREEN);
  }
}

/*****
  Purpose: Information box follow up function for the Load item
           Assumes load is in column 2 row 10

  Parameter list:
    void

  Return value:
    void
*****/
void IBLoadFollowup() {
  char buff[10];
  int valueColor = RA8875_GREEN;
  double block_time;
  double processor_load;
  double elapsed_micros_mean;

  if (elapsed_micros_idx_t > (SampleRate / 960)) {
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
    tft.setTextColor(valueColor);
    MyDrawFloatP(processor_load, 0, IB_COL_2_X, IB_ROW_10_Y, buff, 2);
    tft.print("%");
    elapsed_micros_idx_t = 0;
    elapsed_micros_sum = 0;
    elapsed_micros_mean = 0;
  }
}

/*****
  Purpose: Show estimated WPM in information box
           Assumes decoder is in column 1 row 9

  Parameter list:
    void

  Return value:
    void
*****/
void UpdateIBWPM() {
  tft.setFontScale((enum RA8875tsize)0);
  tft.setTextColor(RA8875_GREEN);
  tft.fillRect(IB_COL_1_X + 37, IB_ROW_9_Y, tft.getFontWidth() * 10, tft.getFontHeight(), RA8875_BLACK);
  tft.setCursor(IB_COL_1_X + 38, IB_ROW_9_Y);
  tft.print("(");
  tft.print(1200L / (dahLength / 3));
  tft.print(" WPM)");
}

/*****
  Purpose: Update CW decode lock indicator in information box
           Assumes decoder is in column 1 row 9

  Parameter list:
    void

  Return value:
    void
*****/
void UpdateDecodeLockIndicator()
{
  // ==========  CW decode "lock" indicator
  if (combinedCoeff > 50)
  {
    tft.fillRect(IB_COL_2_X - 20, IB_ROW_9_Y, 15, 15, RA8875_GREEN);
  }
  else if (combinedCoeff < 50)
  {
    CWLevelTimer = millis();
    if (CWLevelTimer - CWLevelTimerOld > 2000)
    {
      CWLevelTimerOld = millis();
      tft.fillRect(IB_COL_2_X - 20, IB_ROW_9_Y, 17, 17, RA8875_BLACK);
    }
  }
}

/*****
  Purpose: This function draws the Info Box frame and clears the region within it

  Parameter list:
    void

  Return value;
    void
*****/
void DrawInfoBoxFrame() {
  tft.fillRect(INFO_BOX_L + 2, INFO_BOX_T + 2, INFO_BOX_W - 4, INFO_BOX_H - 4, RA8875_BLACK); // clear info box contents
  tft.drawRect(INFO_BOX_L, INFO_BOX_T, INFO_BOX_W, INFO_BOX_H, RA8875_LIGHT_GREY); // draw info box
}
