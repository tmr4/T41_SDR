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
#include "Process2.h"
#include "Tune.h"
#include "Utility.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

int calibrateFlag = 0;
int IQChoice;
int micChoice;
int micGainChoice;

//-------------------------------------------------------------------------------------------------------------
// Forwards
//-------------------------------------------------------------------------------------------------------------

void DoPaddleFlip();

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

/*****
  Purpose: Present the Calibrate options available and return the selection

  Parameter list:
    void

  Return value
   void
*****/
int CalibrateOptions(int IQChoice) {
  static long long freqCorrectionFactorOld = freqCorrectionFactor;

  int val;
  int32_t increment = 100L;
  tft.fillRect(SECONDARY_MENU_X, MENUS_Y, EACH_MENU_WIDTH + 30, CHAR_HEIGHT, RA8875_BLACK);
  //  float transmitPowerLevelTemp;  //AFP 05-11-23
  switch (IQChoice) {

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
          calibrateFlag = 0;
          IQChoice = 5;
          return IQChoice;
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
          calibrateFlag = 0;
          IQChoice = 5;
          return IQChoice;
        }
      }
      break;
    case 2:                  // IQ Receive Cal - Gain and Phase
      DoReceiveCalibrate();  // This function was significantly revised
      IQChoice = 5;
      break;
    case 3:               // IQ Transmit Cal - Gain and Phase
      DoXmitCalibrate();  // This function was significantly revised
      IQChoice = 5;
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
          calibrateFlag = 0;
          IQChoice = 5;
          return IQChoice;
        }
      }
      break;

    case 5:
      //EraseMenus();
      //RedrawDisplayScreen();
      TxRxFreq = centerFreq + NCOFreq;
      //DrawBandwidthBar();
      //ShowFrequency();
      //ShowOperatingStats();
      calibrateFlag = 0;
      modeSelectOutExL.gain(0, 0);
      modeSelectOutExR.gain(0, 0);
      break;

    default:  // Cancelled choice
      micChoice = -1;
      break;
  }
  return 1;
}

/*****
  Purpose: Present the CW options available and return the selection

  Parameter list:
    void

  Return value
    void
*****/
void CWOptions() {
  // const char *cwChoices[] = { "WPM", "Key Type", "CW Filter", "Paddle Flip", "Sidetone Volume", "Transmit Delay", "Cancel" };  // AFP 10-18-22

  switch (secondaryMenuIndex) {
    case 0:  // WPM
      SetWPM();
      SetTransmitDitLength(currentWPM);
      break;

    case 1:          // Type of key:
      SetKeyType();  // Straight key or keyer? Stored in EEPROMData.keyType; no heap/stack variable
      SetKeyPowerUp();
      UpdateInfoBoxItem(&infoBox[IB_ITEM_KEY]);
      break;

    case 2:              // CW Filter BW
      SelectCWFilter();  // in CWProcessing
      break;

    case 3:            // Flip paddles
      DoPaddleFlip();  // Stored in EEPROM; variables paddleDit and paddleDah
      break;

    case 4:  // Sidetone volume
      SetSideToneVolume();
      break;

    case 5:                // new function
      SetTransmitDelay();  // Transmit relay hold delay
      break;

    default:  // Cancel
      break;
  }
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
  UpdateInfoBoxItem(&infoBox[IB_ITEM_AGC]);
}

