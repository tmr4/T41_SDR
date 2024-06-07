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

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

// the three usb serial objects in the teensy (Serial, wsjtSerial and SerialUSB2) are all different classes (usb_serial_class, usb_serial2_class, and usb_serial3_class)
// I suppose to prevent naming conflict somewhere, but this prevents having serial commands with a common argument specifying the serial channel to use, such as
// void WSJTControlSetup(Stream& serial) { serial.begin(); }.  As such might as well duplicate these functions for both the T41 control app and Beacon monitor
void WSJTControlSetup() {
  wsjtSerial.begin(19200);
}

void WSJTControlSendCmd(char *cmd) {
  int sizeBuf = wsjtSerial.availableForWrite();
  if(cmd[0] != 0 && sizeBuf > 0) {
    //Serial.print("    Sending ... "); Serial.println(cmd);
    // the size of Teensy 4.1 serial transmit buffer is 8k and is used in 4 2k parts.
    // I've seen about 6k available at this point.
    // (https://forum.pjrc.com/index.php?threads/usb-serial-on-teensy-4-0-buffer-size-limitation.67826/)
    int len = strlen(cmd);
    //Serial.println(sizeBuf);
    if(wsjtSerial.availableForWrite() > len) {
      //int i=0;
      wsjtSerial.write(cmd, len);
      //wsjtSerial.print(cmd);
      wsjtSerial.send_now(); // we'll have a delay without this
      //wsjtSerial.flush(); // *** TODO: this will cause a freeze if PC stops receiving ***
      //while(cmd[i] != 0) {
      //  if(wsjtSerial.availableForWrite() > 0) {
      //    wsjtSerial.print(cmd[i++]);
      //  } else {
      //    wsjtSerial.flush(); // *** TODO: this will cause a freeze if PC stops receiving ***
      //  }
      //}
    } else {
      int i=0;
      //Serial.println(sizeBuf);
      while(cmd[i] != 0) {
        if(wsjtSerial.availableForWrite() > 0) {
          //wsjtSerial.print(cmd[i++]);
        } else {
          wsjtSerial.flush(); // *** TODO: this will cause a freeze if PC stops receiving ***
        }
      }
    }
  }
}

void WSJTControlGetCommand(char * cmd, int max) {
  int i = 0;

  while(wsjtSerial.available() > 0) {
    cmd[i] = (char) wsjtSerial.read();

    // there might be multiple commands in the serial buffer
    // read only the first one or up to the specified limit
    if(cmd[i] == ';' || i >= max) {
      break;
    }
    i++;
  }
  cmd[i+1] = 0; // *** TODO: this is currently needed by send command, revisit if that is changed ***
}

// Kenwood Band
int GetKenwoodBand() {
  int band;
  switch(currentBand) {
    case BAND_80M:
      band=1;
      break;
    case BAND_40M:
      band=2;
      break;
    case BAND_20M:
      band=4;
      break;
    case BAND_17M:
      band=5;
      break;
    case BAND_15M:
      band=6;
      break;
    case BAND_12M:
      band=7;
      break;
    case BAND_10M:
      band=8;
      break;
    default:
      band=2; // 40m
      break;
  }
  return band;
}

