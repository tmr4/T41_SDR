
#include "SDT.h"

#ifdef T41_REMOTE_DISPLAY

#include <FlexSerial.h>
#include <Wire.h>

#include "Display.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

#define I2C_DEV_ADDR_ESP32 0x55

FlexSerial FlexSerial1(26, 27); // pin 26 receive, pin 27 transmit; Works!

bool connected = false;
uint8_t freqData[512];
uint8_t audioData[270];
int fCount = 0;

uint8_t combData[SPECTRUM_RES + 6 + AUDIO_SPEC_BOX_W + 1 + 4]; // FDyyy[512 bytes of freq spectrum data];AD[270 bytes of audio spectrum data]; where yyy = 255 - max

int delayTime = 10;
//int delayTime = 20;
int limit = 32;  // delay 0 gives 32 byte blocks but misses last 32 bytes of first 256 bytes and all but 32 bytes of second 256 bytes of 518 package
                 // delay 5 gives 32 byte blocks passes first 256 bytes ok but misses at least two 32 byte blocks of second 256 bytes
                 // delay 10 gives 32 byte blocks and successful transfer of 518 byte package
                 //, but fails sometimes with 791 byte package

//int delayTime = 40;
//int limit = 128;  // delay 20 gives 128 byte blocks but misses last 128 bytes of 512 byte package
                  // delay 30 gives 128 byte blocks and successful transfer of 518 byte package with 0-255 data (fails to send last 6 bytes with random data)

//int delayTime = 50;
//int limit = 256; // delay 20 gives max 112 byte blocks and much missing data, longer delay doesn't help

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

void InitRemote() {
  // setup comms with ESP32
  //FlexSerial1.begin(115200);
  //Serial.println("Beginning I2C (m) on T41 T4.1...");
  //Wire2.begin();
}

void T41SendData(uint8_t *data, int len) {
  if(len < limit) {
    Wire2.beginTransmission(I2C_DEV_ADDR_ESP32);
    Wire2.write(data, len);
    Wire2.endTransmission(true);
  } else {
    int total = 0;
    while(total < len) {
      Wire2.beginTransmission(I2C_DEV_ADDR_ESP32);
      Wire2.write(&data[total], len - total < limit ? len - total : limit);
      Wire2.endTransmission(true);
      delay(delayTime);
      total += limit;
    }
  }
}

void SendSpectrumData(uint8_t *freqData, uint8_t *audioData) {
  // set up combData for freq and audio spectrum command
  sprintf((char*)combData, "FD%03d", 0); // set 255 - max to 0 for testing
  combData[517] = ';';
  combData[518] = 'A';
  combData[519] = 'D';
  combData[SPECTRUM_RES + 6 + AUDIO_SPEC_BOX_W] = ';';
  combData[791] = 'S';
  combData[792] = 'D';
  combData[793] = 530 + random(1, 30);
  combData[794] = ';';

  for(int i = 0; i < SPECTRUM_RES; i++) {
    combData[i + 5] = freqData[i];
  }

  for(int i = 0; i < AUDIO_SPEC_RES; i++) {
    combData[i + 517 + 2 + 1] = audioData[i];
  }

  T41SendData(combData, SPECTRUM_RES + 6 + AUDIO_SPEC_BOX_W + 1 + 4);
}

void RemoteLoop() {
  if(!connected && FlexSerial1.available()) {
    char command = FlexSerial1.read();
    Serial.print("got command "); Serial.println(command);
    switch(command) {
      case 'C': // connected
        connected = true;
        break;

      default:
        break;
    }
  }

  if(connected) {
    uint8_t max = 0;

    // create fake frequency spectrum and send it to remote head
    for(int i = 0; i < 512; i++) {
      freqData[i] = 230 - random(1, 40);
      //freqData[i] = i;
      if(freqData[i] > max) max = freqData[i];
    }
    //FlexSerial1.print("S");
    //delay(10);
    //SendFreqSpectrumData(freqData, max);

    // create fake audio spectrum and send it to remote head
    max = 0;
    for(int i = 0; i < 270; i++) {
      audioData[i] = 60 - random(1, 30);
      //audioData[i] = i;
      if(audioData[i] > max) max = audioData[i];
    }
    //SendAudioSpectrumData(data);

    SendSpectrumData(freqData, audioData);
    Serial.printf("sent spectrum data: %d\n", ++fCount);
  }
}

#endif
