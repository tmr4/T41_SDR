#include "SDT.h"

#include "Bearing.h"
#include "ButtonProc.h"
#include "Display.h"
//#include "EEPROM.h"
#include "t41Beacon.h"
#include "Tune.h"

//#include "font_ArialBold.h"

// International beacon transmission schedule, see https://www.ncdxf.org/beacon/

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

// *** temporary for now ***
// *** currently all displays are beacon monitor only (no normal T41 operating info shown) ***
// *** TODO: consider BEACON_DISPLAY_LIST as becon monitor option with normal T41 display as in the first comment to https://new.reddit.com/r/T41_EP/comments/1d0kvxx/t41_international_beacon_monitor/ ***
#define BEACON_DISPLAY_LIST       0 // List of currently transmitting beacons and rolling list of beacon SNR
#define BEACON_DISPLAY_AZIMUTH    1 // BEACON_DISPLAY_LIST plus azimuthal map with bearing to 5 active beacons
#define BEACON_DISPLAY_WORLD1     2 // World map with beacon call sign highlighted with latest SNR for displayed frequency.  Frequency rolls every 10 seconds.
#define BEACON_DISPLAY_WORLD2     3 // World map with SNR for each frequency noted in a colored squares below each beacon's call sign.  SNRs updated every 10 seconds.

#define BEACON_DISPLAY_OPTION     BEACON_DISPLAY_WORLD2

bool beaconFlag = false;

long priorFreq;
long priorBeaconBandFreq[5];
int priorBand;
int priorMode;
int priorDemod;
int priorFilterHi[5];
int priorFilterLo[5];

int band = 0;
const int beaconBand[5] = { BAND_20M, BAND_17M, BAND_15M, BAND_12M, BAND_10M };
const char *beaconBandName[5] = { "20", "17", "15", "12", "10" };
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
  const char *beaconPrefix; // *** TODO: check if this can be replaced with grid square ***
  bool visible;
  int x, y;
  int status; // 0-active, >0 see status message at https://www.ncdxf.org/beacon/
  bool active;
  bool monitor; // true-monitor, false-don't monitor
} Beacon;

// *** TODO: load beacon network from SD card on startup and optionally(?) user editable depending on hardware ***
// updated 5/25/2024 from https://www.ncdxf.org/beacon/beaconlocations.html
// Prefixes generally from Bearing.cpp and need updated in some cases
Beacon beacons[18] = {
  // #   Region         Call sign    Site            Grid     prefix visible   x    y status,active,monitor
  {  1, "United Nations", " 4U1UN ",  "New York    ", "FN30as", "W2N", true,   227, 170, 0, true, true },
  {  1, "Canada        ", " VE8AT ",  "Inuvik, NT  ", "CP38gh", "BC",  true,    68,  87, 0, true, true },
  {  1, "California    ", " W6WX ",   "Mt Umunhum  ", "CM97bd", "W6F", true,   112, 184, 0, true, true }, // region listed as "United States"
  {  1, "Hawaii        ", " KH6RS ",  "Maui        ", "BL10ts", "KH6", true,    36, 220, 0, true, true },
  {  1, "New Zealand   ", " ZL6B ",   "Masterton   ", "RE78tw", "ZL",  true,   738, 379, 0, true, true }, // x was 768, put it off screen
  {  1, "Australia     ", " VK6RBP ", "Rolystone   ", "OF87av", "VK6", true,   656, 354, 0, true, true },
  {  1, "Japan         ", " JA2IGY ", "Mt Asama    ", "PM84jk", "JA",  true,   695, 184, 0, true, true },
  {  1, "Russia        ", " RR9O ",   "Novosibirsk ", "NO14kx", "UA",  true,   577, 137, 0, true, true },
  {  1, "Hong Kong     ", " VR2B ",   "Hong Kong   ", "OL72bg", "VS6", true,   645, 217, 0, true, true },
  {  1, "Sri Lanka     ", " 4S7B ",   "Colombo     ", "MJ96wv", "4S",  true,   566, 260, 0, true, true },
  {  1, "South Africa  ", " ZS6DN ",  "Pretoria    ", "KG33xi", "ZS",  true,   454, 339, 0, true, true },
  {  1, "Kenya         ", " 5Z4B ",   "Kikuyu      ", "KI88hr", "5Z",  true,   486, 289, 0, true, true },
  {  1, "Israel        ", " 4X6TU ",  "Tel Aviv    ", "KM72jb", "4X",  true,   468, 195, 0, true, true },
  {  1, "Finland       ", " OH2B ",   "Lohja       ", "KP20eh", "OH",  true,   443, 123, 0, true, true },
  {  1, "Madeira       ", " CS3B ",   "Sao Jorge   ", "IM12jt", "CT3", true,   350, 195, 0, true, true },
  {  1, "Argentina     ", " LU4AA ",  "Buenos Aires", "GF05tj", "LU",  true,   263, 365, 0, true, true },
  {  1, "Peru          ", " OA4B ",   "Lima        ", "FH17mw", "OA",  true,   209, 307, 0, true, true },
  { 18, "Venezuela     ", " YV5B ",   "Caracas     ", "FJ69cc", "YV",  true,   238, 253, 0, true, true }
};

