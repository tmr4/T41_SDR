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
#include "Filter.h"
#include "ft8.h"
#include "InfoBox.h"
#include "Menu.h"
#include "MenuProc.h"
#include "mouse.h"
#include "Process2.h"
#include "Tune.h"
#include "Utility.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

#define MAX_WPM                  60

int calibrateFlag = -1;

//-------------------------------------------------------------------------------------------------------------
// Forwards
//-------------------------------------------------------------------------------------------------------------

void ProcessEqualizerChoices(int EQType, char *title);

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

/*****
  Purpose: Present the CW options available and return the selection

  Parameter list:
    void

  Return value
    void
*****/
void CWOptions() {
  // const char *cwChoices[] = { "WPM", "Key Type", "CW Filter", "Paddle Flip", "Sidetone Volume", "Transmit Delay", "Cancel" };  // AFP 10-18-22

  //Serial.println(secondaryMenuIndex);
  //Serial.println(menuBarSelected);
  switch (secondaryMenuIndex) {
    case 0:  // WPM
      //SetWPM();
      // GetMenuValue(minValue, maxValue, startValue, increment, prompt, valueOffset)
      GetMenuValue(5, MAX_WPM, &currentWPM, 1, "WPM:", 200, NULL, NULL, &SetWPMFollowup);
      break;

    case 1:          // Type of key:
      SetKeyType();  // Straight key or keyer? Stored in EEPROMData.keyType; no heap/stack variable
      SetKeyPowerUp();
      UpdateInfoBoxItem(IB_ITEM_KEY);
      break;

    case 2:              // CW Filter BW
      SelectCWFilter();  // in CWProcessing
      break;

    case 3:            // Flip paddles
      DoPaddleFlip();  // Stored in EEPROM; variables paddleDit and paddleDah
      break;

    case 4:  // Sidetone volume
      //SetSideToneVolume();
      // GetMenuValue(minValue, maxValue, startValue, increment, prompt, valueOffset)
      GetMenuValue(0, 100, &sidetoneVolume, 1, "Volume:", 200, &SetSideToneVolumeSetup, &SetSideToneVolumeValue, &SetSideToneVolumeFollowup);
  break;

    case 5:                // Transmit relay hold delay
      //SetTransmitDelay();
      GetMenuValue(0, 10000, &cwTransmitDelay, 250, "Delay:", 150, NULL, NULL, &SetTransmitDelayFollowup);
      break;

    default:  // Cancel
      break;
  }
}

// *** TODO: T41EEE does this for each band ***
void RFPowerFollowup() {
  if (xmtMode == CW_MODE) {                                                                                                                                      //AFP 10-13-22
    powerOutCW[currentBand] = (-.0133 * transmitPowerLevel * transmitPowerLevel + .7884 * transmitPowerLevel + 4.5146) * CWPowerCalibrationFactor[currentBand];  //  afp 10-21-22

    EEPROMData.powerOutCW[currentBand] = powerOutCW[currentBand];
  } else {
    if (xmtMode == SSB_MODE) {
      powerOutSSB[currentBand] = (-.0133 * transmitPowerLevel * transmitPowerLevel + .7884 * transmitPowerLevel + 4.5146) * SSBPowerCalibrationFactor[currentBand];  // afp 10-21-22
      EEPROMData.powerOutSSB[currentBand] = powerOutSSB[currentBand];                                                                                                //AFP 10-21-22
    }
  }
  EEPROMData.transmitPowerLevel = transmitPowerLevel;
  EEPROMWrite();
  ShowCurrentPowerSetting();
}

void RFGainFollowup() {
  EEPROMData.rfGainAllBands = rfGainAllBands;
  EEPROMWrite();
}

