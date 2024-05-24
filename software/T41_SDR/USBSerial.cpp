#include "SDT.h"

//#include <USBHost_t36.h>

#include "ButtonProc.h"
#include "Display.h"
#include "Encoders.h"
#include "EEPROM.h"
#include "keyboard.h"
#include "InfoBox.h"
#include "MenuProc.h"
#include "mouse.h"
#include "Tune.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

bool useKenwoodIF = false;
bool dataFlag = false;
uint8_t specData[518]; // xDyyy[up to 512 bytes of data];   x=A or F, yyy = 255 - max

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

void SerialSetup()
{
  //Serial.println("\n\nUSB Host Testing - Serial");
  //Serial1.begin(115200);
  //userial.begin(baud);
  SerialUSB1.begin(19200);
  //SerialUSB2.begin(19200);
}

void SendData(uint8_t *data, int len) {
  //int len = strlen(cmd);
  //int sizeBuf = SerialUSB1.availableForWrite();
  //Serial.print("Sending spectrum data, length: "); Serial.print(len); Serial.print(", buffer size: "); Serial.println(sizeBuf);
  //for(int i = 0; i < SPECTRUM_RES; i++) {
  //  Serial.write(data[i]); Serial.print(" "); Serial.println(data[i]);
  //}
  // *** TODO: work up alternative if USB buffer is sufficient ***
  if(SerialUSB1.availableForWrite() > len) {
    SerialUSB1.write(data, len);
    //SerialUSB1.write(data, SPECTRUM_RES);
    //SerialUSB1.send_now(); // we'll have a delay without this *** TODO: try with and without ***
  }
  //dataFlag = false;
}

void SendCmd(char *cmd) {
  //SerialUSB1.print(cmd);
  int sizeBuf = SerialUSB1.availableForWrite();
  if(cmd[0] != 0 && sizeBuf > 0) {
    // the size of Teensy 4.1 serial transmit buffer is 8k and is used in 4 2k parts.
    // I've seen about 6k available at this point.
    // (https://forum.pjrc.com/index.php?threads/usb-serial-on-teensy-4-0-buffer-size-limitation.67826/)
    int len = strlen(cmd);
    //Serial.println(sizeBuf);
    if(SerialUSB1.availableForWrite() > len) {
      SerialUSB1.write(cmd, len);
      SerialUSB1.send_now(); // we'll have a delay without this
    } else {
      int i=0;
      //Serial.println(sizeBuf);
      while(cmd[i] != 0) {
        if(SerialUSB1.availableForWrite() > 0) {
          //SerialUSB1.print(cmd[i++]);
        } else {
          SerialUSB1.flush(); // *** TODO: this will cause a freeze if PC stops receiving ***
          dataFlag = false;
        }
      }
    }
  } else {
    dataFlag = false;
  }
}

void GetCommand(char * cmd, int max) {
  int i = 0;

  while(SerialUSB1.available() > 0) {
    cmd[i] = (char) SerialUSB1.read();

    // there might be multiple commands in the serial buffer
    // read only the first one or up to the specified limit
    if(cmd[i] == ';' || i >= max) {
      break;
    }
    i++;
  }
  cmd[i+1] = 0; // *** TODO: this is currently needed by send command, revisit if that is changed ***
}

void SendSmeter(int16_t smeterPad, float32_t dbm) {
  char cmd[30];

  // we can send these separately or together
  // send dBm
  //sprintf(cmd, "SM0%+05d;", (int)(dbm * 10));
  //SendCmd(cmd);

  // send s-meter
  //sprintf(cmd, "SM20%04d;", smeterPad);
  //SendCmd(cmd);

  // send dBm and s-meter together
  // it's more efficient to send these together, though it's more work on the
  // receiving end.  We have to do that work anyway as the second message
  // more often than not arrives in the PC buffer prior to the first message
  // being read from the buffer.  Thus the two messages are in essence
  // combined.
  sprintf(cmd, "SM0%+05d;SM20%04d;", (int)(dbm * 10), smeterPad);
  SendCmd(cmd);
}

void SendAS() {
  char cmd[19];

  sprintf(cmd, "AS%011ld%d%d%d;",
    TxRxFreq,                       // freq in Hz (%011d) at index 2
    currentBand,                    // current band (%d) at index 13
    xmtMode,                        // transmission mode (%d) at index 14
    bands[currentBand].mode         // demodulation mode (%d)  at index 15
  );
  SendCmd(cmd);
}