float beaconSNR[18][5];
float dxLonBeacon, dxLatBeacon;
float bearingDegreesBeacon, bearingDistanceBeacon, displayBearingBeacon;

//-------------------------------------------------------------------------------------------------------------
// Forwards
//-------------------------------------------------------------------------------------------------------------

void BeaconMapDraw(const char *filename, int x, int y);
float BeaconBearingHeading(char *dxCallPrefix);
void DrawBeaconBearing(char *beaconPrefix, int color);

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
  // clear screen and set display to beacon monitor
  tft.fillWindow();
  tft.writeTo(L2); // clear layer 2 as well
  tft.fillWindow();
  tft.writeTo(L1);
  displayScreen = DISPLAY_BEACON_MONITOR;
  //displayScreen = 2; // use for testing that normal updates have been eliminated, should be a blank display with the exception of the xmit indicator

  // *** temporary ***
  // create area on T41 display for beacon info
  // Erase waterfall in region
  //tft.fillRect(WATERFALL_L, YPIXELS - 25 * 5, WATERFALL_W, 25 * 5 + 3, RA8875_BLACK);
  //tft.writeTo(L2); // it's on layer 2 as well
  //tft.fillRect(WATERFALL_L, YPIXELS - 25 * 5, WATERFALL_W, 25 * 5 + 3, RA8875_BLACK);
  //tft.writeTo(L1);
  // set waterfall to one less row
  //wfRows = WATERFALL_H - 25 * 5 - 3;

  // initialize beacon SNR
  for(int j = 0; j < 18; j++) {
    for(int i = 0; i < 5; i++) {
      beaconSNR[j][i] = 0;
    }
  }

  // draw beacon map
  if((BEACON_DISPLAY_OPTION == BEACON_DISPLAY_LIST || (BEACON_DISPLAY_OPTION == BEACON_DISPLAY_AZIMUTH))) {
    BeaconMapDraw((char *)myMapFiles[BEACON_DISPLAY_AZIMUTH + 1].mapNames, IMAGE_CORNER_X, IMAGE_CORNER_Y);
  }
  if((BEACON_DISPLAY_OPTION > BEACON_DISPLAY_AZIMUTH)) {
    BeaconMapDraw((char *)myMapFiles[BEACON_DISPLAY_WORLD2].mapNames, 0, 0);
  }
  beaconInit = true;
}

