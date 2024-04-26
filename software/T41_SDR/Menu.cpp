#include "SDT.h"
#include "Bearing.h"
#include "Button.h"
#include "ButtonProc.h"
#include "CWProcessing.h"
#include "Display.h"
#include "EEPROM.h"
#include "Encoders.h"
#include "Menu.h"
#include "MenuProc.h"
#include "mouse.h"
#include "Utility.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

int32_t mainMenuIndex;
int32_t secondaryMenuIndex;
int32_t subMenuMaxOptions;           // holds the number of submenu options

bool getMenuValueActive = false;
bool getMenuOptionActive = false;
bool getMenuSelected = false;
void (*ptrMenuLoop)() = NULL;
void (*ptrMenuFollowup)() = NULL;
int getMenuMin, getMenuMax, getMenuInc, getMenuOffset;
int *ptrMenuValueCurrent;

int8_t menuStatus = NO_MENUS_ACTIVE;

const char *topMenus[] = { "CW Options", "RF Set", "VFO Select",
                           "EEPROM", "AGC", "Spectrum Options", "Mic Gain", "Mic Comp",
                           "EQ Rec Set", "EQ Xmt Set", "Calibrate", "Bearing", "Cancel" };

void (*functionPtr[])() = { &CWOptions, &RFOptions, &VFOSelect,
                           &EEPROMOptions, &AGCOptions, &SpectrumOptions, &MicGainSet, &MicOptions,
                           &EqualizerRecOptions, &EqualizerXmtOptions, &CalibrateOptions, &BearingMaps, &Cancel };

const char *secondaryChoices[][8] = {
  /* CW Options */ { "WPM", "Key Type", "CW Filter", "Paddle Flip", "Sidetone Vol", "Xmit Delay", "Cancel" },
  /* RF Set */ { "Power level", "Gain", "Cancel" },
  /* VFO Select */ { "VFO A", "VFO B", "Split", "Cancel" },
  /* EEPROM */ { "Save Current", "Set Defaults", "Get Favorite", "Set Favorite", "EEPROM-->SD", "SD-->EEPROM", "SD Dump", "Cancel" },
  /* AGC */ { "Off", "Long", "Slow", "Medium", "Fast", "Cancel" },
  /* Spectrum Options */ { "20 dB/unit", "10 dB/unit", " 5 dB/unit", " 2 dB/unit", " 1 dB/unit", "Cancel" },
  /* Mic Gain */ { "Set Mic Gain", "Cancel" },
  /* Mic Comp */ //{ "On", "Off", "Set Threshold", "Set Ratio", "Set Attack", "Set Decay", "Cancel" },
  /* Mic Comp */ { "On", "Off", "Set Threshold", "Cancel" }, // only threshold is used currently
  /* EQ Rec Set */ { "On", "Off", "EQSet", "Cancel" },
  /* EQ Xmt Set */ { "On", "Off", "EQSet", "Cancel" },
  /* Calibrate */ { "Freq Cal", "CW PA Cal", "Rec Cal", "Xmit Cal", "SSB PA Cal", "Cancel" },
  /* Bearing */ { "Set Prefix", "Cancel" },
  /* Cancel */ { "" }
};
const int secondaryMenuCount[] {7, 3, 4, 8, 6, 6, 2, 7, 4, 4, 6, 2, 1};

const char *menuOptions[][6] = {
  /* keyChoice */ { "Straight Key", "Keyer", "Cancel" },
  /* CWFilter */  { "0.8kHz", "1.0kHz", "1.3kHz", "1.8kHz", "2.0kHz", " Off " },
  /* paddleState */ { "Right = dah", "Right = dit" }
};
const int menuOptionsCount[] {2, 6, 2};

int receiveEQFlag;
int xmitEQFlag;

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

void Cancel() {
}

