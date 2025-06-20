
#include "SDT.h"

#include "AudioConfig.h"
#include "CWProcessing.h"
#include "CW_Excite.h"
#include "Display.h"
#include "InfoBox.h"
#include "keyboard.h"
#include "keyer.h"
#include "pi.h"
#include "Tune.h"
#include "Utility.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

float cwRampUp[128], cwRampDown[128];
unsigned long cwDelayTimer;       // used to keep transmitter keyed after CW transmission
elapsedMillis cwAtomTimer = 0;    // used for CW signal timing, automatically increases as time passes
unsigned long transmitDitLength;

char keyerMessages[MAX_MESSAGES][MAX_MESSAGE_LENGTH + 1] = {
  "CQ CQ CQ DE KN6ZDE",
  "RIG T41 1W QRP",
  "ANT EFHW SLOPE to 20 FT",
  "73",
  "599",
};

bool keyerMessagesActive = true;
bool keyerMessageEditMode = false;
int keyerEditIndex;

char letterTable[] = {                 // Morse coding: dit = 0, dah = 1
  0b101,              // A                first 1 is the sentinel marker
  0b11000,            // B
  0b11010,            // C
  0b1100,             // D
  0b10,               // E
  0b10010,            // F
  0b1110,             // G
  0b10000,            // H
  0b100,              // I
  0b10111,            // J
  0b1101,             // K
  0b10100,            // L
  0b111,              // M
  0b110,              // N
  0b1111,             // O
  0b10110,            // P
  0b11101,            // Q
  0b1010,             // R
  0b1000,             // S
  0b11,               // T
  0b1001,             // U
  0b10001,            // V
  0b1011,             // W
  0b11001,            // X
  0b11011,            // Y
  0b11100             // Z
};

char numberTable[] = {
  0b111111,           // 0
  0b101111,           // 1
  0b100111,           // 2
  0b100011,           // 3
  0b100001,           // 4
  0b100000,           // 5
  0b110000,           // 6
  0b111000,           // 7
  0b111100,           // 8
  0b111110            // 9
};

char punctuationTable[] = {
  0b01101011,         // exclamation mark 33
  0b01010010,         // double quote 34
  0b10001001,         // dollar sign 36
  0b00101000,         // ampersand 38
  0b01011110,         // apostrophe 39
  0b01011110,         // parentheses (L) 40, 41
  0b01110011,         // comma 44
  0b00100001,         // hyphen 45
  0b01010101,         // period  46
  0b00110010,         // slash 47
  0b01111000,         // colon 58
  0b01101010,         // semi-colon 59
  0b01001100,         // question mark 63
  0b01001101,         // underline 95
  0b01101000,         // paragraph
};
int ASCIIForPunctuation[] = {33, 34, 36, 39, 41, 44, 45, 46, 47, 58, 59, 63, 95};  // Indexes into code

int keyerState = 0; // 0 - off, 1 - on
int selectedMsg = 0;
DMAMEM uint8_t msgBuffer[50];
int msgIndexIn = 0;

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

/*****
  Purpose: establish the dit length for code transmission. Crucial since
    all spacing is done using dit length

  Parameter list:
    int wpm

  Return value:
    void
*****/
FLASHMEM void SetTransmitDitLength() {
  transmitDitLength = 1200 / currentWPM;
}

// *** TODO: consider more refined shaping with https://www.ivarc.org.uk/uploads/1/2/3/8/12380834/keyclicks_version_1.pdf ***
void IncreaseGain(float cwPwr) {
  CW_ExciterIQData();
  for(int i = 0; i < 2000; i++) {
    //float fac = (float)i / 2000.0;
    //modeSelectOutExL.gain(0, cwPwr * fac);
    //modeSelectOutExR.gain(0, cwPwr * fac);
    delayMicroseconds(5);
  }
}

void DecreaseGain(float cwPwr) {
  CW_ExciterIQData();
  for(int i = 0; i < 100; i++) {
    //float fac = (100.0 - i) / 100.0;
    //modeSelectOutExL.gain(0, cwPwr * fac);
    //modeSelectOutExR.gain(0, cwPwr * fac);
    delayMicroseconds(100);
  }
}

/*****
  Purpose: create a morse code dit

  Paramter list:
  void

  Return value:
  void
*****/
void Dit() {
  CreateCWSignal(transmitDitLength);
}

/*****
  Purpose: create a morse code dah

  Paramter list:
  void

  Return value:
  void
*****/
void Dah() {
  CreateCWSignal(3UL * transmitDitLength);
}

