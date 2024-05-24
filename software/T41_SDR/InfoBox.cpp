#include <malloc.h>

#include "SDT.h"
#include "Button.h"
#include "ButtonProc.h"
#include "CWProcessing.h"
#include "Display.h"
#include "EEPROM.h"
#include "Encoders.h"
#include "ft8.h"
#include "InfoBox.h"
#include "Menu.h"
#include "mouse.h"
#include "Process.h"
#include "Utility.h"

//-------------------------------------------------------------------------------------------------------------
// Forwards
//-------------------------------------------------------------------------------------------------------------

void IBDecoderFollowup(int row, int col);
void IBCompressionFollowup(int row, int col);
void IBTuneIncFollowup(int row, int col);
void IBWPMFollowup(int row, int col);
void IBVolFollowup(int row, int col);
void IBEQFollowup(int row, int col);
void IBTempFollowup(int row, int col);
void IBLoadFollowup(int row, int col);
void IBFT8Followup(int row, int col);
void IBStackFollowup(int row, int col);
void IBHeapFollowup(int row, int col);

void DrawInfoBoxFrame();

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

typedef  struct {
  const char *label;      // info box label
  const char **Options;   // label options
  int *option;            // pointer to option selector
  int fontSize;           // 0 - small or 1 - large font (large font takes two rows, adjust item rows and/or IB_ROW_#_Y accordingly)
  int clearWidth;         // maximum number of characters to clear when updating field
  int highlightFlag;      // 0 - highlight all options in green, 1 - don't highlight first option, 2 - first option white, second option red, other options green

  // specifying row and col index is easiest but less flexible especailly if you use both small and large fonts
  // as in the fefault info box
  //int col, row;           // item column and row (up to 10 rows, 2 columns)
  int col, row;           // item placement by screen pixel (up to 10 rows with small font)
  void (*followFnPtr)(int row, int col);  // function to run after info box field is updated (note that these may be hard-coded to a particular location
                          // and will need updated if the underlying item is moved)
} infoBoxItem;

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

const char *ft8Opts[] = { "Off", "not sync'd", "sync'd" };

#define IB_NUM_ITEMS 12