/*****
  Purpose: Process RF options

  Parameter list:
    void

  Return value
    void
*****/
void RFOptions() {
  //  const char *rfOptions[] = { "Power level", "Gain", "Cancel" };
  switch (secondaryMenuIndex) {
    case 0: // Power Level
      //transmitPowerLevel = (float)GetEncoderValue(1, 20, transmitPowerLevel, 1, (char *)"Power: ");
      GetMenuValue(1, 20, &transmitPowerLevel, 1, "Power:", 200, NULL, NULL, &RFPowerFollowup);
      break;

    case 1: // Gain
      //rfGainAllBands = GetEncoderValue(-60, 10, rfGainAllBands, 5, (char *)"RF Gain dB: ");
      GetMenuValue(-60, 10, &rfGainAllBands, 5, "Gain:", 200, NULL, NULL, &RFGainFollowup);
      break;
  }
}

/*****
  Purpose: Used to change the currently active VFO

  Parameter list:
    void

  Return value
    void
*****/
void VFOSelect(int32_t index) {
  if(xmtMode == DATA_MODE) {
    // restore old demodulation mode before we change bands
    bands[currentBand].mode = priorDemodMode;
  }

  splitVFO = false;
  NCOFreq = 0L;

  switch (index) {
    case VFO_A:
      centerFreq = TxRxFreq = currentFreqA;
      activeVFO = VFO_A;
      currentBand = currentBandA;
      //tft.fillRect(FILTER_PARAMETERS_X + 180, FILTER_PARAMETERS_Y, 150, 20, RA8875_BLACK);  // Erase split message
      break;

    case VFO_B:
      centerFreq = TxRxFreq = currentFreqB;
      activeVFO = VFO_B;
      currentBand = currentBandB;
      //tft.fillRect(FILTER_PARAMETERS_X + 180, FILTER_PARAMETERS_Y, 150, 20, RA8875_BLACK);  // Erase split message
      break;

    case VFO_SPLIT:
      DoSplitVFO();
      break;

    default:  // Cancel
      return;
      break;
  }

  if(xmtMode == DATA_MODE) {
    priorDemodMode = bands[currentBand].mode; // save demod mode for restoration later

    switch (currentDataMode) {
      case DEMOD_PSK31_WAV:
      case DEMOD_PSK31:
        bands[currentBand].mode = DEMOD_PSK31;
        break;

      case DEMOD_FT8:
      case DEMOD_FT8_WAV:
        bands[currentBand].mode = DEMOD_FT8;
        syncFlag = false; 
        ft8State = 1;
        UpdateInfoBoxItem(IB_ITEM_FT8);
        break;
    }
  }

  bands[currentBand].freq = TxRxFreq;
  SetBand();                            // SetBand updates the display
  SetBandRelay(HIGH);                   // Required when switching VFOs

  EEPROMData.activeVFO = activeVFO;
  EEPROMWrite();

  if (xmtMode == CW_MODE) {
    UpdateCWFilter();
  }
}

void VFOSelect() {
  VFOSelect(secondaryMenuIndex);
}

/*****
  Purpose: Allow user to set current EEPROM values or restore default settings

  Parameter list:
    void

  Return value
    void
*****/
void EEPROMOptions() {
  //  const char *EEPROMOpts[] = { "Save Current", "Set Defaults", "Get Favorite", "Set Favorite",
  //                               "Copy EEPROM-->SD", "Copy SD-->EEPROM", "SD EEPROM Dump", "Cancel" };
  switch (secondaryMenuIndex) {   
    case 0:  // Save current values
      EEPROMWrite();
      break;

    case 1:
      EEPROMSaveDefaults2();  // Restore defaults
      break;

    case 2:
      GetFavoriteFrequency();  // Get a stored frequency and store in active VFO
      break;

    case 3:
      SetFavoriteFrequency();  // Set favorites
      break;

    case 4:
      CopyEEPROMToSD();  // Save current EEPROM value to SD
      break;

    case 5:
      CopySDToEEPROM();  // Copy from SD to EEPROM
      EEPROMRead();
      tft.writeTo(L2);   // This is specifically to clear the bandwidth indicator bar.  KF5N August 7, 2023
      tft.clearMemory();
      tft.writeTo(L1);
      RedrawDisplayScreen();  // Assume there are lots of changes and do a heavy-duty refresh.  KF5N August 7, 2023
      break;

    case 6:
      SDEEPROMDump();  // Show SD data
      break;

    default:
      break;
  }
}