void SendIF() {
  char cmd[50];

  // *** Warning: this is not the Kenwood implimentation ***
  sprintf(cmd, "IF%011ld%d%d%d%03d%+06ld%04d%d%d%d%d%d%d%d%ld%011d;",
    // active VFO Freq = TxRxFreq, centerFreq = TxRxFreq - NCOFreq
    //  *** TODO: we only need 8 digits for first field for T41, consider using other 3 for something ***
    TxRxFreq,                       // freq in Hz (%011d) at index 2
    currentBand,                    // current band (%d) at index 13
    xmtMode,                        // transmission mode (%d) at index 14
    bands[currentBand].mode,        // demodulation mode (%d)  at index 15
    audioVolume,                    // audio volume (%03d) at index 16
    NCOFreq,                        // NCO freq (%+06ld) at index 19
    currentNoiseFloor[currentBand], // noise floor (%04d) at index 25 *** TODO: verify need for +- or number of digits ***
    liveNoiseFloorFlag,             // set noise floor active/inactive 1/0 (%d) at index 29
    !xrState,                       // RX/TX (1/0) (%d) at index 30
    activeVFO,                      // VFO A/B (0/1) (%d) at index 31
    mouseCenterTuneActive ? 1 : 0,  // fine or center tune enabled (0/1) (%d) at index 32
    ftIndex,                        // fine tune index (%d) at index 33
    tuneIndex,                      // center tune index (%d) at index 34
    AGCMode,                        // AGC mode (%d) at index 35
    spectrumZoom,                   // spectrum zoom (%ld) at index 36
    activeVFO == 0 ? currentFreqB : currentFreqA // inactive VFO freq in Hz (%011d) at index 37
    //splitVFO ? 1 : 0,               // VFO split status (%d) at index xx
  );
  SendCmd(cmd);
}

// Kenwood modes
int GetMode() {
  // 1: LSB, 2: USB, 3: CW, 4: FM, 5: AM
  int mode;
  if (xmtMode == CW_MODE) {
    mode=3;
  } else {
    switch(bands[currentBand].mode) {
      case DEMOD_USB:
        mode=2; // USB
        break;
      case DEMOD_LSB:
        mode=1; // LSB
        break;
      case DEMOD_AM:
      case DEMOD_SAM:
        mode=5; // AM
        break;
      case DEMOD_NFM:
        mode=4; // FM
        break;
      default:
        mode=1; // LSB
        break;
    }
  }
  return mode;
}