/*****
  Purpose: Display top line menu according to set menu parameters

  Parameter list:
    char *menuItem          pointers to the menu
    int where               PRIMARY_MENU or SECONDARY_MENU

  Return value;
    void
*****/
FLASHMEM void ShowMenu(const char *menu[], int where) {
  tft.setFontScale( (enum RA8875tsize) 1);

  if (menuStatus == NO_MENUS_ACTIVE) {
    NoActiveMenu(); // display error message if no menu selected
    Serial.println("NAM #4");
  }

  switch (where) {
    case PRIMARY_MENU:
      tft.fillRect(PRIMARY_MENU_X, MENUS_Y, EACH_MENU_WIDTH, CHAR_HEIGHT, RA8875_BLUE);
      tft.setCursor(PRIMARY_MENU_X + 1, MENUS_Y);
      tft.setTextColor(RA8875_WHITE);
      tft.print(*menu);
      break;

    case SECONDARY_MENU:
      tft.fillRect(SECONDARY_MENU_X, MENUS_Y, EACH_MENU_WIDTH, CHAR_HEIGHT, RA8875_GREEN);
      tft.setCursor(SECONDARY_MENU_X + 1, MENUS_Y);
  //    tft.setTextColor(RA8875_WHITE);
      tft.setTextColor(RA8875_BLACK);
      tft.print(*menu);  // Secondary Menu
      break;

      default:
        break;
  }
}

/*****
  Purpose: To process a menu up or down

  Parameter list:
    void

  Return value:
    void
*****/
FLASHMEM void MenuBarChange(int change) {
  switch (menuStatus) {
    case PRIMARY_MENU_ACTIVE:
      mainMenuIndex += change;

      // limit index
      if (mainMenuIndex < 0) {
        mainMenuIndex = TOP_MENU_COUNT - 1;
      } else if (mainMenuIndex == TOP_MENU_COUNT) {
        mainMenuIndex = 0;
      }
      ShowMenu(&topMenus[mainMenuIndex], PRIMARY_MENU);
      break;

    case SECONDARY_MENU_ACTIVE:
      secondaryMenuIndex += change;

      // limit index
      if (secondaryMenuIndex < 0) {
        secondaryMenuIndex = subMenuMaxOptions - 1;
      } else if (secondaryMenuIndex == subMenuMaxOptions) {
        secondaryMenuIndex = 0;
      }
      ShowMenu(&secondaryChoices[mainMenuIndex][secondaryMenuIndex], SECONDARY_MENU);
      break;

    default:
      break;
  }
}

FLASHMEM void ShowMenuBar(int menu = 0, int change = 0) {
  if (menuStatus == NO_MENUS_ACTIVE) {
    menuStatus = PRIMARY_MENU_ACTIVE;
    mainMenuIndex = menu;
    ShowMenu(&topMenus[mainMenuIndex], PRIMARY_MENU);
  } else {
    if(change != 0) MenuBarChange(change);
  }
}

FLASHMEM void MenuBarSelect() {
  switch (menuStatus) {
    case NO_MENUS_ACTIVE:
      #ifdef DEBUG_SW
        //NoActiveMenu();
        Serial.print("NAM #0: val = ");
        Serial.println(val);
      #endif
      break;

    case PRIMARY_MENU_ACTIVE:
      if(mainMenuIndex == TOP_MENU_COUNT - 1) {
        menuStatus = NO_MENUS_ACTIVE;
        mainMenuIndex = 0;
        EraseMenus();
      } else {
        menuStatus = SECONDARY_MENU_ACTIVE;
        secondaryMenuIndex = 0;
        subMenuMaxOptions = secondaryMenuCount[mainMenuIndex];
        ShowMenu(&secondaryChoices[mainMenuIndex][secondaryMenuIndex], SECONDARY_MENU);
      }
      break;

    case SECONDARY_MENU_ACTIVE:
      if(secondaryMenuIndex == secondaryMenuCount[mainMenuIndex] - 1) {
        // cancel selected
        menuStatus = PRIMARY_MENU_ACTIVE;
        EraseSecondaryMenu();
      } else {
        functionPtr[mainMenuIndex]();

        // wrap up menu unless we're still getting a value
        if(!getMenuValueActive  && !getMenuOptionActive) {
          EraseMenus();
          menuStatus = NO_MENUS_ACTIVE;
        }
      }
      break;

    default:
      break;
  }
}