/*****
  Purpose: Present the bands available and return the selection

  Parameter list:
    void

  Return value
    void
*****/
void AGCOptions() {
  // const char *AGCChoices[] = { "Off", "Long", "Slow", "Medium", "Fast", "Cancel" }; // G0ORX (Added Long) September 5, 2023

  AGCMode = secondaryMenuIndex;
  AGCLoadValues();

  EEPROMData.AGCMode = AGCMode; // Store in EEPROM and...
  EEPROMWrite();  // ...save it
  UpdateInfoBoxItem(IB_ITEM_AGC);
}

/*****
  Purpose: Show the list of scales for the spectrum divisions

  Parameter list:
    void

  Return value
    void
*****/
void SpectrumOptions() {
  const char *spectrumChoices[] = { "20 dB/unit", "10 dB/unit", "5 dB/unit", "2 dB/unit", "1 dB/unit", "Cancel" };
  int spectrumSet = EEPROMData.currentScale;

  spectrumSet = secondaryMenuIndex;
  if (strcmp(spectrumChoices[spectrumSet], "Cancel") == 0) {
    return;
  }
  currentScale = spectrumSet;  // Yep...
  EEPROMData.currentScale = currentScale;
  EEPROMWrite();
  ShowSpectrumdBScale();
}

/*****
  Purpose: Receive EQ set

  Parameter list:
    void

  Return value
    void
*****/
void EqualizerRecOptions() {
  //  const char *RecEQChoices[] = { "On", "Off", "EQSet", "Cancel" };  // Add code practice oscillator
 switch (secondaryMenuIndex) {
     case 0:
      receiveEQFlag = ON;
      break;
    case 1:
      receiveEQFlag = OFF;
      break;
    case 2:
      for (int iFreq = 0; iFreq < EQUALIZER_CELL_COUNT; iFreq++) {
      }
      ProcessEqualizerChoices(0, (char *)"Receive Equalizer");
      EEPROMWrite();
      RedrawDisplayScreen();
      break;
    case 3:
      break;
  }
}

/*****
  Purpose: Xmit EQ options

  Parameter list:
    void

  Return value
    void
*****/
void EqualizerXmtOptions() {
  //  const char *XmtEQChoices[] = { "On", "Off", "EQSet", "Cancel" };  // Add code practice oscillator
 switch (secondaryMenuIndex) {
    case 0:
      xmitEQFlag = ON;
      break;
    case 1:
      xmitEQFlag = OFF;
      break;
    case 2:
      ProcessEqualizerChoices(1, (char *)"Transmit Equalizer");
      EEPROMWrite();
      RedrawDisplayScreen();
      break;
    case 3:
      break;
  }
}

void MicGainFollowup() {
  EEPROMData.currentMicGain = currentMicGain;
  EEPROMWrite();
}

/*****
  Purpose: Set mic gain level

  Parameter list:
    void

  Return value
    void
*****/
void MicGainSet() {
  //  const char *micGainChoices[] = { "Set Mic Gain", "Cancel" };
  switch (secondaryMenuIndex) {
    case 0:
      // GetMenuValue(minValue, maxValue, startValue, increment, prompt, valueOffset)
      GetMenuValue(-40, 30, &currentMicGain, 1, "Gain:", 200, NULL, NULL, &MicGainFollowup);
      break;

    case 1:
      break;
  }
}

void SetCompressionLevelFollowup() {
  EEPROMData.currentMicThreshold = currentMicThreshold;
  EEPROMWrite();
  UpdateInfoBoxItem(IB_ITEM_COMPRESS);
}

