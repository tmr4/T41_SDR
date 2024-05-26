#include "SDT.h"

#include "ButtonProc.h"
#include "Display.h"
#include "EEPROM.h"
#include "Tune.h"

// International beacon transmission schedule, see https://www.ncdxf.org/beacon/

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

long priorFreq;
long priorBeaconBandFreq[5];
int priorBand;
int priorMode;
int priorDemod;

const int beaconBand[5] = { BAND_20M, BAND_17M, BAND_15M, BAND_12M, BAND_10M };
const int beaconFreq[5] = { 14100000, 18110000, 21150000, 24930000, 28200000 };
bool monitorFreq[5] = { true, false, true, false, true };
bool beaconInit = false;
bool beaconSyncFlag = false;

uint32_t beaconStart;

typedef struct {
  int order;
  const char *region;
  const char *callSign;
  const char *site;
  const char *grid;
  int status; // 0-active, >0 see status message at https://www.ncdxf.org/beacon/
  bool active;
  bool monitor; // true-monitor, false-don't monitor
} Beacon;

// *** TODO: load beacon network from SD card on startup and optionally(?) user editable depending on hardware ***
// updated 5/25/2024 from https://www.ncdxf.org/beacon/beaconlocations.html
Beacon beacons[18] = {
  // #   Region         Call sign    Site            Grid     status,active,monitor
  {  1, "United Nations", "4U1UN ", "New York    ", "FN30as", 0, true, true },
  {  1, "Canada        ", "VE8AT ", "Inuvik, NT  ", "CP38gh", 0, true, true },
  {  1, "California    ", "W6WX  ", "Mt Umunhum  ", "CM97bd", 0, true, true }, // region listed as "United States"
  {  1, "Hawaii        ", "KH6RS ", "Maui        ", "BL10ts", 0, true, true },
  {  1, "New Zealand   ", "ZL6B  ", "Masterton   ", "RE78tw", 0, true, true },
  {  1, "Australia     ", "VK6RBP", "Rolystone   ", "OF87av", 0, true, true },
  {  1, "Japan         ", "JA2IGY", "Mt Asama    ", "PM84jk", 0, true, true },
  {  1, "Russia        ", "RR9O  ", "Novosibirsk ", "NO14kx", 0, true, true },
  {  1, "Hong Kong     ", "VR2B  ", "Hong Kong   ", "OL72bg", 0, true, true },
  {  1, "Sri Lanka     ", "4S7B  ", "Colombo     ", "MJ96wv", 0, true, true },
  {  1, "South Africa  ", "ZS6DN ", "Pretoria    ", "KG33xi", 0, true, true },
  {  1, "Kenya         ", "5Z4B  ", "Kikuyu      ", "KI88hr", 0, true, true },
  {  1, "Israel        ", "4X6TU ", "Tel Aviv    ", "KM72jb", 0, true, true },
  {  1, "Finland       ", "OH2B  ", "Lohja       ", "KP20eh", 0, true, true },
  {  1, "Madeira       ", "CS3B  ", "Sao Jorge   ", "IM12jt", 0, true, true },
  {  1, "Argentina     ", "LU4AA ", "Buenos Aires", "GF05tj", 0, true, true },
  {  1, "Peru          ", "OA4B  ", "Lima        ", "FH17mw", 0, true, true },
  { 18, "Venezuela     ", "YV5B  ", "Caracas     ", "FJ69cc", 0, true, true }
};

float beaconSNR[18][5];

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

/*****
  Purpose: Initialize beacon monitor

  Parameter list:
    void

  Return value
    void
*****/
void BeaconInit() {
  // *** temporary ***
  // create area on T41 display for beacon info
  // Erase waterfall in region
  tft.fillRect(WATERFALL_L, YPIXELS - 25 * 5, WATERFALL_W, 25 * 5 + 3, RA8875_BLACK);
  tft.writeTo(L2); // it's on layer 2 as well
  tft.fillRect(WATERFALL_L, YPIXELS - 25 * 5, WATERFALL_W, 25 * 5 + 3, RA8875_BLACK);
  tft.writeTo(L1);
  // set waterfall to one less row
  wfRows = WATERFALL_H - 25 * 5 - 3;

  // initialize beacon SNR
  for(int j = 0; j < 18; j++) {
    for(int i = 0; i < 5; i++) {
      beaconSNR[j][i] = 0;
    }
  }

  beaconInit = true;
}

/*****
  Purpose: Returns index of beacon transmitting on the selected band

  Parameter list:
    int - index of band of interest

  Return value
    int - index of beacon transmitting now on the given band
*****/
// *** TODO: while these should always return 0-17 for a normal clock,
//           consider bound checking to prevent an out of range exception ***
int GetBeaconNow(int band) {
  int index = (((hour() * 60 * 60 + minute() * 60 + second()) % 180) / 10) - band;

  return index + (index < 0 ? 18 : 0);
}
inline int GetBeacon20mNow() {
  return (((hour() * 60 * 60 + minute() * 60 + second()) % 180)) / 10;
}