/*****
  Purpose: Get a value for a menu bar item using the encoder or mouse wheel

  Parameter list:
    int minValue                lowest value allowed
    int maxValue                largest value allowed
    int *currentValue           pointer to current value
    int increment               amount by which each increment changes the value
    char prompt[]               menu bar prompt
    void (*ptrSetup)()          pointer to function that will run at setup
    void (*ptrValue)()          pointer to function that will run at the beginning of each loop
    void (*ptrFollowup)()       pointer to function that will run after Select button is pressed or on mouse left click

  Return value;
    void
*****/
FLASHMEM void GetMenuValue(int minValue, int maxValue, int *currentValue, int increment, const char *prompt, int offset, void (*ptrSetup)(), void (*ptrValue)(), void (*ptrFollowup)()) {
  getMenuMin = minValue;
  getMenuMax = maxValue;
  getMenuInc = increment;
  ptrMenuValueCurrent = currentValue;
  getMenuOffset = offset;

  getEncoderValueFlag = true;

  tft.setFontScale((enum RA8875tsize)1);

  tft.fillRect(SECONDARY_MENU_X, MENUS_Y, EACH_MENU_WIDTH, CHAR_HEIGHT, RA8875_MAGENTA);
  tft.setTextColor(RA8875_WHITE);
  tft.setCursor(SECONDARY_MENU_X, MENUS_Y);
  tft.print(prompt);
  tft.setCursor(SECONDARY_MENU_X + getMenuOffset, MENUS_Y);
  tft.print(*currentValue);

  if(ptrSetup) ptrSetup();

  getMenuValueActive = true;
  getMenuSelected = false;
  ptrMenuLoop = ptrValue;
  ptrMenuFollowup = ptrFollowup;

  menuBarSelected = false;
}

// the changes made here are reflected immediately
// *** TODO: consider ability to cancel w/o change or making live update optional (that could replace GetEncoderValueLive as well ***
void GetMenuValueLoop() {
  int val = -1;
  int change = menuEncoderMove + mouseWheelValue;
  long oldValue = *ptrMenuValueCurrent;

  if(ptrMenuLoop) ptrMenuLoop();

  if (change != 0) {
    oldValue += change * getMenuInc;

    // limit value
    if (oldValue < getMenuMin) {
      oldValue = getMenuMin;
    } else if (oldValue > getMenuMax) {
      oldValue = getMenuMax;
    }

    *ptrMenuValueCurrent = oldValue;

    tft.setFontScale((enum RA8875tsize)1);

    // erase old value *** TODO: consider tft.getFontWidth() * value width in place of 50 below ***
    tft.fillRect(SECONDARY_MENU_X + getMenuOffset, MENUS_Y, 50, CHAR_HEIGHT, RA8875_MAGENTA);

    // update current value
    tft.setTextColor(RA8875_WHITE);
    tft.setCursor(SECONDARY_MENU_X + getMenuOffset, MENUS_Y);
    tft.print(oldValue);
  }

  // check if we're done
  if(menuBarSelected) {
    val = MENU_OPTION_SELECT;
  } else {
    val = ReadSelectedPushButton();  // Read pin that controls all switches
    if (val != -1 && val < (switchValues[0] + WIGGLE_ROOM)) {
      val = ProcessButtonPress(val);
    }
  }

  if (val == MENU_OPTION_SELECT) {
    getMenuSelected = true;
    getEncoderValueFlag = false;
  }

  menuEncoderMove = 0;
  mouseWheelValue = 0;
}