void CWPause(unsigned long ms) {
  cwAtomTimer = 0; // reset CW signal timing
  while(cwAtomTimer < ms) {
    ;
  }
}

/*****
  Purpose: create spacing within letters

  Parameter list:
  void

  Return value:
  void
*****/
void IntraSpace() {
  CWPause(transmitDitLength);
}

/*****
  Purpose: create spacing between letters

  Parameter list:
  void

  Return value:
  void
*****/
void LetterSpace() {
  CWPause(3UL * transmitDitLength);
}

/*****
  Purpose: create spacing between words

  Parameter list:
  void

  Return value:
  void
*****/
void WordSpace() {
  CWPause(7UL * transmitDitLength);
}

/*****
  Purpose: send a Morse code character

  Parameter list:
    char code       the code for the letter to send

  Return value:
    void
*****/
void SendCode(char code) {
  int i;

  // Find the sentinel. Loop looks for first 1 which marks the start of the letter:   0b11000 = 'B'
  for(i = 7; i >= 0; i--) {
    if(code & (1 << i)) break;
  }

  // Now look at rest of binary value: 0b1000 = B after reading sentinel
  for(i--; i >= 0; i--) {
    cwDelayTimer = millis();
    if(code & (1 << i)) {
      // send a dah
      Dah();
    } else {
      // send a dit
      Dit();
    }

    if(i == 0) {
      // pause for space between letters
      CWPause(3UL * transmitDitLength);
    } else {
      // pause for space within letter
      CWPause(transmitDitLength - 7UL);
    }
  }
}

/*****
  Purpose: send a Morse code character

  Parameter list:
  char chr       character to be sent

  Return value:
  void
*****/
void Send(char chr) {
  if(isalpha(chr)) {
    if(islower(chr)) {
      chr = toupper(chr);
    }
    SendCode(letterTable[chr - 'A']);  // Make into a zero-based array index
    return;
  } else if(isdigit(chr)) {
    SendCode(numberTable[chr - '0']);  // Same deal here...
    return;
  }

  switch(chr) {  // Non-alpha and non-digit characters
    case '\r':
    case '\n':
    case '!':
      SendCode(0b01101011);  // exclamation mark 33
      break;
    case '"':
      SendCode(0b01010010);  // double quote 34
      break;
    case '$':
      SendCode(0b10001001);  // dollar sign 36
      break;
    case '@':
      SendCode(0b00101000);  // ampersand 38
      break;
    case '\'':
      SendCode(0b01011110);  // apostrophe 39
      break;

    case '(':
    case ')':
      SendCode(0b01011110);  // parentheses (L) 40, 41
      break;

    case ',':
      SendCode(0b01110011);  // comma 44
      break;

    case '.':
      SendCode(0b01010101);  // period  46
      break;
    case '-':
      SendCode(0b00100001);  // hyphen 45
      break;
    case ':':
      SendCode(0b01111000);  // colon 58
      break;
    case ';':
      SendCode(0b01101010);  // semi-colon 59
      break;
    case '?':
      SendCode(0b01001100);  // question mark 63
      break;
    case '_':
      SendCode(0b01001101);  // underline 95
      break;

    case (char)182:
      SendCode(0b01101000);  // paragraph #182, '¶'
      break;

    case ' ':  // Space
    default:
      // pause for space between words
      // we've already paused inter-letter
      CWPause((7UL - 3UL) * transmitDitLength);
      break;
  }
}

/*****
  Purpose: send a message in CW

  Parameter list:
    char *msg         message to send

  Return value:
    void
*****/
void SendMessage(char *msg) {
  // configure radio for CW transmission
  radioState = CW_TRANSMIT_KEYER_STATE;
  ConfigAudioState();
  SetFreq();
  ShowTransmitReceiveStatus();

  digitalWrite(RXTX, HIGH);  // turn on xmit relay

  cwDelayTimer = millis();

  while(*msg != '\0') {
    Send(*msg++);
  }

  // continue CW exciter until we reach transmit delay
  while(millis() - cwDelayTimer <= cwTransmitDelay) {
    ;
  }

  digitalWrite(RXTX, LOW);

  lastState = -1;
}

/*****
  Purpose: send the selected message

  Paramter list:
    int index    index of message to send

  Return value:
    void
*****/
void SendMessage(int index) {
  SendMessage(&keyerMessages[index][0]);
}