void DisplayBeacons(int beacon20m) {
  char message[48];
  int rowCount = 5;
  int columnOffset = 0;

  tft.setFontScale(0,1);
  tft.setTextColor(RA8875_WHITE);

  // reset message area
  tft.fillRect(WATERFALL_L, YPIXELS - 25 * 5, WATERFALL_W, 25 * 5 + 3, RA8875_BLACK);

  // print messages in 2 columns
  for (int i = 0; i < 5; i++){
    int index = beacon20m - i + (beacon20m - i < 0 ? 18 : 0);
    sprintf(message,"%8d: %.14s", beaconFreq[i], beacons[index].region);
    tft.setCursor(WATERFALL_L + columnOffset, YPIXELS - 25 * rowCount - 3);
    tft.print(message);

    //Serial.print(i); Serial.print(" : ");
    //Serial.println(message);

    --rowCount;
    if(i == 4) {
      // start in column 2
      rowCount = 5;
      columnOffset = 256;
    }
  }
}

void DisplayBeaconsSNR(int beacon) {
  char message[48];
  int rowCount = 5;
  int columnOffset = 256;

  tft.setFontScale(0,1);
  tft.setTextColor(RA8875_WHITE);

  // reset message area
  //tft.fillRect(WATERFALL_L, YPIXELS - 25 * 5, WATERFALL_W, 25 * 5 + 3, RA8875_BLACK);

  // print messages in 2 columns
  for (int i = 0; i < 5; i++){
    char f1[5], f2[5], f3[5];
    int index = beacon - i + (beacon - i < 0 ? 18 : 0);
    dtostrf(beaconSNR[index][0], 4, 1, f1);
    dtostrf(beaconSNR[index][2], 4, 1, f2);
    dtostrf(beaconSNR[index][4], 4, 1, f3);
    sprintf(message,"%.6s: %.4s %.4s %.4s", beacons[index].callSign, f1, f2, f3);
    tft.setCursor(WATERFALL_L + columnOffset, YPIXELS - 25 * rowCount - 3);
    tft.print(message);

    //Serial.print(i); Serial.print(" : ");
    //Serial.println(message);

    --rowCount;
    //if(i == 4) {
    //  // start in column 2
    //  rowCount = 5;
    //  columnOffset = 256;
    //}
  }
}

/*****
  Purpose: sync beacon monitor loop to transmit cycle

  Parameter list:
    void

  Return value
    void
*****/
void autoSyncBeacon() {
  // loop until we're at a 10 boundary
  while ((second())%10 != 0){
  }

  beaconStart =millis();
  beaconSyncFlag = true;
}

/*****
  Purpose: beacon monitor loop

  Parameter list:
    void

  Return value
    void
*****/
void BeaconLoop() {
  static float min = 0.0, max = -1000.0;
  static int count;
  static bool changeBandFlag = false;
  static int band = 0;
  static int currentBeacon = 0;
  int beacon;

  if(beaconSyncFlag) {
    if(count == 0 && changeBandFlag) {
      // change band
      band++;
      if(band > 4) {
        band = 0;
      }

      // are we monitoring this band?
      // just let the T41 loop bring us back here if we're not monitoring this band
      if(monitorFreq[band]) {
        // change band if needed
        if(beaconBand[band] != currentBand) {
          BandChange(beaconBand[band] - currentBand);
        }

        // allow radio to stablize during the rest of this 10 second cycle
        changeBandFlag = false;
      }
    } else if(count > 0) {
      // *** TODO: probably need abs here ***
      if(dbm < min) {
        min = dbm;
      }
      if(dbm > max) {
        max = dbm;
      }

      beacon = GetBeaconNow(band);
      if(beacon != currentBeacon) {
        // record SNR for current beacon and freq
        // *** TODO: create an aging scheme for SNR ***
        if(min != 0) {
          beaconSNR[beacon][band] = max / min;
        } else {
          beaconSNR[beacon][band] = 0;
        }

        // highlight currently transmitting beacons
        // *** for now just print them to the display ***
        DisplayBeacons(GetBeacon20mNow());

        // update beacon SNR
        // *** for now just print a sample to the display ***
        DisplayBeaconsSNR(beacon);

        // get ready for next beacon
        currentBeacon = beacon;
        min = 0;
        max = -1000.0;
        count++;
        if(count > 18) {
          // reset for next band
          count = 0;
          changeBandFlag = true;
        }
      }
    } else if(count == 0 && !changeBandFlag) {
      // increment count after the next beacon change
      // by this point the radio should be stableized on the band frequency
      // a little awkward but it works
      beacon = GetBeaconNow(band);
      if(beacon != currentBeacon) {
        count++;
      }
    }
  } else {
    // save current state
    priorFreq = TxRxFreq;
    priorBand = currentBand;
    priorMode = xmtMode;
    priorDemod = bands[currentBand].mode;;

    // set radio state for beacon monitoring
    // cycle through the bands to preset the beacon frequencies
    // so we don't have to do it again
    for(int i = 0; i < 5; i++) {
      if(monitorFreq[i]) {
        BandChange(beaconBand[i] - currentBand);

        // save band frequency and set it to the beacon's frequency for this band
        priorBeaconBandFreq[i] = TxRxFreq;
        SetTxRxFreq(beaconFreq[i]);
        band = i; // remember the last band
      }
    }

    // make sure mode and demod mode are set appropriately
    ChangeMode(CW_MODE);
    ChangeDemodMode(DEMOD_USB); // all beacons are on USB

    // initialize beacon state
    autoSyncBeacon();
    currentBeacon = GetBeacon20mNow();
    count = 0;
    changeBandFlag = true;
  }
}