/*****
  Purpose: Select an option from a fixed bar submenu using the Menu Up/Down button, encoder or mouse wheel

  Parameter list:
    int menuIndex               submenu options index (menuOptions)
    int numberOfChoices         number of choices available
    int *ptrCurrentValue        pointer to current value or option index
    void (*ptrSetup)()          pointer to function that will run at setup
    void (*ptrValue)()          pointer to function that will run at the beginning of each loop
    void (*ptrFollowup)()       pointer to function that will run after Select button is pressed or on mouse left click

  Return value;
    void
*****/
FLASHMEM void GetMenuOption(int menuIndex, int *ptrCurrentValue, void (*ptrSetup)(), void (*ptrValue)(), void (*ptrFollowup)()) {
  getMenuOffset = menuIndex;
  ptrMenuValueCurrent = ptrCurrentValue;
  getMenuMin = 0;
  getMenuMax = menuOptionsCount[menuIndex];

  getEncoderValueFlag = true;

  tft.setFontScale((enum RA8875tsize)1);

  tft.fillRect(SECONDARY_MENU_X, MENUS_Y, EACH_MENU_WIDTH, CHAR_HEIGHT, RA8875_GREEN);
  //tft.setTextColor(RA8875_WHITE);
  tft.setTextColor(RA8875_BLACK);
  tft.setCursor(SECONDARY_MENU_X, MENUS_Y);
  tft.print(menuOptions[menuIndex][*ptrCurrentValue]);

  if(ptrSetup) ptrSetup();

  getMenuOptionActive = true;
  getMenuSelected = false;
  ptrMenuLoop = ptrValue;
  ptrMenuFollowup = ptrFollowup;

  menuBarSelected = false;
}

// the changes made here are reflected immediately
// *** TODO: consider ability to cancel w/o change or making live update optional (that could replace GetEncoderValueLive as well ***
void GetMenuOptionLoop() {
  int val = -1;
  int change = menuEncoderMove + mouseWheelValue;
  int currentValue = *ptrMenuValueCurrent;

  if(ptrMenuLoop) ptrMenuLoop();

  if (change == 0) {
    // see if a change was made through menu buttons
    val = ReadSelectedPushButton();  // Read pin that controls all switches
    if (val != -1 && val < (switchValues[0] + WIGGLE_ROOM)) {
      val = ProcessButtonPress(val);
      if (val > -1) {                 // Valid choice?
        switch (val) {
          case MENU_OPTION_SELECT:
            val = MENU_OPTION_SELECT;
            break;

          case MAIN_MENU_UP:
            change = 1;
            val = -1;
            break;

          case MAIN_MENU_DN:
            change = -1;
            val = -1;
            break;

          default:
            val = -1;
            break;
        }
      }
    }
  }

  if (change != 0) {
    currentValue += change;

    // roll value at ends
    if (currentValue < getMenuMin) {
      currentValue = getMenuMax - 1;
    } else if (currentValue >= getMenuMax) {
      currentValue = 0;
    }

    *ptrMenuValueCurrent = currentValue;

    tft.setFontScale((enum RA8875tsize)1);

    // erase old value *** TODO: consider tft.getFontWidth() * value width in place of 50 below ***
    tft.fillRect(SECONDARY_MENU_X, MENUS_Y, EACH_MENU_WIDTH, CHAR_HEIGHT, RA8875_GREEN);

    // update current value
    //tft.setTextColor(RA8875_WHITE);
    tft.setTextColor(RA8875_BLACK);
    tft.setCursor(SECONDARY_MENU_X, MENUS_Y);
    tft.print(menuOptions[getMenuOffset][currentValue]);
  }

  // check if an option was selected with the mouse
  if(menuBarSelected) {
    val = MENU_OPTION_SELECT;
  }

  if (val == MENU_OPTION_SELECT) {
    getMenuSelected = true;
    getEncoderValueFlag = false;
  }

  menuEncoderMove = 0;
  mouseWheelValue = 0;
}