void SerialLoop()
{
  if(SerialUSB1.available()) {
    char cmd[256];
    int mode = GetMode();

    GetCommand(cmd, 256);
    //Serial.print("Received ");  Serial.println(cmd);
    //int sizeBuf = SerialUSB1.availableForWrite();
    //Serial.println(sizeBuf);
    switch(cmd[0]) {
      case 'B':
        if(cmd[1] == 'U' && cmd[2] == ';') {
          // band up
          BandChange(1);
          SendAS();
        } else if(cmd[1] == 'D' && cmd[2] == ';') {
          // band up
          BandChange(-1);
          SendAS();
        }
        return; // *** TODO: or we can set cmd[0] to null
        break;

      case 'D':
        if(cmd[1] == 'S' && cmd[2] == ';') {
          // start sending spectrum data
          dataFlag = true;
        } else if(cmd[1] == 'P' && cmd[2] == ';') {
          // stop sending spectrum data
          dataFlag = false;
        }
        return; // *** TODO: or we can set cmd[0] to null
        break;

      case 'F':
        long f;
        switch(cmd[1]) {
          case 'A':
            if(cmd[13] == ';') {
              // set VFO A frequency
              f = atol(&cmd[2]);
              if(mouseCenterTuneActive) {
                SetCenterTune(f - centerFreq);
                currentFreqA = f;
              } else {
                SetFineTune(f - currentFreqA);
              }
              //Serial.print("Set VFO A to "); Serial.println(f);
              return;
            } else if(cmd[2] == ';') {
              // read VFO A frequency
              sprintf(cmd,"FA%011d;",currentFreqA);
            }
            break;

          case 'B':
            if(cmd[13] == ';') {
              // set VFO B frequency
              f = atol(&cmd[2]);
              if(mouseCenterTuneActive) {
                SetCenterTune(f - centerFreq);
                currentFreqB = f;
              } else {
                SetFineTune(f - currentFreqB);
              }
             // Serial.print("Set VFO B to "); Serial.println(f);
              return;
            } else if(cmd[2] == ';') {
              // read VFO B frequency
              sprintf(cmd,"FB%011d;",currentFreqB);
            }
            break;

          case 'C':
            if(cmd[13] == ';') {
              // set center frequency
              f = atol(&cmd[2]);
              centerFreq = f;
              NCOFreq = 0L;
              SetTxRxFreq(f);
              DrawBandwidthBar();
              //Serial.print("Center freq set to "); Serial.println(f);
              return;
            } else if(cmd[2] == ';') {
              // read center frequency
              sprintf(cmd,"FC%011ld;",centerFreq);
            }
            break;

          case 'I':
            if(cmd[4] == ';') {
              // freq or fine tune increment change
              if(cmd[2] == '0') {
                ChangeFreqIncrement(atol(&cmd[3]) - tuneIndex);
              } else if(cmd[2] == '1') {
                ChangeFtIncrement(atol(&cmd[3]) - ftIndex);
              }
            }
            return;
            break;

          case 'S':
            if(cmd[3] == ';') {
              // fine tune on or off
              SetFtActive(atoi(&cmd[2]));
              return;
            }
            break;

          case 'T':
            if(cmd[3] == ';') {
              // select VFO
              VFOSelect(atoi(&cmd[2]));
              SendAS();
              return;
            }
            break;

          default:
            cmd[0] = '?';
            cmd[1] = ';';
            cmd[2] = 0;
            break;
        }
        break;

      case 'G':
        if(cmd[1] == 'T' && cmd[3] == ';') {
          // update AGC
          AGCMode = atol(&cmd[2]);
          UpdateInfoBoxItem(IB_ITEM_AGC);
        }
        return;
        break;

      case 'I':
        if(cmd[1] == 'F' && cmd[2] == ';') {
          // retrieves transceiver status
          if(useKenwoodIF) {
            // *** TODO: not set up, just for testing ***
            sprintf(cmd, "IF%011ld%04d%+06d%d%d%d%02d%d%d%d%d%d%d%02d%d;",
              TxRxFreq,     // freq in Hz
              0,            // freq step size
              0,            // RIT/XIT freq in Hz, +-99999, this isn't preserved in the T41 but would be VFO A - VFO B if split
              0,            // RIT on/off
              0,            // XIT on/off
              0,0,          // channel bank number
              !xrState,     // RX/TX (1/0)
              mode,         // operating mode
              activeVFO,    // RX VFO
              0,            // scan Status
              0,            // split status (Kenwood manual refers to SP command which doesn't exist)
              0,            // CTCSS enabled
              0,            // CTCSS tone frequency
              0             // shift status
            );
          } else {
            SendIF();
            return;
          }
        }
        break;

      case 'M':
        if(cmd[1] == 'D' && cmd[2] == ';') {
          // send demod mode
          sprintf(cmd,"MD%d;", useKenwoodIF ? mode : bands[currentBand].mode);
        } else if(cmd[1] == 'D' && cmd[3] == ';') {
          // set demod mode status
          ChangeDemodMode(atoi(&cmd[2]));
          SendAS();
          return;
        } if(cmd[1] == 'E' && cmd[3] == ';') {
          // set operating mode
          ChangeMode(atoi(&cmd[2]));
          SendAS();
          return;
        }
        break;

      case 'N':
        if(cmd[1] == 'F' && cmd[2] == ';') {
          // send noise floor
          sprintf(cmd,"NF%04d;", currentNoiseFloor[currentBand]);
        } else if(cmd[1] == 'F' && cmd[6] == ';') {
          // set noise floor
          currentNoiseFloor[currentBand] = atoi(&cmd[2]);
          return;
        } else if(cmd[1] == 'G' && cmd[3] == ';') {
          // *** TODO: consider just toggling this through call to
          liveNoiseFloorFlag = atoi(&cmd[2]);

          // save final noise floor setting if toggling flag off
          if(liveNoiseFloorFlag == 0) {
            EEPROMData.currentNoiseFloor[currentBand]  = currentNoiseFloor[currentBand];
            EEPROMWrite();
          }
          UpdateInfoBoxItem(IB_ITEM_FLOOR);
          return;
        }
        break;

      case 'P': // PCxxx;
        if(cmd[1] == 'C' && cmd[5] == ';') {
          // set transmitter power level
          transmitPowerLevel = atoi(&cmd[2]);
          ShowCurrentPowerSetting();
        }
        break;

      case 'T':
        if(cmd[1] == 'M' && cmd[13] == ';') {
          // set Teensy RTC
          //Serial.println(atol(&cmd[2]));
          //Serial.println(Teensy3Clock.get());
          Teensy3Clock.set(atol(&cmd[2]));
          setTime(atol(&cmd[2]));
        }
        return;
        break;

      case 'V': // VOxxx;
        if(cmd[1] == 'O' && cmd[5] == ';') {
          // set transmitter power level
          audioVolume = atoi(&cmd[2]);
          volumeChangeFlag = true;
        }
        break;

      default:
        cmd[0] = '?';
        cmd[1] = ';';
        cmd[2] = 0;
        break;
    }

    SendCmd(cmd);
    //Serial.print("Responded with: "); Serial.println(cmd);
  }
}

//bool CompareStrings(const char *sz1, const char *sz2) {
//  while (*sz2 != 0) {
//    if (toupper(*sz1) != toupper(*sz2))
//      return false;
//    sz1++;
//    sz2++;
//  }
//  return true; // end of string so show as match
//}