/*
void SetCompressionRatioFollowup() {
  //currentMicCompRatio += ((float) menuEncoderMove * .1);

  EEPROMData.currentMicCompRatio = currentMicCompRatio;
  EEPROMWrite();
}

void SetCompressionAttackFollowup() {
  //currentMicAttack += ((float) menuEncoderMove * 0.1);
  //else if (currentMicAttack < .1)
  //  currentMicAttack = .1;

  EEPROMData.currentMicAttack = currentMicAttack;
  EEPROMWrite();
}

void SetCompressionReleaseFollowup() {
  //currentMicRelease += ((float) menuEncoderMove * 0.1);
  //else if (currentMicRelease < 0.1)                 // 100% max
  //  currentMicRelease = 0.1;

  EEPROMData.currentMicCompRatio = currentMicCompRatio;
  EEPROMWrite();
}
*/

/*****
  Purpose: Turn mic compression on and set the level

  Parameter list:
    void

  Return value
    void
*****/
void MicOptions() {
  //  const char *micChoices[] = { "On", "Off", "Set Threshold", "Set Comp_Ratio", "Set Attack", "Set Decay", "Cancel" };
  switch (secondaryMenuIndex) {
    case 0:                // On
      compressorFlag = 1;
      UpdateInfoBoxItem(IB_ITEM_COMPRESS);
      break;

    case 1:  // Off
      compressorFlag = 0;
      UpdateInfoBoxItem(IB_ITEM_COMPRESS);
      break;

    case 2:
      //SetCompressionLevel();
      GetMenuValue(-60, 00, &currentMicThreshold, 1, "Compression:", 200, NULL, NULL, &SetCompressionLevelFollowup);
      break;

    /* *** TODO: these aren't used currently ***
    case 3:
      //SetCompressionRatio();
      // *** TODO: FIX: original had increment at 0.1 ***
      //GetMenuValue(1, 10, &currentMicCompRatio, 1, "Ratio:", 200, NULL, NULL, &SetCompressionRatioFollowup);
      break;

    case 4:
      //SetCompressionAttack();
      // *** TODO: FIX: original had min and increment at 0.1 ***
      //GetMenuValue(1, 10, &currentMicAttack, 1, "Ratio:", 200, NULL, NULL, &SetCompressionAttackFollowup);
      break;

    case 5:
      //SetCompressionRelease();
      // *** TODO: FIX: original had increment at 0.1 ***
      //GetMenuValue(1, 10, &currentMicRelease, 1, "Ratio:", 200, NULL, NULL, &SetCompressionReleaseFollowup);
      break;

    case 6:
      break;
    */
    default:  // Cancelled choice
      break;
  }
  secondaryMenuIndex = -1;
}

