
//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

extern bool beaconDataFlag;
extern uint8_t beaconData[96];

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

void T41BeaconSetup();
void T41BeaconLoop();
void T41BeaconSendData(uint8_t *data, int len);