/*****
  Purpose: Restore T41 operating state

  Parameter list:
    void

  Return value
    void
*****/
void BeaconExit() {
  // set display screen
  displayScreen = DISPLAY_T41;

  // clear layer 2
  tft.writeTo(L2);
  tft.fillWindow();
  tft.writeTo(L1);
  tft.useLayers(true); // mainly used to turn on layers
  tft.layerEffect(OR); // overlay layers

  // restore T41 state in reverse order (probably overkill)
  // cycle back through monitored bands, restoring prior values
  for(int i = 0; i < 5; i++) {
    if(monitorFreq[i]) {
      ChangeBand(beaconBand[i] - currentBand);

      // save band frequency and set it to the beacon's frequency for this band
      TxRxFreq = priorBeaconBandFreq[i];
      SetTxRxFreq(TxRxFreq);

      // save and set filters for the beacon bands as well
      bands[currentBand].FHiCut = priorFilterHi[i];
      bands[currentBand].FLoCut = priorFilterLo[i];
    }
  }
  ChangeBand(1);
  ChangeDemodMode(priorDemod);
  ChangeMode(priorMode);
  ChangeBand(priorBand - currentBand);
  TxRxFreq = priorFreq;

  RedrawDisplayScreen();
  beaconInit = false;
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

// *** this is all just temporary for now ***
int beaconColor[5] = { RA8875_GREEN, MAGENTA, DARKGREY, ORANGE, RA8875_RED };
void DisplayBeacons(int beacon20m) {
  char message[48];
  int rowCount = 5;
  int columnOffset = 0;

  tft.setFontScale(0,1);

  // can't use layers to display bearings, use it for fast erase of map
  //tft.layerEffect(OR); // bearings show as white
  //tft.layerEffect(AND); // only shows bearings
  //tft.layerEffect(TRANSPARENT); // only shows map
  //tft.layerEffect(FLOATING); // flicking display
  tft.layerEffect(LAYER1);

  // reset message area
  if((BEACON_DISPLAY_OPTION == BEACON_DISPLAY_LIST || (BEACON_DISPLAY_OPTION == BEACON_DISPLAY_AZIMUTH))) {
    tft.fillRect(WATERFALL_L, YPIXELS - 25 * 5, WATERFALL_W, 25 * 5 + 3, RA8875_BLACK);
  }

  // clear beacon traces on layer 1 by copying map on layer 2
  if((BEACON_DISPLAY_OPTION == BEACON_DISPLAY_AZIMUTH)) {
    tft.BTE_move(0, 0, 800, 480, 0, 0, 2);
    while (tft.readStatus())  // Make sure it is done.  Memory moves can take time.
      ;
  }

  // work with currently transmitting beacons
  for (int i = 0; i < 5; i++){
    int index = beacon20m - i + (beacon20m - i < 0 ? 18 : 0);

    if((BEACON_DISPLAY_OPTION == BEACON_DISPLAY_LIST || (BEACON_DISPLAY_OPTION == BEACON_DISPLAY_AZIMUTH))) {
      // print messages in 2 columns
      sprintf(message,"%8d: %.14s", beaconFreq[i], beacons[index].region);
      tft.setCursor(WATERFALL_L + columnOffset, YPIXELS - 25 * rowCount - 3);
      tft.setTextColor(beaconColor[i]);
      tft.print(message);

      --rowCount;
      if(i == 4) {
        // start in column 2
        rowCount = 5;
        columnOffset = 256;
      }
    }

    if((BEACON_DISPLAY_OPTION == BEACON_DISPLAY_AZIMUTH)) {
      // draw bearing for beacon
      // *** doesn't work on layer 2 ***
      //tft.writeTo(L2);
      DrawBeaconBearing((char *)beacons[index].beaconPrefix, beaconColor[i]);
      //tft.writeTo(L1);
    }

    if((BEACON_DISPLAY_OPTION == BEACON_DISPLAY_WORLD1)) {
      // highlight beacon
      tft.setTextColor(beaconColor[i]);
      tft.setCursor(beacons[index].x, beacons[index].y);
      tft.print(beacons[index].callSign);
    }
  }
}

//                        S0            S1        S2      S3        S4    S5          S6            S7      S8      S9
//int beaconSNRColor[10] = { RA8875_BLACK, DARKGREY, PURPLE, DARKCYAN, CYAN, DARK_GREEN, RA8875_GREEN, YELLOW, ORANGE, RA8875_RED };
// orange is the same as yellow
int beaconSNRColor[10] = { RA8875_BLACK, RA8875_LIGHT_GREY, RA8875_PURPLE, RA8875_BLUE, RA8875_CYAN, DARK_GREEN, RA8875_GREEN, RA8875_YELLOW, RA8875_DARK_ORANGE, RA8875_RED };

int GetSNRColor(int snr, int *index) {
  int color = RA8875_BLACK;

  if(snr > 0) {
    *index = (snr / 6);
    if(*index < 10) {
      color = beaconSNRColor[*index];
    } else {
      color = RA8875_RED;
    }
  }

  return color;
}

void DisplayBeaconsSNR(int beacon) {
  char message[48];
  int rowCount = 5;
  int columnOffset = 256;
  int index = 0;
  static int beaconFreqCount = 0;

  tft.layerEffect(LAYER1);
  if((BEACON_DISPLAY_OPTION < BEACON_DISPLAY_WORLD1)) {
    // *** TODO: these need reformated for BEACON_DISPLAY_AZIMUTH and without normal T41 operation display can be expanded ***
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

      --rowCount;
      //if(i == 4) {
      //  // start in column 2
      //  rowCount = 5;
      //  columnOffset = 256;
      //}
    }
  }

  if((BEACON_DISPLAY_OPTION == BEACON_DISPLAY_WORLD1)) {
    // show snr results with a call sign highlight for a rolling frequency
    if(beaconFreqCount > 4) beaconFreqCount = 0;
    if(monitorFreq[beaconFreqCount]) {
      for (int i = 0; i < 18; i++){
        // *** following line gives a sample SNR highlight ***
        //int color = GetSNRColor(i - (i > 10 ? 10 : 0));
        int color = GetSNRColor(beaconSNR[i][beaconFreqCount], &index);

        tft.setCursor(beacons[i].x, beacons[i].y);
        if(color == RA8875_BLACK) {
          tft.setTextColor(RA8875_WHITE, color);
        } else {
          tft.setTextColor(RA8875_BLACK, color);
        }
        tft.print(beacons[i].callSign);
      }

      tft.setFontScale(2);
      tft.setTextColor(RA8875_BLACK, RA8875_DARK_ORANGE);
      tft.setCursor(310,430);
      tft.print(beaconFreq[beaconFreqCount]);
      beaconFreqCount++; // roll to next frequency
    }
  }

  if((BEACON_DISPLAY_OPTION == BEACON_DISPLAY_WORLD2)) {
    for (int i = 0; i < 18; i++){
      tft.setFontScale(0,1);
      // print beacon call sign
      tft.setCursor(beacons[i].x, beacons[i].y);
      tft.setTextColor(RA8875_BLACK, RA8875_RED);
      tft.print(beacons[i].callSign);

      // print SNR square for each monitored band
      for(int j = 0; j < 5; j++) {
        // *** following 2 lines gives a sample SNR patch ***
        //index = i+j - ((i+j) >= 10 ? 10 + ((i+j) >= 20 ? 10 : 0) : 0);
        //int color = beaconSNRColor[index];
        int color = GetSNRColor(beaconSNR[i][j], &index);

        tft.setFontScale(0);
        tft.setCursor(beacons[i].x + j * 10, beacons[i].y + 33);

        if(monitorFreq[j]) {
          if(color == RA8875_BLACK) {
            tft.setTextColor(RA8875_WHITE, color);
          } else {
            tft.setTextColor(RA8875_BLACK, color);
          }

          tft.print(beaconBandName[j]);
        }

        if(beaconDataFlag) {
          if(monitorFreq[j]) {
            beaconData[i * 5 + j + 5] = index < 10 ? index : 9;
          } else {
            beaconData[i * 5 + j + 5] = 0;
          }
        }
      }
    }

    // *** TODO: move fixed items to init function and adjust what is erased ***
    tft.fillRect(10, 410, 170, 60, RA8875_BLACK);
    tft.setFontScale(0);
    tft.setTextColor(RA8875_WHITE);
    tft.setCursor(10, 410);
    tft.print("Monitoring: ");
    //tft.print(beaconFreq[band]); // *** TODO: consider just using band name here instead ***
    tft.print(beaconBandName[band]);
    tft.print("m");
    tft.setCursor(10, 430);
    //tft.print("Beacon: ");
    tft.print(beacons[beacon].callSign);
    //tft.print(" ");
    tft.print(beacons[beacon].region);
    tft.setCursor(10, 450);
    tft.print("Volume: ");
    tft.print(audioVolume);

    if(beaconDataFlag) {
      beaconData[0] = 'B';
      beaconData[1] = 'M';
      beaconData[2] = (uint8_t)band;
      beaconData[3] = (uint8_t)beacon;
      beaconData[4] = (uint8_t)audioVolume;
      beaconData[95] = ';';

      T41BeaconSendData(beaconData, 96);
    }
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
#define NOISE_LO 3
#define NOISE_HI 28
void BeaconLoop() {
  static float min = 0.0, max = -1000.0;
  //static double aveNoiseSquared = 0, aveSignalSquared = 0;
  static int snrCount = 0;
  static int count;
  static bool changeBandFlag = false;
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
          ChangeBand(beaconBand[band] - currentBand);
        }

        // allow radio to stablize during the rest of this 10 second cycle
        changeBandFlag = false;
      }
    } else if(count > 0) {
      snrCount++;

      // trying out some different SNR routines. Code uses #1 currently.
      // 1 - max - min dBm over 10 second cycle
      //      - can yield some large values for weak signals
      // 2 - estimate average signal and noise power levels
      //      - technically inaccurate as these two should be measured at same time
      //      - can yield negative SNR, must set these to zero
      //      - still yields some high values in noisy conditions
      // 3 - simple average of dBm over same intervals as #2
      //      - technically inaccurate as these two should be measured at same time
      //      - doesn't seem any better
      // I'm now doing some statistical analysis on actual signals to see if what we're
      // getting here. Perhaps relying on dbm, which is an average over the loop, hides
      // some detail we need.  Perhaps we need to get our data from ProcessIQData() in
      // Process.cpp.
      // #1
      if(dbm < min) {
        min = dbm;
      }
      if(dbm > max) {
        max = dbm;
      }

      // #2 and #3
      // break 10 second beacon cycle into noise and signal segments
      //if((snrCount <= NOISE_LO) || (snrCount >= NOISE_HI)) {
      //  //aveNoiseSquared += sq(pow(10, dbm/10.0));
      //  min += dbm;
      //} else {
      //  //aveSignalSquared += sq(pow(10, dbm/10.0));
      //  max += dbm;
      //}

      beacon = GetBeaconNow(band);
      //Serial.print(snrCount); Serial.print(","); Serial.print(beacons[beacon].callSign); Serial.print(","); Serial.println(dbm);
      //Serial.print(snrCount); Serial.print(","); Serial.println(dbm);
      if(beacon != currentBeacon) {
        // record SNR for current beacon and freq
        // *** TODO: create an aging scheme for SNR ***
        //if(min != 0) {
        //if(aveNoiseSquared != 0 && (snrCount != (NOISE_HI - NOISE_LO - 2))) {
        if(snrCount != (NOISE_HI - NOISE_LO - 2)) {
          double value = max - min;
          //double value = 10.0 * log(aveSignalSquared / (NOISE_HI - NOISE_LO - 1) / aveNoiseSquared * (snrCount - NOISE_HI + NOISE_LO + 2));
          //double value = max / (NOISE_HI - NOISE_LO - 1) - min / (snrCount - NOISE_HI + NOISE_LO + 2);

          beaconSNR[beacon][band] = value > 0 ? value : 0;
        } else {
          beaconSNR[beacon][band] = 0;
        }

        // highlight currently transmitting beacons
        if((BEACON_DISPLAY_OPTION == BEACON_DISPLAY_LIST || (BEACON_DISPLAY_OPTION == BEACON_DISPLAY_AZIMUTH))) {
          DisplayBeacons(GetBeacon20mNow());
        }

        // update beacon SNR
        DisplayBeaconsSNR(beacon);

        //Serial.print(snrCount); Serial.print(","); Serial.print(beacons[beacon].callSign); Serial.print(","); Serial.println(dbm);
        snrCount = 0;
        //Serial.print(beacons[beacon].callSign); //Serial.print(",");

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
        ChangeBand(beaconBand[i] - currentBand);

        // save band frequency and set it to the beacon's frequency for this band
        priorBeaconBandFreq[i] = TxRxFreq;
        SetTxRxFreq(beaconFreq[i]);

        // save and set filters for the beacon bands as well
        priorFilterHi[i] = bands[currentBand].FHiCut;
        priorFilterLo[i] = bands[currentBand].FLoCut;
        bands[currentBand].FHiCut = 1500;
        bands[currentBand].FLoCut = 500;

        band = i; // remember the last band
      }
    }

    // Change bands one more time, it doesn't matter where.
    // This is required by to lock in the frequency for
    // the last band change above.  We could use custom code to do
    // this but why bother when this simple addition does what we need.
    // *** TODO: figure out why the next band change during the monitoring
    //           process doesn't do this ***
    ChangeBand(1);

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