/*****
  Purpose: checks if a message key has been pressed

  Parameter list:
    void

  Return value:
    int           the switch that was pressed or -1 if no switch
*****/
int ReadMessageKeys() {
  int key = -1;

  return key;
}

void KeyerSetup() {
  msgIndexIn = 0;
  UpdateInfoBoxItem(IB_ITEM_KEYER);

  // create raised cosine factors for 5 ms ramps
  // https://en.wikipedia.org/wiki/Raised-cosine_filter
  for(int i = 0; i < 128; i++){
    cwRampUp[i] = 0.5 * (1.0 + cos((1.0 + (float)i / 128.0) * PI));
    cwRampDown[i] = 0.5 * (1.0 + cos((float)i / 128.0 * PI));
  }
}

void KeyerLoop() {
  uint8_t chr;
  static char tmpBuffer[MAX_MESSAGE_LENGTH + 1];

  // Process keyboard input
  chr = getc();

  // process input
  if(keyerMessagesActive) {
    // process input relative to stored keyer messages and edit state
    if(keyerMessageEditMode) {
      // editing stored message
      switch(chr) {
        case 10:         // enter
          // leave edit mode after making changes
          keyerMessages[selectedMsg][keyerEditIndex] = 0; // terminate message
          keyerMessageEditMode = false;
          break;

        case 27:         // escape
          // leave edit mode without changes

          // restore stored message
          strcpy(keyerMessages[selectedMsg], tmpBuffer);
          keyerMessageEditMode = false;
          break;

        case 32:          // space
        case 48 ... 57:   // '0' to '9'
        case 65 ... 90:   // 'a' to 'z'
        case 97 ... 122:  // 'A' to 'Z'
          keyerMessages[selectedMsg][keyerEditIndex++] = chr;

          // treat as circular buffer for now
          if(keyerEditIndex >= MAX_MESSAGE_LENGTH) {
            keyerEditIndex = 0;
          }
          break;

        case 127:         // backspase
        case 212:         // delete
          keyerEditIndex--;
          keyerMessages[selectedMsg][keyerEditIndex] = 0; // erase character
          break;

        default:
          break;
      }
    } else {
      // stored message navigation mode
      switch(chr) {
        case 10:         // enter
          SendMessage(&keyerMessages[selectedMsg][0]);
          break;

        case 209:         // insert
          // enter edit mode
          // store message for possible restoration later
          //strcpy(&keyerMessages[selectedMsg][0], tmpBuffer);
          //Serial.println(keyerMessages[selectedMsg]);
          strcpy(tmpBuffer, keyerMessages[selectedMsg]);
          //Serial.println(tmpBuffer);
          keyerEditIndex = strlen(keyerMessages[selectedMsg]);
          //Serial.println(keyerEditIndex);
          keyerMessageEditMode = true;
          break;

        case 210:         // home
          selectedMsg = 0;
          break;

        case 213:         // end
          selectedMsg = MAX_MESSAGES - 1;
          break;

        case 215:         // right arrow
          selectedMsg++;
          if(selectedMsg >= MAX_MESSAGES) selectedMsg = 0;
          break;

        case 216:         // left arrow
          selectedMsg--;
          if(selectedMsg < 0) selectedMsg = MAX_MESSAGES - 1;
          break;

        case 217:         // up arrow
        case 218:         // down arrow
          keyerMessagesActive = !keyerMessagesActive;
          break;

        default:
          break;
      }
    }
  } else {
    // process input relative to stored keyer messages
    switch(chr) {
      case 10:         // enter
        msgBuffer[msgIndexIn] = 0;
        SendMessage((char *)msgBuffer);
        msgIndexIn = 0;
        break;

      case 27:         // escape
        msgIndexIn = 0;
        break;

      case 32:          // space
      case 48 ... 57:   // '0' to '9'
      case 65 ... 90:   // 'a' to 'z'
      case 97 ... 122:  // 'A' to 'Z'
        msgBuffer[msgIndexIn++] = chr;

        // treat as circular buffer for now
        if(msgIndexIn >= MAX_MESSAGE_LENGTH) {
          msgIndexIn = 0;
        }
        break;

      case 127:         // backspase
      case 212:         // delete
        msgIndexIn--;
        break;

      case 217:         // up arrow
      case 218:         // down arrow
        keyerMessagesActive = !keyerMessagesActive;
        break;

      default:
        break;
    }
  }
  UpdateInfoBoxItem(IB_ITEM_KEYER);
}