/*****
  Purpose: Present the Calibrate options available and return the selection

  Parameter list:
    void

  Return value
   void
*****/
void CalibrateOptions() {
  static long long freqCorrectionFactorOld = freqCorrectionFactor;
  int val;
  int32_t increment = 100L;

  tft.fillRect(SECONDARY_MENU_X, MENUS_Y, EACH_MENU_WIDTH + 30, CHAR_HEIGHT, RA8875_BLACK);

  if(calibrateFlag < 0) {
    calibrateFlag = secondaryMenuIndex;
  }

  switch (calibrateFlag) {
    case 0:  // Calibrate Frequency  - uses WWV
      freqCorrectionFactor = GetEncoderValueLive(-200000, 200000, freqCorrectionFactor, increment, (char *)"Freq Cal: ");
      if (freqCorrectionFactor != freqCorrectionFactorOld) {
        si5351.init(SI5351_CRYSTAL_LOAD_10PF, Si_5351_crystal, freqCorrectionFactor);
        si5351.drive_strength(SI5351_CLK1, SI5351_DRIVE_8MA);  // KF5N July 10 2023
        si5351.drive_strength(SI5351_CLK2, SI5351_DRIVE_8MA);  // KF5N July 10 2023
        SetFreq();
        delay(10L);
        freqCorrectionFactorOld = freqCorrectionFactor;
      }
      val = ReadSelectedPushButton();
      if (val != BOGUS_PIN_READ) {        // Any button press??
        val = ProcessButtonPress(val);    // Use ladder value to get menu choice
        if (val == MENU_OPTION_SELECT) {  // Yep. Make a choice??
          tft.fillRect(SECONDARY_MENU_X, MENUS_Y, EACH_MENU_WIDTH + 35, CHAR_HEIGHT, RA8875_BLACK);
          EEPROMWrite();
          calibrateFlag = 5;
        }
      }
      break;

    case 1:  // CW PA Cal
      if (keyPressedOn == 1 && xmtMode == CW_MODE) {
        //================  CW Transmit Mode Straight Key ===========
        if (digitalRead(KEYER_DIT_INPUT_TIP) == LOW && keyType == 0) {  //Straight Key
          powerOutCW[currentBand] = (-.0133 * transmitPowerLevel * transmitPowerLevel + .7884 * transmitPowerLevel + 4.5146) * CWPowerCalibrationFactor[currentBand];
          CW_ExciterIQData();
          xrState = TRANSMIT_STATE;
          ShowTransmitReceiveStatus();
          SetFreq();                 //  AFP 10-02-22
          digitalWrite(MUTE, HIGH);  //   Mute Audio  (HIGH=Mute)
          modeSelectInR.gain(0, 0);
          modeSelectInL.gain(0, 0);
          modeSelectInExR.gain(0, 0);
          modeSelectOutL.gain(0, 0);
          modeSelectOutR.gain(0, 0);
          modeSelectOutExL.gain(0, 0);
          modeSelectOutExR.gain(0, 0);
        }
      }
      CWPowerCalibrationFactor[currentBand] = GetEncoderValueLive(-2.0, 2.0, CWPowerCalibrationFactor[currentBand], 0.001, (char *)"CW PA Cal: ");
      powerOutCW[currentBand] = (-.0133 * transmitPowerLevel * transmitPowerLevel + .7884 * transmitPowerLevel + 4.5146) * CWPowerCalibrationFactor[currentBand];  // AFP 10-21-22
      val = ReadSelectedPushButton();
      if (val != BOGUS_PIN_READ) {        // Any button press??
        val = ProcessButtonPress(val);    // Use ladder value to get menu choice
        if (val == MENU_OPTION_SELECT) {  // Yep. Make a choice??
          tft.fillRect(SECONDARY_MENU_X, MENUS_Y, EACH_MENU_WIDTH + 35, CHAR_HEIGHT, RA8875_BLACK);
          EEPROMData.CWPowerCalibrationFactor[currentBand] = CWPowerCalibrationFactor[currentBand];
          EEPROMWrite();
          calibrateFlag = 5;
        }
      }
      break;

    case 2:                  // IQ Receive Cal - Gain and Phase
      DoReceiveCalibrate();  // This function was significantly revised
      calibrateFlag = 5;
      break;

    case 3:               // IQ Transmit Cal - Gain and Phase
      DoXmitCalibrate();  // This function was significantly revised
      calibrateFlag = 5;
      break;

    case 4:  // SSB PA Cal
      SSBPowerCalibrationFactor[currentBand] = GetEncoderValueLive(-2.0, 2.0, SSBPowerCalibrationFactor[currentBand], 0.001, (char *)"SSB PA Cal: ");
      powerOutSSB[currentBand] = (-.0133 * transmitPowerLevel * transmitPowerLevel + .7884 * transmitPowerLevel + 4.5146) * SSBPowerCalibrationFactor[currentBand];  // AFP 10-21-22
      val = ReadSelectedPushButton();
      if (val != BOGUS_PIN_READ) {        // Any button press??
        val = ProcessButtonPress(val);    // Use ladder value to get menu choice
        if (val == MENU_OPTION_SELECT) {  // Yep. Make a choice??
          tft.fillRect(SECONDARY_MENU_X, MENUS_Y, EACH_MENU_WIDTH + 35, CHAR_HEIGHT, RA8875_BLACK);
          EEPROMWrite();
          calibrateFlag = 5;
        }
      }
      break;

    case 5: // wrap up calibration
      //EraseMenus();
      //RedrawDisplayScreen();
      TxRxFreq = centerFreq + NCOFreq;
      //DrawBandwidthBar();
      //ShowFrequency();
      //ShowOperatingStats();
      calibrateFlag = -1;
      modeSelectOutExL.gain(0, 0);
      modeSelectOutExR.gain(0, 0);
      break;

    default:  // Cancelled choice
      break;
  }
}

