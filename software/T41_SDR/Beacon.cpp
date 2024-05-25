#include "SDT.h"

#include "Display.h"
#include "ft8.h"

// International beacon transmission schedule, see https://www.ncdxf.org/beacon/

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

const int beaconFreq[5] = { 14100000, 18110000, 21150000, 24930000, 28200000 };
bool monitorFreq[5] = { true, false, true, false, true };
bool beaconInit = false;
bool beaconSyncFlag = false;
int currentBeacon = 0;

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
  beaconInit = true;
}

/*****
  Purpose: Returns index of beacon transmitting on the selected band

  Parameter list:
    int - index of band of interest

  Return value
    int - index of beacon transmitting now on the given band
*****/
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
  tft.setTextColor(White);

  // reset message area
  tft.fillRect(WATERFALL_L, YPIXELS - 25 * 5, WATERFALL_W, 25 * 5 + 3, RA8875_BLACK);

  // print messages in 2 columns
  for (int i = 0; i < 5; i++){
    int index = beacon20m - i + (beacon20m - i < 0 ? 18 : 0);
    sprintf(message,"%8d: %.14s %d dBm", beaconFreq[i], beacons[index].region, 0);
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

/*****
  Purpose: sync beacon monitor loop to transmit cycle

  Parameter list:
    void

  Return value
    void
*****/
void autoSyncBeacon() {
  // allow process to loop until we're within 1 second of the next transmit cycle
  if((second())%10 == 9) {
    // now we can sync up without causing a long delay
    while ((second())%10 != 0){
    }

    currentBeacon = GetBeacon20mNow();
    beaconStart =millis();
    beaconSyncFlag = true;
  }
}

/*****
  Purpose: beacon monitor loop

  Parameter list:
    void

  Return value
    void
*****/
void BeaconLoop() {
  if(beaconSyncFlag) {
    int beacon = GetBeacon20mNow();
    if(beacon != currentBeacon) {
      DisplayBeacons(beacon);
      currentBeacon = beacon;
    }
  } else {
    autoSyncBeacon();
    beaconSyncFlag = true;
  }
}