PROGMEM const infoBoxItem infoBox[] =
{ //                                                     font    # chars
  // label         Options      option                   size    to erase  flag  col            row,           follow-up function
  { "Vol:",        NULL,        NULL,                     1,        3,      0,   IB_COL_1_X,    IB_ROW_1_Y,    &IBVolFollowup         }, // Tune Inc
  { "AGC",         agcOpts,     &AGCMode,                 1,        3,      1,   IB_COL_2L_X,   IB_ROW_1_Y,    NULL                   }, // Tune Inc
  { "Increment:",  tuneValues,  &tuneIndex,               0,        7,      0,   IB_COL_1_X,    IB_ROW_3_Y,    &IBTuneIncFollowup     }, // Tune Inc
  { "FT Inc:",     ftValues,    &ftIndex,                 0,        3,      0,   IB_COL_2_X,    IB_ROW_3_Y,    &IBTuneIncFollowup     }, // FT Inc
  { "Zoom:",       zoomOptions, (int*)&spectrumZoom,      0,        3,      0,   IB_COL_1_X,    IB_ROW_4_Y,    NULL                   }, // Zoom
  { "Decoder:",    onOff,       &decoderFlag,             0,        3,      1,   IB_COL_1_X,    IB_ROW_5_Y,    NULL                   }, // Decoder
  { "NF Set:",     onOff,       &liveNoiseFloorFlag,      0,        3,      1,   IB_COL_2_X,    IB_ROW_4_Y,    NULL                   }, // Noise Floor
  { "Temp:",       NULL,        NULL,                     0,        3,      1,   IB_COL_1_X,    IB_ROW_7_Y,    &IBTempFollowup        }, // Teensy Temp
  { "Load:",       NULL,        NULL,                     0,        4,      1,   IB_COL_2_X,    IB_ROW_7_Y,    &IBLoadFollowup        },  // Teensy Load
  { "FT8       ",  ft8Opts,     &ft8State,                0,       10,      2,   IB_COL_1_X,    IB_ROW_8_Y,    &IBFT8Followup         },  // FT8 sync
  { "Stack:",      NULL,        NULL,                     0,        4,      2,   IB_COL_1_X,    IB_ROW_6_Y,    &IBStackFollowup       },  // Stack
  { "Heap:",       NULL,        NULL,                     0,        4,      2,   IB_COL_2_X,    IB_ROW_6_Y,    &IBHeapFollowup        },  // Heap
  //{ "AutoNotch:",  onOff,       (int*)&ANR_notchOn,       0,        3,      1,   IB_COL_1_X,    IB_ROW_5_Y,    NULL                   }, // Auto Notch
  //{ "Noise:",      filter,      &nrOptionSelect,          0,        8,      1,   IB_COL_1_X,    IB_ROW_6_Y,    NULL                   }, // Noise Filter
  //{ "Compress:",   onOff,       &compressorFlag,          0,        6,      1,   IB_COL_2_X,    IB_ROW_5_Y,    &IBCompressionFollowup }, // Compress
  //{ "Keyer:",      optionsWPM,  &EEPROMData.keyType,      0,       12,      0,   IB_COL_1_X,    IB_ROW_8_Y,    &IBWPMFollowup         }, // Keyer
  //{ "Equalizers:", NULL,        NULL,                     0,       10,      1,   IB_COL_1_X,    IB_ROW_10_Y,   &IBEQFollowup          }  // Equalizers

  //{ "Vol:",       NULL,        NULL,                      1,        2,      0,   IB_COL_1_X,    IB_ROW_1_Y,    &IBVolFollowup         }, // Tune Inc
  //{ "AGC",        agcOpts,     &AGCMode,                  1,        3,      1,   IB_COL_2L_X,   IB_ROW_1_Y,    NULL                   }, // Tune Inc
  //{ "Increment:", tuneValues,  &tuneIndex,                0,        7,      0,   IB_COL_1_X,    IB_ROW_3_Y,    NULL                   }, // Tune Inc
  //{ "FT Inc:",    ftValues,    &ftIndex,                  0,        3,      0,   IB_COL_2_X,    IB_ROW_3_Y,    NULL                   }, // FT Inc
  //{ "AutoNotch:", onOff,       (int*)&ANR_notchOn,        0,        3,      1,   IB_COL_1_X,    IB_ROW_5_Y,    NULL                   }, // Auto Notch
  //{ "Noise:",     filter,      &nrOptionSelect,           0,        8,      1,   IB_COL_1_X,    IB_ROW_4_Y,    NULL                   }, // Noise Filter
  //{ "Zoom:",      zoomOptions, (int*)&spectrumZoom,      0,        3,      0,   IB_COL_2_X,    IB_ROW_4_Y,    NULL                   }, // Zoom
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
void UpdateInfoBoxItem(uint8_t item) {
  int label_x;
//  int xOffset = infoBox[item].col == 1 ? IB_COL_1_X : IB_COL_2_X;
//  int yOffset = IB_ROW_1_Y + (infoBox[item].row - 1) * 20;
  int xOffset = infoBox[item].col;
  int yOffset = infoBox[item].row;

  if(item >= IB_NUM_ITEMS) return;

  //if(item == IB_ITEM_TUNE) Serial.println(tuneIndex);

  tft.setFontScale((enum RA8875tsize)infoBox[item].fontSize);
  tft.fillRect(xOffset, yOffset, tft.getFontWidth() * infoBox[item].clearWidth, tft.getFontHeight(), RA8875_BLACK);
  tft.setTextColor(RA8875_WHITE);
  label_x = xOffset - 5 - strlen(infoBox[item].label) * tft.getFontWidth();
  tft.setCursor(label_x, yOffset);
  tft.print(infoBox[item].label);

  if(infoBox[item].Options != NULL) {
    if((infoBox[item].highlightFlag > 0) && (*infoBox[item].option == 0)) {
      tft.setTextColor(RA8875_WHITE);
    } else if((infoBox[item].highlightFlag == 2) && (*infoBox[item].option == 1)) {
      tft.setTextColor(RA8875_RED);
    } else {
      tft.setTextColor(RA8875_GREEN);
    }

    tft.setCursor(xOffset, yOffset);
    tft.print(infoBox[item].Options[*infoBox[item].option]);
  }

  if(infoBox[item].followFnPtr != NULL) {
    infoBox[item].followFnPtr(infoBox[item].row, infoBox[item].col);
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
  //UpdateInfoBoxItem(IB_ITEM_VOL);
  //UpdateInfoBoxItem(IB_ITEM_AGC);
  //UpdateInfoBoxItem(IB_ITEM_TUNE);
  //UpdateInfoBoxItem(IB_ITEM_FINE);
  //UpdateInfoBoxItem(IB_ITEM_COMPRESS);
  //UpdateInfoBoxItem(IB_ITEM_DECODER);
  //UpdateInfoBoxItem(IB_ITEM_FILTER);
  //UpdateInfoBoxItem(IB_ITEM_FLOOR);
  //UpdateInfoBoxItem(IB_ITEM_NOTCH);
  //UpdateInfoBoxItem(IB_ITEM_KEY);
  //UpdateInfoBoxItem(IB_ITEM_ZOOM);

  // ... or update them in order
  for(int i = 0; i < IB_NUM_ITEMS; i++) {
    UpdateInfoBoxItem(i);
  }
}

/*****
  Purpose: Information box follow up function for the Compression item

  Parameter list:
    void

  Return value:
    void
*****/
void IBTuneIncFollowup(int row, int col) {
  SetFtActive(!mouseCenterTuneActive);
}

/*****
  Purpose: Information box follow up function for the Compression item
           Assumes this is only called as part of updating Compression item

  Parameter list:
    void

  Return value:
    void
*****/
void IBCompressionFollowup(int row, int col) {
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
void IBWPMFollowup(int row, int col) {
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
void IBVolFollowup(int row, int col) {
  tft.setFontScale((enum RA8875tsize)1);
  tft.setTextColor(RA8875_GREEN);
  tft.setCursor(col, row);
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
void IBEQFollowup(int row, int col) {
  tft.setCursor(col, row);
  if (receiveEQFlag) {
    tft.setTextColor(RA8875_RED);
    tft.print("Rx");
    tft.setTextColor(RA8875_GREEN);
    tft.setCursor(col + 25, row);
    tft.print("On");
  } else {
    tft.setTextColor(RA8875_RED);
    tft.print("Rx");
    tft.setCursor(col + 25, row);
    tft.setTextColor(RA8875_WHITE);
    tft.print("Off");
  }
  tft.setCursor(col + 55, row);
  if (xmitEQFlag) {
    tft.setTextColor(RA8875_RED);
    tft.print("Tx");
    tft.setTextColor(RA8875_GREEN);
    tft.setCursor(col + 80, row);
    tft.print("On");
  } else {
    tft.setTextColor(RA8875_RED);
    tft.print("Tx");
    tft.setTextColor(RA8875_WHITE);
    tft.setCursor(col + 80, row);
    tft.print("Off");
  }
}

/*****
  Purpose: Information box follow up function for the Temp item

  Parameter list:
    void

  Return value:
    void
*****/
void IBTempFollowup(int row, int col) {
  char buff[10];

  //if (elapsed_micros_idx_t > (SampleRate / 960))
  {
    tft.setFontScale((enum RA8875tsize)0);
    tft.setTextColor(RA8875_GREEN);
    MyDrawFloatP(TGetTemp(), 0, col, row, buff, 2);
    tft.drawCircle(col + 22, row + 5, 3, RA8875_GREEN);
  }
}

/*****
  Purpose: Information box follow up function for the Load item

  Parameter list:
    void

  Return value:
    void
*****/
void IBLoadFollowup(int row, int col) {
  char buff[10];
  int valueColor = RA8875_GREEN;
  double block_time;
  double processor_load;
  double elapsed_micros_mean;

  //if (elapsed_micros_idx_t > (SampleRate / 960))
  {
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
    MyDrawFloatP(processor_load, 0, col, row, buff, 2);
    tft.print("%");
    elapsed_micros_idx_t = 0;
    elapsed_micros_sum = 0;
    //elapsed_micros_mean = 0;
  }
}

/*****
  Purpose: Information box follow up function for the FT8 item

  Parameter list:
    void

  Return value:
    void
*****/
void IBFT8Followup(int row, int col) {
  if(bands[currentBand].mode == DEMOD_FT8 || bands[currentBand].mode == DEMOD_FT8_WAV) {
    tft.setTextColor(WHITE);
    tft.setCursor(INFO_BOX_L + 5, row + 20);

    //                  1         2         3
    //         1234567890123456789012345678901
    //         10:48 1   943    0  xxxx
    tft.print("PST   I  Freq  SNR  Dist");

    if(ft8MsgSelectActive) {
      tft.setTextColor(RA8875_GREEN);
    } else {
      tft.setTextColor(YELLOW);
    }

    // give details of active message if any
    if(num_decoded_msg > 0) {
      DisplayActiveMessageDetails(row + 40 - 2, INFO_BOX_L + 5);
    }
  }
}

// *** TODO: eliminate hard coded column/row references in next two ***
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
  tft.fillRect(IB_COL_1_X + 37, IB_ROW_4_Y, tft.getFontWidth() * 10, tft.getFontHeight(), RA8875_BLACK);
  tft.setCursor(IB_COL_1_X + 38, IB_ROW_4_Y);
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
    tft.fillRect(IB_COL_2_X - 20, IB_ROW_4_Y, 15, 15, RA8875_GREEN);
  }
  else if (combinedCoeff < 50)
  {
    CWLevelTimer = millis();
    if (CWLevelTimer - CWLevelTimerOld > 2000)
    {
      CWLevelTimerOld = millis();
      tft.fillRect(IB_COL_2_X - 20, IB_ROW_4_Y, 17, 17, RA8875_BLACK);
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

/*****
  Purpose: Information box follow up function for the Stack item
            The stack value is more informative when called from within a function that might be stressing the stack

  Parameter list:
    void

  Return value:
    void
*****/
void IBStackFollowup(int row, int col) {
  // note: these values are defined by the linker, they are not valid memory
  // locations in all cases - by defining them as arrays, the C++ compiler
  // will use the address of these definitions - it's a big hack, but there's
  // really no clean way to get at linker-defined symbols from the .ld file

  extern char _ebss[];

  auto sp = (char*) __builtin_frame_address(0);

  auto stack = (sp - _ebss) >> 10;

  //Serial.print("Stack: ");
  //Serial.println((int)(sp - _ebss));
  //Serial.println("");

  tft.setFontScale((enum RA8875tsize)0);
  tft.setTextColor(RA8875_GREEN);
  tft.setCursor(col, row);
  tft.print(stack);
  tft.print("k");
}


/*****
  Purpose: Information box follow up function for the Heap item
           Note: mallinfo() must be primed by fully allocating the
           heap at startup.  See PrimeMallInfo() in Utility.cpp.

  Parameter list:
    void

  Return value:
    void
*****/
void IBHeapFollowup(int row, int col) {
  // note: these values are defined by the linker, they are not valid memory
  // locations in all cases - by defining them as arrays, the C++ compiler
  // will use the address of these definitions - it's a big hack, but there's
  // really no clean way to get at linker-defined symbols from the .ld file

  //extern char _heap_end[], *__brkval; // this is only useful at startup

  //struct mallinfo mi = mallinfo();

  //Serial.println(mi.arena);
  //Serial.println(mi.ordblks);
  //Serial.println(mi.smblks);
  //Serial.println(mi.hblks);
  //Serial.println(mi.hblkhd);
  //Serial.println(mi.usmblks);
  //Serial.println(mi.fsmblks);
  //Serial.println(mi.uordblks);
  //Serial.println(mi.fordblks);
  //Serial.println(mi.keepcost);

  size_t heap = mallinfo().fordblks;

  //Serial.println(mallinfo().fordblks);
  //Serial.println(heap);

  heap = mallinfo().fordblks >> 10;

  tft.setFontScale((enum RA8875tsize)0);
  tft.setTextColor(RA8875_GREEN);
  tft.setCursor(col, row);
  tft.print(heap);
  tft.print("k");
}

void SetFtActive(int flag) {
  if(flag == 1) {
    mouseCenterTuneActive = false;
    HighlightIBItem(IB_ITEM_FINE, RA8875_GREEN);
    HighlightIBItem(IB_ITEM_TUNE, RA8875_WHITE);
  } else {
    mouseCenterTuneActive = true;
    HighlightIBItem(IB_ITEM_TUNE, RA8875_GREEN);
    HighlightIBItem(IB_ITEM_FINE, RA8875_WHITE);
  }
}

// mouse actions
void MouseButtonInfoBox(int button, int x, int y) {
  // *** TODO: this is weak ***
  int item, itemX, itemY, itemSize, itemChars, itemW, itemH;

  //Serial.println(x);
  //Serial.println(y);
  //Serial.println(itemX);
  //Serial.println(itemY);
  //Serial.println(itemSize);
  //Serial.println(itemChars);
  //Serial.println(itemW);
  //Serial.println(itemH);


  // *** TODO: rework this after we add full capability
  for(int i = 0; i < 4; i++) {
    switch(i) {
      case 0:
        item = IB_ITEM_TUNE;
        break;
      case 1:
        item = IB_ITEM_FINE;
        break;
      case 2:
        item = IB_ITEM_ZOOM;
        break;
      case 3:
        item = IB_ITEM_FLOOR;
        break;
    }

    itemX = infoBox[item].col;
    itemY = infoBox[item].row;
    itemSize = infoBox[item].fontSize;
    itemChars = infoBox[item].clearWidth;
    itemW = (itemSize == 1 ? 16 : 8) * itemChars;
    itemH = itemSize == 1 ? 32 : 16;

    // allow action within a portion of label as well
    if(x > itemX - 50 && x < itemX + itemW && y > itemY && y < itemY + itemH) {
      switch(item) {
        case IB_ITEM_TUNE:
          if(button == 1) {
            SetFtActive(0);
          }
          break;

        case IB_ITEM_FINE:
          if(button == 1) {
            SetFtActive(1);
          }
          break;

        case IB_ITEM_ZOOM:
          if(button == 1) {
            SetZoom(++spectrumZoom);
          } else {
            SetZoom(--spectrumZoom);
          }
          break;

        case IB_ITEM_FLOOR:
          ToggleLiveNoiseFloorFlag();
          break;

        default:
          break;
      }
    }
  }
}

void MouseWheelInfoBox(int wheel, int x, int y) {
  // *** TODO: this is weak ***
  int item, itemX, itemY, itemSize, itemChars, itemW, itemH;

  //Serial.println(x);
  //Serial.println(y);
  //Serial.println(itemX);
  //Serial.println(itemY);
  //Serial.println(itemSize);
  //Serial.println(itemChars);
  //Serial.println(itemW);
  //Serial.println(itemH);

  for(int i = 0; i < 4; i++) {
    switch(i) {
      case 0:
        item = IB_ITEM_VOL;
        break;
      case 1:
        item = IB_ITEM_TUNE;
        break;
      case 2:
        item = IB_ITEM_FINE;
        break;
      case 3:
        item = IB_ITEM_ZOOM;
        break;
    }

    itemX = infoBox[item].col;
    itemY = infoBox[item].row;
    itemSize = infoBox[item].fontSize;
    itemChars = infoBox[item].clearWidth;
    itemW = (itemSize == 1 ? 16 : 8) * itemChars;
    itemH = itemSize == 1 ? 32 : 16;

    // allow action within a portion of label as well
    if(x > itemX - 50 && x < itemX + itemW && y > itemY && y < itemY + itemH) {
      switch(item) {
        case IB_ITEM_VOL:
          audioVolume += wheel;

          if (audioVolume > MAX_AUDIO_VOLUME) {
            audioVolume = MAX_AUDIO_VOLUME;
          } else {
            if (audioVolume < MIN_AUDIO_VOLUME)
              audioVolume = MIN_AUDIO_VOLUME;
          }

          volumeChangeFlag = true;  // Need this because of unknown timing in display updating.
          break;

          case IB_ITEM_TUNE:
          ChangeFreqIncrement(wheel);
          if(mouseCenterTuneActive) {
            HighlightIBItem(IB_ITEM_TUNE, RA8875_GREEN);
          }
          break;

        case IB_ITEM_FINE:
          ChangeFtIncrement(wheel);
          if(!mouseCenterTuneActive) {
            HighlightIBItem(IB_ITEM_FINE, RA8875_GREEN);
          }
          break;

      case IB_ITEM_ZOOM:
          if(wheel == 1) {
            SetZoom(++spectrumZoom);
          } else {
            SetZoom(--spectrumZoom);
          }
          break;

        default:
          break;
      }
    }
  }
}

void HighlightIBItem(uint8_t item, int color) {
  int label_x;
  int xOffset = infoBox[item].col;
  int yOffset = infoBox[item].row;

  if(item >= IB_NUM_ITEMS) return;

  tft.setFontScale((enum RA8875tsize)infoBox[item].fontSize);
  //tft.fillRect(xOffset, yOffset, tft.getFontWidth() * infoBox[item].clearWidth, tft.getFontHeight(), RA8875_BLACK);
  tft.setTextColor(color);
  label_x = xOffset - 5 - strlen(infoBox[item].label) * tft.getFontWidth();
  tft.setCursor(label_x, yOffset);
  tft.print(infoBox[item].label);
}