/*****
  Purpose: IQ Options

  Parameter list:
    void

  Return value
    void
*****/
void IQOptions() {
  // const char *IQOptions[] = { "Freq Cal", "CW PA Cal", "Rec Cal", "Xmit Cal", "SSB PA Cal", "Cancel" };  //AFP 10-21-22
  // const char *IQOptions[] = {"Rec Cal", "Xmit Cal", "Freq Cal", "SSB PA Cal", "CW PA Cal", "Cancel"}; //AFP 10-21-22
  calibrateFlag = 1;
  IQChoice = secondaryMenuIndex;
  CalibrateOptions(IQChoice);
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

/*****
  Purpose: Set Mic level

  Parameter list:
    void

  Return value
    void
*****/
void MicGainSet() {
  //  const char *micGainChoices[] = { "Set Mic Gain", "Cancel" };
  switch (secondaryMenuIndex) {
    case 0:
      int val;
      currentMicGain = EEPROMData.currentMicGain;
      tft.setFontScale((enum RA8875tsize)1);
      tft.fillRect(SECONDARY_MENU_X - 50, MENUS_Y, EACH_MENU_WIDTH + 50, CHAR_HEIGHT, RA8875_MAGENTA);
      tft.setTextColor(RA8875_WHITE);
      tft.setCursor(SECONDARY_MENU_X - 48, MENUS_Y);
      tft.print("Mic Gain:");
      tft.setCursor(SECONDARY_MENU_X + 180, MENUS_Y);
      tft.print(currentMicGain);
      while (true) {
        if (menuEncoderMove != 0) {
          currentMicGain += ((float)menuEncoderMove);
          if (currentMicGain < -40)
            currentMicGain = -40;
          else if (currentMicGain > 30)  // 100% max
            currentMicGain = 30;
          tft.fillRect(SECONDARY_MENU_X + 180, MENUS_Y, 80, CHAR_HEIGHT, RA8875_MAGENTA);
          tft.setCursor(SECONDARY_MENU_X + 180, MENUS_Y);
          tft.print(currentMicGain);
          menuEncoderMove = 0;
        }
        val = ReadSelectedPushButton();  // Read pin that controls all switches
        val = ProcessButtonPress(val);
        //delay(150L);
        if (val == MENU_OPTION_SELECT) {  // Make a choice??
          EEPROMData.currentMicGain = currentMicGain;
          EEPROMWrite();
          break;
        }
      }
    case 1:
      break;
  }
}

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
      UpdateInfoBoxItem(&infoBox[IB_ITEM_COMPRESS]);
      break;
    case 1:  // Off
      compressorFlag = 0;
      UpdateInfoBoxItem(&infoBox[IB_ITEM_COMPRESS]);
      break;
    case 2:
      SetCompressionLevel();
      break;
    case 3:
      SetCompressionRatio();
      break;
    case 4:
      SetCompressionAttack();
      break;
    case 5:
      SetCompressionRelease();
      break;
    case 6:
      break;
    default:  // Cancelled choice
      micChoice = -1;
      break;
  }
  secondaryMenuIndex = -1;
}

/*****
  Purpose: Present the bands available and return the selection
            *** consider FT8 power options ***
  Parameter list:
    void

  Return value12
    void
*****/
void RFOptions() {
  //  const char *rfOptions[] = { "Power level", "Gain", "Cancel" };
  switch (secondaryMenuIndex) {
    case 0: // Power Level
      transmitPowerLevel = (float)GetEncoderValue(1, 20, transmitPowerLevel, 1, (char *)"Power: ");
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
      break;

    case 1: // Gain
      rfGainAllBands = GetEncoderValue(-60, 10, rfGainAllBands, 5, (char *)"RF Gain dB: ");  // Argument: min, max, start, increment
      EEPROMData.rfGainAllBands = rfGainAllBands;
      EEPROMWrite();
      break;
  }
}

/*****
  Purpose: This option reverses the dit and dah paddles on the keyer

  Parameter list:
    void

  Return value
    void
*****/
void DoPaddleFlip() {
  const char *paddleState[] = { "Right paddle = dah", "Right paddle = dit" };
  int choice, lastChoice;
  int pushButtonSwitchIndex;
  int valPin;

  paddleDah = KEYER_DAH_INPUT_RING;  // Defaults
  paddleDit = KEYER_DIT_INPUT_TIP;
  choice = lastChoice = 0;

  tft.setTextColor(RA8875_BLACK);
  tft.fillRect(SECONDARY_MENU_X - 100, MENUS_Y, EACH_MENU_WIDTH + 100, CHAR_HEIGHT, RA8875_GREEN);
  tft.setCursor(SECONDARY_MENU_X - 93, MENUS_Y);
  tft.print(paddleState[choice]);  // Show the default (right paddle = dah

  while (true) {
    delay(150L);
    valPin = ReadSelectedPushButton();                     // Poll buttons
    if (valPin != -1) {                                    // button was pushed
      pushButtonSwitchIndex = ProcessButtonPress(valPin);  // Winner, winner...chicken dinner!
      if (pushButtonSwitchIndex == MAIN_MENU_UP || pushButtonSwitchIndex == MAIN_MENU_DN) {
        choice = !choice;  // Reverse the last choice
        tft.fillRect(SECONDARY_MENU_X - 100, MENUS_Y, EACH_MENU_WIDTH + 100, CHAR_HEIGHT, RA8875_GREEN);
        tft.setCursor(SECONDARY_MENU_X - 93, MENUS_Y);
        tft.print(paddleState[choice]);
      }
      if (pushButtonSwitchIndex == MENU_OPTION_SELECT)
      {  // Made a choice??
        if (choice)
        {  // Means right-paddle dit
          paddleDit = KEYER_DAH_INPUT_RING;
          paddleDah = KEYER_DIT_INPUT_TIP;
          paddleFlip = 1; // KD0RC
        }
        else
        {
          paddleDit = KEYER_DIT_INPUT_TIP;
          paddleDah = KEYER_DAH_INPUT_RING;
          paddleFlip = 0;  // KD0RC
        }
        EEPROMData.paddleDit = paddleDit;
        EEPROMData.paddleDah = paddleDah;
        EraseMenus();
        UpdateInfoBoxItem(&infoBox[IB_ITEM_KEY]);
        return;
      }
    }
  }
}

