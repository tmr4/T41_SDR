#include "SDT.h"

//#include "ButtonProc.h"
//#include "Display.h"
//#include "Encoders.h"
//#include "EEPROM.h"
//#include "keyboard.h"
//#include "InfoBox.h"
//#include "MenuProc.h"
//#include "mouse.h"
//#include "Tune.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

bool beaconDataFlag = false;
uint8_t beaconData[96]; // format bytes "BM" + band index, beacon index, volume, 18 * 5 SNR patch color indexes

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

// the three usb serial objects in the teensy (Serial, SerialUSB1 and SerialUSB2) are all different classes (usb_serial_class, usb_serial2_class, and usb_serial3_class)
// I suppose to prevent naming conflict somewhere, but this prevents having serial commands with a common argument specifying the serial channel to use, such as
// void T41BeaconSetup(Stream& serial) { serial.begin(); }.  As such might as well duplicate these functions for both the T41 control app and Beacon monitor
void T41BeaconSetup() {
  SerialUSB2.begin(19200);
}

void T41BeaconSendData(uint8_t *data, int len) {
  // *** TODO: work up alternative if USB buffer is sufficient ***
  if(SerialUSB2.availableForWrite() > len) {
    SerialUSB2.write(data, len);
  }
}

void T41BeaconGetCommand(char * cmd, int max) {
  int i = 0;

  while(SerialUSB2.available() > 0) {
    cmd[i] = (char) SerialUSB2.read();

    //Serial.print("cmd from SerialUSB2: "); Serial.println(cmd[i]);
    // there might be multiple commands in the serial buffer
    // read only the first one or up to the specified limit
    if(cmd[i] == ';' || i >= max) {
      break;
    }
    i++;
  }
  cmd[i+1] = 0; // *** TODO: this is currently needed by send command, revisit if that is changed ***
}

void T41BeaconLoop()
{
  if(SerialUSB2.available()) {
    char cmd[256];

    T41BeaconGetCommand(cmd, 256);

    switch(cmd[0]) {
      case 'D':
        if(cmd[1] == 'S' && cmd[2] == ';') {
          // start sending beacon data
          beaconDataFlag = true;
        } else if(cmd[1] == 'P' && cmd[2] == ';') {
          // stop sending beacon data
          beaconDataFlag = false;
        }
        break;

      case 'T':
        if(cmd[1] == 'M' && cmd[13] == ';') {
          // set Teensy RTC
          //Serial.print("TM cmd from SerialUSB2: "); Serial.println(atol(&cmd[2]));
          //Serial.println(Teensy3Clock.get());
          Teensy3Clock.set(atol(&cmd[2]));
          setTime(atol(&cmd[2]));
        }
        break;

      default:
        break;
    }
  }
}
