
//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

#define BOGUS_PIN_READ             -1 // If no push button read

#define MENU_OPTION_SELECT           0
#define MAIN_MENU_UP                 1
#define BAND_UP                      2
#define ZOOM                         3
#define MAIN_MENU_DN                 4
#define BAND_DN                      5
#define FILTER                       6
#define DEMODULATION                 7
#define SET_MODE                     8
#define NOISE_REDUCTION              9
#define NOTCH_FILTER                10
#define NOISE_FLOOR                 11
#define FINE_TUNE_INCREMENT         12
#define DECODER_TOGGLE              13
#define MAIN_TUNE_INCREMENT         14
#define RESET_TUNING                15
#define UNUSED_1                    16
#define BEARING                     17
#define BEACON                      17

extern bool buttonInterruptsEnabled;

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

void EnableButtonInterrupts();
int ProcessButtonPress(int valPin);
int ReadSelectedPushButton();
void ExecuteButtonPress(int val);
void NoActiveMenu();