/*****
  Purpose: Used to change the currently active VFO

  Parameter list:
    void

  Return value
    void
*****/
void VFOSelect() {
#ifdef FT8_SUPPORT
  if(xmtMode == DATA_MODE) {
    // restore old demodulation mode before we change bands
    bands[currentBand].mode = priorDemodMode;
  }
#endif

  //delay(10);
  NCOFreq = 0L;
  switch (secondaryMenuIndex) {
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

#ifdef FT8_SUPPORT
  if(xmtMode == DATA_MODE) {
    priorDemodMode = bands[currentBand].mode; // save demod mode for restoration later
    bands[currentBand].mode = DEMOD_FT8;
    syncFlag = false; 
    ft8State = 1;
    UpdateInfoBoxItem(&infoBox[IB_ITEM_FT8]);
  }
#endif

  bands[currentBand].freq = TxRxFreq;
  SetBand();                            // SetBand updates the display
  SetBandRelay(HIGH);                   // Required when switching VFOs

  EEPROMData.activeVFO = activeVFO;
  EEPROMWrite();

  if (xmtMode == CW_MODE) {
    UpdateCWFilter();
  }
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
  Purpose: To select an option from a submenu

  Parameter list:
    char *options[]           submenus
    int numberOfChoices       choices available
    int defaultState          the starting option

  Return value
    int           an index into the band array
*****/
int SubmenuSelect(const char *options[], int numberOfChoices, int defaultStart) {
  int refreshFlag = 0;
  int val;
  int encoderReturnValue;

  tft.setTextColor(RA8875_BLACK);
  encoderReturnValue = defaultStart;  // Start the options using this option

  tft.setFontScale((enum RA8875tsize)1);
  if (refreshFlag == 0) {
    tft.fillRect(SECONDARY_MENU_X, MENUS_Y, EACH_MENU_WIDTH, CHAR_HEIGHT, RA8875_GREEN);  // Show the option in the second field
    tft.setCursor(SECONDARY_MENU_X + 1, MENUS_Y);
    tft.print(options[encoderReturnValue]);  // Secondary Menu
    refreshFlag = 1;
  }
  delay(150L);

  while (true) {
    val = ReadSelectedPushButton();  // Read the ladder value
    delay(150L);
    if (val != -1 && val < (EEPROMData.switchValues[0] + WIGGLE_ROOM)) {
      val = ProcessButtonPress(val);  // Use ladder value to get menu choice
      if (val > -1) {                 // Valid choice?
        switch (val) {
          case MENU_OPTION_SELECT:  // They made a choice
            tft.setTextColor(RA8875_WHITE);
            EraseMenus();
            return encoderReturnValue;
            break;

          case MAIN_MENU_UP:
            encoderReturnValue++;
            if (encoderReturnValue >= numberOfChoices)
              encoderReturnValue = 0;
            break;

          case MAIN_MENU_DN:
            encoderReturnValue--;
            if (encoderReturnValue < 0)
              encoderReturnValue = numberOfChoices - 1;
            break;

          default:
            encoderReturnValue = -1;  // An error selection
            break;
        }
        if (encoderReturnValue != -1) {
          tft.fillRect(SECONDARY_MENU_X, MENUS_Y, EACH_MENU_WIDTH, CHAR_HEIGHT, RA8875_GREEN);  // Show the option in the second field
          tft.setTextColor(RA8875_BLACK);
          tft.setCursor(SECONDARY_MENU_X + 1, MENUS_Y);
          tft.print(options[encoderReturnValue]);
          delay(50L);
          refreshFlag = 0;
        }
      }
    }
  }
}