/*****
  Purpose: To process the graphics for the 14 chan equalizar otpion

  Parameter list:
    int array[]         the peoper array to fill in
    char *title             the equalizer being set
  Return value
    void
*****/
void ProcessEqualizerChoices(int EQType, char *title) {
  for (int i = 0; i < EQUALIZER_CELL_COUNT; i++) {
  }
  const char *eqFreq[] = { " 200", " 250", " 315", " 400", " 500", " 630", " 800",
                           "1000", "1250", "1600", "2000", "2500", "3150", "4000" };
  int yLevel[EQUALIZER_CELL_COUNT];

  int columnIndex;
  int iFreq;
  int newValue;
  int xOrigin = 50;
  int xOffset;
  int yOrigin = 50;
  int wide = 700;
  int high = 300;
  int barWidth = 46;
  int barTopY;
  int barBottomY;
  int val;

  for (iFreq = 0; iFreq < EQUALIZER_CELL_COUNT; iFreq++) {
    if (EQType == 0) {
      yLevel[iFreq] = EEPROMData.equalizerRec[iFreq];
    } else {
      if (EQType == 1) {
        yLevel[iFreq] = EEPROMData.equalizerXmt[iFreq];
      }
    }
  }
  tft.writeTo(L2);
  tft.clearMemory();
  tft.writeTo(L1);
  tft.fillWindow(RA8875_BLACK);

  tft.fillRect(xOrigin - 50, yOrigin - 25, wide + 50, high + 50, RA8875_BLACK);  // Clear data area
  tft.setTextColor(RA8875_GREEN);
  tft.setFontScale((enum RA8875tsize)1);
  tft.setCursor(200, 0);
  tft.print(title);

  tft.drawRect(xOrigin - 4, yOrigin, wide + 4, high, RA8875_BLUE);
  tft.drawFastHLine(xOrigin - 4, yOrigin + (high / 2), wide + 4, RA8875_RED);  // Print center zero line center
  tft.setFontScale((enum RA8875tsize)0);

  tft.setTextColor(RA8875_WHITE);
  tft.setCursor(xOrigin - 4 - tft.getFontWidth() * 3, yOrigin + tft.getFontHeight());
  tft.print("+12");
  tft.setCursor(xOrigin - 4 - tft.getFontWidth() * 3, yOrigin + (high / 2) - tft.getFontHeight());
  tft.print(" 0");
  tft.setCursor(xOrigin - 4 - tft.getFontWidth() * 3, yOrigin + high - tft.getFontHeight() * 2);
  tft.print("-12");

  barTopY = yOrigin + (high / 2);                // 50 + (300 / 2) = 200
  barBottomY = barTopY + DEFAULT_EQUALIZER_BAR;  // Default 200 + 100

  for (iFreq = 0; iFreq < EQUALIZER_CELL_COUNT; iFreq++) {
    tft.fillRect(xOrigin + (barWidth + 4) * iFreq, barTopY - (yLevel[iFreq] - DEFAULT_EQUALIZER_BAR), barWidth, yLevel[iFreq], RA8875_CYAN);
    tft.setCursor(xOrigin + (barWidth + 4) * iFreq, yOrigin + high - tft.getFontHeight() * 2);
    tft.print(eqFreq[iFreq]);
    tft.setCursor(xOrigin + (barWidth + 4) * iFreq + tft.getFontWidth() * 1.5, yOrigin + high + tft.getFontHeight() * 2);
    tft.print(yLevel[iFreq]);
  }

  columnIndex = 0;  // Get ready to set values for columns
  newValue = 0;
  while (columnIndex < EQUALIZER_CELL_COUNT) {
    xOffset = xOrigin + (barWidth + 4) * columnIndex;   // Just do the math once
    tft.fillRect(xOffset,                               // Indent to proper bar...
                 barBottomY - yLevel[columnIndex] - 1,  // Start at red line
                 barWidth,                              // Set bar width
                 newValue + 1,                          // Erase old bar
                 RA8875_BLACK);

    tft.fillRect(xOffset,                           // Indent to proper bar...
                 barBottomY - yLevel[columnIndex],  // Start at red line
                 barWidth,                          // Set bar width
                 yLevel[columnIndex],               // Draw new bar
                 RA8875_MAGENTA);
    while (true) {
      newValue = yLevel[columnIndex];  // Get current value
      if (menuEncoderMove != 0) {

        tft.fillRect(xOffset,                    // Indent to proper bar...
                     barBottomY - newValue - 1,  // Start at red line
                     barWidth,                   // Set bar width
                     newValue + 1,               // Erase old bar
                     RA8875_BLACK);
        newValue += (PIXELS_PER_EQUALIZER_DELTA * menuEncoderMove);  // Find new bar height. OK since menuEncoderMove equals 1 or -1
        tft.fillRect(xOffset,                                          // Indent to proper bar...
                     barBottomY - newValue,                            // Start at red line
                     barWidth,                                         // Set bar width
                     newValue,                                         // Draw new bar
                     RA8875_MAGENTA);
        yLevel[columnIndex] = newValue;

        tft.fillRect(xOffset + tft.getFontWidth() * 1.5 - 1, yOrigin + high + tft.getFontHeight() * 2,  // Update bottom number
                     barWidth, CHAR_HEIGHT, RA8875_BLACK);
        tft.setCursor(xOffset + tft.getFontWidth() * 1.5, yOrigin + high + tft.getFontHeight() * 2);
        tft.print(yLevel[columnIndex]);
        if (newValue < DEFAULT_EQUALIZER_BAR) {  // Repaint red center line if erased
          tft.drawFastHLine(xOrigin - 4, yOrigin + (high / 2), wide + 4, RA8875_RED);
          ;  // Clear hole in display center
        }
      }
      menuEncoderMove = 0;
      delay(200L);

      val = ReadSelectedPushButton();  // Read the ladder value

      if (val != -1 && val < (EEPROMData.switchValues[0] + WIGGLE_ROOM)) {
        val = ProcessButtonPress(val);  // Use ladder value to get menu choice
        delay(100L);

        tft.fillRect(xOffset,                // Indent to proper bar...
                     barBottomY - newValue,  // Start at red line
                     barWidth,               // Set bar width
                     newValue,               // Draw new bar
                     RA8875_GREEN);

        if (EQType == 0) {
          equalizerRec[columnIndex] = newValue;
          EEPROMData.equalizerRec[columnIndex] = equalizerRec[columnIndex];
        } else {
          if (EQType == 1) {
            equalizerXmt[columnIndex] = newValue;
            EEPROMData.equalizerXmt[columnIndex] = equalizerXmt[columnIndex];
          }
        }

        menuEncoderMove = 0;
        columnIndex++;
        break;
      }
    }
  }

  EEPROMWrite();
}