// *** TODO: just refactor drawing of bearings from the Bearing.cpp bmpDraw to avoid the duplication here ***
/*
*****/
FLASHMEM void BeaconMapDraw(const char *filename, int x, int y) {
  File bmpFile;
  int bmpWidth, bmpHeight;             // W+H in pixels
  uint8_t bmpDepth;                    // Bit depth (currently must be 24)
  uint32_t bmpImageoffset;             // Start of image data in file
  uint32_t rowSize;                    // Not always = bmpWidth; may have padding
  uint8_t sdbuffer[3 * BUFFPIXEL];     // pixel in buffer (R+G+B per pixel)
  uint16_t lcdbuffer[BUFFPIXEL];       // pixel out buffer (16-bit per pixel)
  uint8_t buffidx = sizeof(sdbuffer);  // Current position in sdbuffer
  boolean goodBmp = false;             // Set to true on valid header parse
  boolean flip = true;                 // BMP is stored bottom-to-top
  int w, h, row, col, xpos, ypos;

  uint8_t r, g, b;
  uint32_t pos = 0;
  uint8_t lcdidx = 0;

  if ((x >= tft.width()) || (y >= tft.height()))
    return;

  if (!SD.begin(BUILTIN_SDCARD)) {
    tft.print("SD card cannot be initialized.");
    delay(2000L);  // Given them time to read it.
    return;
  }
  // Open requested file on SD card
  if ((bmpFile = SD.open(filename)) == false) {
    tft.setCursor(100, 300);
    tft.print("File not found");
    return;
  }

  // Parse BMP header
  if (read16(bmpFile) == 0x4D42) {  // BMP signature
    read32(bmpFile);
    (void)read32(bmpFile);             // Read & ignore creator bytes
    bmpImageoffset = read32(bmpFile);  // Start of image data

    // Read DIB header
    read32(bmpFile);
    bmpWidth = read32(bmpFile);
    bmpHeight = read32(bmpFile);

    if (read16(bmpFile) == 1) {                          // # planes -- must be '1'
      bmpDepth = read16(bmpFile);                        // bits per pixel
      if ((bmpDepth == 24) && (read32(bmpFile) == 0)) {  // 0 = uncompressed
        goodBmp = true;                                  // Supported BMP format -- proceed!

        //                                                      BMP rows are padded (if needed) to 4-byte boundary
        rowSize = (bmpWidth * 3 + 3) & ~3;

        // If bmpHeight is negative, image is in top-down order.
        // This is not canon but has been observed in the wild.
        if (bmpHeight < 0) {
          bmpHeight = -bmpHeight;
          flip = false;
        }

        // Crop area to be loaded
        w = bmpWidth;
        h = bmpHeight;
        if ((x + w - 1) >= tft.width()) w = tft.width() - x;
        if ((y + 135 - 1) >= tft.height()) h = tft.height() - y;

        // Set TFT address window to clipped image bounds
        ypos = y;
        for (row = 0; row < h; row++) {  // For each scanline...
          // Seek to start of scan line.  It might seem labor-
          // intensive to be doing this on every line, but this
          // method covers a lot of gritty details like cropping
          // and scanline padding.  Also, the seek only takes
          // place if the file position actually needs to change
          // (avoids a lot of cluster math in SD library).
          if (flip)  // Bitmap is stored bottom-to-top order (normal BMP)
            pos = bmpImageoffset + (bmpHeight - 1 - row) * rowSize;
          else  // Bitmap is stored top-to-bottom
            pos = bmpImageoffset + row * rowSize;

          if (bmpFile.position() != pos) {  // Need seek?
            bmpFile.seek(pos);
            buffidx = sizeof(sdbuffer);  // Force buffer reload
          }
          xpos = x;
          for (col = 0; col < w; col++) {  // For each column...
            // Time to read more pixel data?
            if (buffidx >= sizeof(sdbuffer)) {  // Indeed
              // Push LCD buffer to the display first
              if (lcdidx > 0) {
                tft.drawPixels(lcdbuffer, lcdidx, xpos, ypos);
                xpos += lcdidx;
                lcdidx = 0;
              }

              bmpFile.read(sdbuffer, sizeof(sdbuffer));
              buffidx = 0;  // Set index to beginning
            }

            // Convert pixel from BMP to TFT format
            b = sdbuffer[buffidx++];
            g = sdbuffer[buffidx++];
            r = sdbuffer[buffidx++];
            lcdbuffer[lcdidx++] = Color565(r, g, b);
            if (lcdidx >= sizeof(lcdbuffer) || (xpos - x + lcdidx) >= w) {
              tft.drawPixels(lcdbuffer, lcdidx, xpos, ypos);
              lcdidx = 0;
              xpos += lcdidx;
            }
          }  // end pixel
          ypos++;
        }  // end scanline

        // Write any remaining data to LCD
        if (lcdidx > 0) {
          tft.drawPixels(lcdbuffer, lcdidx, xpos, ypos);
          xpos += lcdidx;
        }
      }  // end goodBmp

      // copy map to layer 2
      tft.BTE_move(0, 0, 800, 480, 0, 0, 1, 2);
      while (tft.readStatus())  // Make sure it is done.  Memory moves can take time.
      ;
    }
  }
  bmpFile.close();
  if (!goodBmp) {
    tft.setCursor(100, 300);
    tft.print("BMP format not recognized.");
  }
}