/*****
  Purpose: To present the encoder-driven full menu display

  Argument List;
    void

  Return value: index number for the selected menu
*****/
/*
const char *secondaryFunctions[][8] = {
  { "WPM", "Key Type", "CW Filter", "Paddle Flip", "Sidetone Vol", "Xmit Delay", "Cancel" },
  { "Power level", "Gain", "Cancel" },
  { "VFO A", "VFO B", "Split", "Cancel" },
  { "Save Current", "Set Defaults", "Get Favorite", "Set Favorite", "EEPROM-->SD", "SD-->EEPROM", "SD Dump", "Cancel" },
  { "Off", "Long", "Slow", "Medium", "Fast", "Cancel" },
  { "20 dB/unit", "10 dB/unit", " 5 dB/unit", " 2 dB/unit", " 1 dB/unit", "Cancel" },
  { "Set floor", "Cancel" },
  { "Set Mic Gain", "Cancel" },
  { "On", "Off", "Set Threshold", "Set Ratio", "Set Attack", "Set Decay", "Cancel" },
  { "On", "Off", "EQSet", "Cancel" },
  { "On", "Off", "EQSet", "Cancel" },
  { "Freq Cal", "CW PA Cal", "Rec Cal", "Xmit Cal", "SSB PA Cal", "Cancel" },
  { "Set Prefix", "Cancel" }
};
*/
FLASHMEM int DrawMenuDisplay() {
  int i;
  menuStatus = 0;                                                       // No primary or secondary menu set
  mainMenuIndex = 0;
  secondaryMenuIndex = 0;

  tft.writeTo(L2);                                                      // Clear layer 2.  KF5N July 31, 2023
  tft.clearMemory();
  tft.writeTo(L1);
  tft.fillRect(1, SPECTRUM_TOP_Y + 1, 513, 379, RA8875_BLACK);          // Show Menu box
  tft.drawRect(1, SPECTRUM_TOP_Y + 1, 513, 378, RA8875_YELLOW);

  tft.setFontScale((enum RA8875tsize)1);
  tft.setTextColor(RA8875_WHITE);
  for (i = 0; i < TOP_MENU_COUNT; i++) {                                // Show primary menu list
    tft.setCursor(10, i * 25 + 115);
    tft.print(topMenus[i]);
  }
  tft.setTextColor(RA8875_GREEN);                                       // show currently active menu
  tft.setCursor(10, mainMenuIndex * 25 + 115);
  tft.print(topMenus[mainMenuIndex]);
  i = 0;
  tft.setTextColor(DARKGREY, RA8875_BLACK);                   
  while (strcmp(secondaryChoices[mainMenuIndex][i], "Cancel") != 0) {   // Show secondary choices
    tft.setCursor(300, i * 27 + 115);
    tft.print(secondaryChoices[mainMenuIndex][i]);
    i++;
  }
  tft.setCursor(300, i * 27 + 115);
  tft.print(secondaryChoices[mainMenuIndex][i]);

  return 0;
}