// Kenwood TS-890S operating modes
int GetKenwoodMode() {
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

int GetT41Demod(int mode) {
  int demod = DEMOD_LSB;
  switch(mode) {
    case 1: // LSB
      demod = DEMOD_LSB;
      break;
    case 2: // USB
      demod = DEMOD_USB;
      break;
    case 3: // CW
      demod = CW_MODE;
      break;
    case 4: // FM
      demod = DEMOD_NFM;
      break;
    case 5: // AM
      demod = DEMOD_AM;
      break;
    default:
      demod = DEMOD_LSB;
      break;
  }
  return demod;
}

// Kenwood TS-890S computer control commands
// WSJT-X had trouble with this
void WSJTLoop()
{
  if(wsjtSerial.available()) {
    char cmd[256];
    int mode = GetKenwoodMode();

    WSJTControlGetCommand(cmd, 256);
    //Serial.print("Received ");  Serial.println(cmd);
    //int sizeBuf = wsjtSerial.availableForWrite();
    //Serial.println(sizeBuf);
    // *** TODO: some of these need changed from the T41 control app settings ***
    switch(cmd[0]) {
      case 'A':
        if(cmd[1] == 'I' && cmd[2] == ';') {
          sprintf(cmd,"AI0;"); // Auto info off
        } else if(cmd[1] == 'I' && cmd[3] == ';') {
          // auto info command
          return;
        }
        break;

      case 'B':
        if(cmd[1] == 'U' && cmd[2] == ';') {
          // band up
          ChangeBand(1);
          return;
        } else if(cmd[1] == 'D' && cmd[2] == ';') {
          // band up
          ChangeBand(-1);
          return;
        } else if(cmd[1] == 'U' && cmd[3] == ';') {
          if(atoi(&cmd[2]) == 0) {
            sprintf(cmd,"BU0%d;",GetKenwoodBand());
          } else {
            sprintf(cmd,"BU1%d;",GetKenwoodBand());
          }
        } else if(cmd[1] == 'D' && cmd[3] == ';') {
          if(atoi(&cmd[2]) == 0) {
            sprintf(cmd,"BD0%d;",GetKenwoodBand());
          } else {
            sprintf(cmd,"BD1%d;",GetKenwoodBand());
          }
        }
        break;

      case 'F':
        long f;
        switch(cmd[1]) {
          // a frequency change by wsjt is likely to be large and
          // involve a band change, just use center tuning changes
          case 'A':
            if(cmd[13] == ';') {
              // set VFO A frequency
              f = atol(&cmd[2]);
              ChangeBand(f);
              SetCenterTune(f - centerFreq);
              currentFreqA = f;
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
              ChangeBand(f);
              SetCenterTune(f - centerFreq);
              currentFreqB = f;
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

          case 'R':
            if(cmd[2] == ';') {
              sprintf(cmd,"FR0;"); // receive on VFO A
            } else if(cmd[3] == ';') {
              // select VFO
              VFOSelect(atoi(&cmd[2]));
              return;
            }
            break;

          case 'S':
            if(cmd[3] == ';') {
              // fine tune on or off
              SetFtActive(atoi(&cmd[2]));
              return;
            }
            break;

          case 'T':
            if(cmd[2] == ';') {
              sprintf(cmd,"FT1;"); // transmit on VFO B
            } else if(cmd[3] == ';') {
              // select VFO
              VFOSelect(atoi(&cmd[2]));
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
        if(cmd[1] == 'D' && cmd[2] == ';') {
          sprintf(cmd,"ID024;"); // TS-890S
          //sprintf(cmd,"ID019;"); // TS-2000
        } else if(cmd[1] == 'F' && cmd[2] == ';') {
          // retrieves transceiver status
          sprintf(cmd, "IF%011ld%04d%+06d%d%d%d%02d%d%d%d%d%d%d%02d%d;",
            TxRxFreq,     // freq in Hz
            5000,            // freq step size
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
            1,            // CTCSS tone frequency
            0             // shift status
          );
        }
        break;

      case 'M': // the TS890S doesn't have this command, but WSJT-X uses it anyway
        if(cmd[1] == 'D' && cmd[2] == ';') {
          // send demod mode
          sprintf(cmd,"MD%d;", mode);
          //sprintf(cmd,"MD%d;", 2);
          //sprintf(cmd,"?;");
        } else if(cmd[1] == 'D' && cmd[3] == ';') {
          // set demod mode status
          int demod = GetT41Demod(atoi(&cmd[2]));
          //Serial.print("Changing demod mode to: "); Serial.println(demod);
          ChangeDemodMode(demod);
          return;
          //sprintf(cmd,"?;");
        } else if(cmd[1] == 'E' && cmd[3] == ';') {
          // set operating mode
          //ChangeMode(atoi(&cmd[2]));
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

      case 'O': // PCxxx;
        if(cmd[1] == 'M' && cmd[3] == ';') {
          // operating demod mode
          int item = cmd[2];
          sprintf(cmd,"OM%d%d;", item, mode);
        } else if(cmd[1] == 'M' && cmd[4] == ';') {
          // set demod mode status
          char val[2] = { cmd[2], 0 };
          int item = atoi(val);
          val[0] = cmd[3];
          int mode = atoi(val);
          int demod = GetT41Demod(mode);
          if(demod == CW_MODE) {

          } else {
            ChangeDemodMode(demod);
          }
          return;
        }
        break;

      case 'P': // PCxxx;
        if(cmd[1] == 'C' && cmd[5] == ';') {
          // set transmitter power level
          transmitPowerLevel = atoi(&cmd[2]);
          ShowCurrentPowerSetting();
        } else if(cmd[1] == 'S' && cmd[2] == ';') {
          //sprintf(cmd,"PS0;"); // manual has 0=On, 1=Off
          sprintf(cmd,"PS1;"); // wsjt has opposite
        }
        break;

      case 'R': //
        if(cmd[1] == 'X' && cmd[2] == ';') {
          xrState = RECEIVE_STATE;
        }
        return;
        break;

      case 'S': //
        if(cmd[1] == 'P' && cmd[3] == ';') {
          // set split VFO on/off
          return;
        } else if(cmd[1] == 'P' && cmd[2] == ';') {
          sprintf(cmd,"SP%d;", splitVFO ? 1 : 0);
        }
        break;

      case 'T':
        if(cmd[1] == 'M' && cmd[13] == ';') {
          // set Teensy RTC
          //Serial.print("TM cmd from controlSerial: "); Serial.println(atol(&cmd[2]));
          //Serial.println(Teensy3Clock.get());
          Teensy3Clock.set(atol(&cmd[2]));
          setTime(atol(&cmd[2]));
        } else if(cmd[1] == 'X' && cmd[2] == ';') {
          xrState = TRANSMIT_STATE;
        }
        return;
        break;

      default:
        cmd[0] = '?';
        cmd[1] = ';';
        cmd[2] = 0;
        break;
    }

    WSJTControlSendCmd(cmd);
    //Serial.print("Responded with: "); Serial.println(cmd);
  }
}

// Kenwood TS-2000 modes
int GetKenwoodTS2000Mode() {
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
/*
// Kenwood TS-2000 computer control commands
// WSJT-X had trouble with this
void WSJTLoopTS2000()
{
  if(wsjtSerial.available()) {
    char cmd[256];
    int mode = GetKenwoodTS2000Mode();

    WSJTControlGetCommand(cmd, 256);
    //Serial.print("Received ");  Serial.println(cmd);
    //int sizeBuf = wsjtSerial.availableForWrite();
    //Serial.println(sizeBuf);
    // *** TODO: some of these need changed from the T41 control app settings ***
    switch(cmd[0]) {
      case 'A':
        if(cmd[1] == 'I' && cmd[2] == ';') {
          sprintf(cmd,"AI0;"); // Auto info off
        } else if(cmd[1] == 'I' && cmd[3] == ';') {
          // auto info command
          return;
        }
        break;

      case 'B':
        if(cmd[1] == 'U' && cmd[2] == ';') {
          // band up
          ChangeBand(1);
        } else if(cmd[1] == 'D' && cmd[2] == ';') {
          // band up
          ChangeBand(-1);
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
              //Serial.print("Set VFO B to "); Serial.println(f);
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
        if(cmd[1] == 'D' && cmd[2] == ';') {
          sprintf(cmd,"ID019;"); // TS-2000
        } else if(cmd[1] == 'F' && cmd[2] == ';') {
          // retrieves transceiver status
          sprintf(cmd, "IF%011ld%04d%+06d%d%d%d%02d%d%d%d%d%d%d%02d%d;",
            TxRxFreq,     // freq in Hz
            5000,            // freq step size
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
            1,            // CTCSS tone frequency
            0             // shift status
          );
        }
        break;

      case 'M':
        if(cmd[1] == 'D' && cmd[2] == ';') {
          // send demod mode
          sprintf(cmd,"MD%d;", mode);
        } else if(cmd[1] == 'D' && cmd[3] == ';') {
          // set demod mode status
          ChangeDemodMode(atoi(&cmd[2]));
          return;
        } else if(cmd[1] == 'E' && cmd[3] == ';') {
          // set operating mode
          ChangeMode(atoi(&cmd[2]));
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
          return;
        } else if(cmd[1] == 'S' && cmd[2] == ';') {
          sprintf(cmd,"PS0;"); // manual has 0=On, 1=Off
          //sprintf(cmd,"PS1;"); // wsjt has opposite
        }
        break;

      case 'R': //
        if(cmd[1] == 'X' && cmd[2] == ';') {
          xrState = RECEIVE_STATE;
        }
        return;
        break;

      case 'S': //
        if(cmd[1] == 'P' && cmd[3] == ';') {
          // set split VFO on/off
          return;
        } else if(cmd[1] == 'P' && cmd[2] == ';') {
          sprintf(cmd,"SP%d;", splitVFO ? 1 : 0);
        }
        break;

      case 'T':
        if(cmd[1] == 'M' && cmd[13] == ';') {
          // set Teensy RTC
          //Serial.print("TM cmd from controlSerial: "); Serial.println(atol(&cmd[2]));
          //Serial.println(Teensy3Clock.get());
          Teensy3Clock.set(atol(&cmd[2]));
          setTime(atol(&cmd[2]));
        } else if(cmd[1] == 'X' && cmd[2] == ';') {
          xrState = TRANSMIT_STATE;
        }
        return;
        break;

      default:
        cmd[0] = '?';
        cmd[1] = ';';
        cmd[2] = 0;
        break;
    }

    WSJTControlSendCmd(cmd);
    //Serial.print("Responded with: "); Serial.println(cmd);
  }
}
*/