void DrawBeaconBearing(char *beaconPrefix, int color) {
  float homeLon = myMapFiles[1].lon; // *** TODO: make these generic ***
  float homeLat = myMapFiles[1].lat;
  //int len;
  int rayStart = 0, rayEnd = 0;
  float x1, y1;
  float homeLatRadians;
  float dxLatBeaconRadians;
  float deltaLon = (dxLonBeacon - homeLon);
  float deltaLonRadians;
  float bearingRadians;

  displayBearingBeacon = BeaconBearingHeading(beaconPrefix);  // Different because of image distortion

//======================================
  homeLatRadians = homeLat * DEGREES2RADIANS;
  dxLatBeaconRadians = dxLatBeacon * DEGREES2RADIANS;
  deltaLonRadians = deltaLon * DEGREES2RADIANS;

  float yR = sin(deltaLonRadians) * cos(dxLatBeaconRadians);
  float xR = cos(homeLatRadians) * sin(dxLatBeaconRadians) - sin(homeLatRadians) * cos(dxLatBeaconRadians) * cos(deltaLonRadians);

  bearingRadians = atan2(yR, xR);
  bearingDegreesBeacon = bearingRadians * RADIANS2DEGREES;

  x1 = CENTER_SCREEN_X;                                          // The image center coordinates
  y1 = CENTER_SCREEN_Y;                                          // and should be the same for all
  //  x2 = CENTER_SCREEN_X * sin(bearingRadians) + CENTER_SCREEN_X;  // Endpoints for ray
  //  y2 = CENTER_SCREEN_Y * cos(bearingRadians) - CENTER_SCREEN_Y;

  rayStart = displayBearingBeacon - 8.0;
  rayEnd = displayBearingBeacon + 8.0;

  if (displayBearingBeacon > 16 && displayBearingBeacon < 345) {  // Check for end-point mapping issues
    rayStart = -8;
    rayEnd = 9;
  } else {
    if (displayBearingBeacon < 9) {
      rayStart = 8 - displayBearingBeacon;
      rayEnd = displayBearingBeacon + 8;
    } else {
      if (displayBearingBeacon > 344) {
        rayStart = displayBearingBeacon - 8;
        rayEnd = displayBearingBeacon + 8;
      }
    }
  }
  //  if (y2 < 0) {
  //    y2 = fabs(y2);
  //  }

  for (int i = rayStart; i < rayEnd; i++) {
    tft.drawLineAngle(x1, y1, displayBearingBeacon + i, RAY_LENGTH, color, -90);
  }
  //len = strlen(dxCities[countryIndex].country);
  //tft.setCursor(380 - (len * tft.getFontWidth(0)) / 2, 1);  // Center city above image
  //tft.setTextColor(RA8875_GREEN);
  //
  //tft.print(dxCities[countryIndex].country);
  //tft.setCursor(20, 440);
  //tft.print("Bearing:  ");
  //tft.print(displayBearingBeacon, 0);
  //tft.print(DEGREE_SYMBOL);
  //tft.setCursor(480, 440);
  //tft.print("  Distance: ");
  //tft.print(bearingDistanceBeacon, 0);
  //tft.print(" km");
  //    tft.print(d * 0.6213712);   // If you want miles instead, comment out previous 2 lines and uncomment
  //    tft.print(" miles");        // these 2 lines
}