/*****
  Purpose: To select the primary menu on full menu

  Argument List;
    void

  Return value: index number for the selected primary menu 
*****/
FLASHMEM int SetPrimaryMenuIndex() {
  int i;
  int val;

  while (true) {

    if (menuEncoderMove != 0) {             // Did they move the encoder?
      tft.setTextColor(RA8875_WHITE);         // Yep. Repaint the old choice
      tft.setCursor(10, mainMenuIndex * 25 + 115);
      tft.print(topMenus[mainMenuIndex]);
      mainMenuIndex += menuEncoderMove;     // Change the menu index to the new value
      if (mainMenuIndex >= TOP_MENU_COUNT) {  // Did they go past the end of the primary menu list?
        mainMenuIndex = 0;                    // Yep. Set to start of the list.
      } else {
        if (mainMenuIndex < 0) {               // Did they go past the start of the list?
          mainMenuIndex = TOP_MENU_COUNT - 1;  // Yep. Set to end of the list.
        }
      }
      tft.setTextColor(RA8875_GREEN);
      tft.setCursor(10, mainMenuIndex * 25 + 115);
      tft.print(topMenus[mainMenuIndex]);
      tft.fillRect(299, SPECTRUM_TOP_Y + 5, 210, 279, RA8875_BLACK);         // Erase secondary menu list
      tft.setTextColor(DARKGREY);
      i = 0;
      for(int i = 0; i < secondaryMenuCount[mainMenuIndex]; i++) {   // Have we read the last entry in secondary menu?
        tft.setTextColor(DARKGREY);                                         // Nope.
        tft.setCursor(300, i * 25 + 115);
        tft.print(secondaryChoices[mainMenuIndex][i]);
      }
      tft.setCursor(300, i * 25 + 115);
      tft.print(secondaryChoices[mainMenuIndex][i]);
      menuEncoderMove = 0;
    }
    val = ReadSelectedPushButton();  // Read the ladder value
    delay(150L);
    if (val != -1 && val < (EEPROMData.switchValues[0] + WIGGLE_ROOM)) {      // Did they press Select?
      val = ProcessButtonPress(val);                                          // Use ladder value to get menu choice
      if (val > -1) {                                                         // Valid choice?

        if (val == MENU_OPTION_SELECT) {                                      // They made a choice
          tft.setTextColor(RA8875_WHITE);
          break;
        }
        delay(50L);
      }
    }

  }  // End while True
  tft.setTextColor(RA8875_WHITE);

  return mainMenuIndex;
}

/*****
  Purpose: To select the secondary menu on full menu

  Argument List;
    void

  Return value: index number for the selected primary menu 
*****/
FLASHMEM int SetSecondaryMenuIndex() {
  int i = 0;
  int secondaryMenuCount = 0;
  int oldIndex = 0;
  int val;

  while (true) {                                                        // How many secondary menu options?
    if (strcmp(secondaryChoices[mainMenuIndex][i], "Cancel") != 0) {    // Have we read the last entry in secondary menu?
      i++;                                                              // Nope.  
    } else {
      secondaryMenuCount = i + 1;                                       // Add 1 because index starts with 0
      break;
    }
  }
  secondaryMenuIndex = 0;                                   // Change the menu index to the new value
  menuEncoderMove  = 0;
  i = 0;

  tft.setTextColor(RA8875_GREEN);
  tft.setCursor(300, 115);
  tft.print(secondaryChoices[mainMenuIndex][0]);

  i = 0;
  while (true) {

    if (menuEncoderMove != 0) {  // Did they move the encoder?
      tft.setTextColor(DARKGREY);  // Yep. Repaint the old choice
      tft.setCursor(300, oldIndex * 25 + 115);
      tft.print(secondaryChoices[mainMenuIndex][oldIndex]);
      i += menuEncoderMove;  // Change the menu index to the new value
     
      if (i == secondaryMenuCount) {  // Did they go past the end of the primary menu list?
        i = 0;                        // Yep. Set to start of the list.
      } else {
        if (i < 0) {                  // Did they go past the start of the list?
          i = secondaryMenuCount - 1; // Yep. Set to end of the list.
        }
      }
      oldIndex = i;
      tft.setTextColor(RA8875_GREEN);
      tft.setCursor(300, i * 25 + 115);
      tft.print(secondaryChoices[mainMenuIndex][i]);
      menuEncoderMove = 0;
    }
    val = ReadSelectedPushButton();  // Read the ladder value
    delay(200L);
    if (val != -1 && val < (EEPROMData.switchValues[0] + WIGGLE_ROOM)) {
      val = ProcessButtonPress(val);  // Use ladder value to get menu choice
      if (val > -1) {                 // Valid choice?
        if (val == MENU_OPTION_SELECT) {  // They made a choice
          tft.setTextColor(RA8875_WHITE);
          secondaryMenuIndex = oldIndex;
          break;
        }
        delay(50L);
      }
    }
  }  // End while True

  return secondaryMenuIndex;
}