// unfortunately this needs duplicated because the original clears the screen and sets a font
// TODO: see if the above can be resolved to reduce redundant code ***
float BeaconBearingHeading(char *dxCallPrefix) {
  float deltaLong;  // For radians conversion
  float x, y;       // Temporary variables
  int countryIndex = -1;

  countryIndex = FindCountry(dxCallPrefix);  // do coutry lookup

  if (countryIndex != -1) {              // Did we find prefix??
    dxLatBeacon = dxCities[countryIndex].lat;  //Yep, but I entered the
    dxLonBeacon = dxCities[countryIndex].lon;
  } else {
    return -1.0;
  }

  deltaLong = (homeLon - dxLonBeacon);

  x = cos(dxLatBeacon * DEGREES2RADIANS) * sin(deltaLong * DEGREES2RADIANS);
  y = cos(homeLat * DEGREES2RADIANS) * sin(dxLatBeacon * DEGREES2RADIANS) - sin(homeLat * DEGREES2RADIANS) * cos(dxLatBeacon * DEGREES2RADIANS) * cos((deltaLong)*DEGREES2RADIANS);
  bearingDistanceBeacon = HaversineDistance(dxLatBeacon, dxLonBeacon);


  bearingDegreesBeacon = atan2(x, y) * RADIANS2DEGREES;
  bearingDegreesBeacon = fmod(bearingDegreesBeacon, 360.0);

  if (bearingDegreesBeacon > 0) {
    displayBearingBeacon = 360.0 - bearingDegreesBeacon;
  } else {
    displayBearingBeacon = bearingDegreesBeacon * -1.0;
  }
  return displayBearingBeacon;
}
