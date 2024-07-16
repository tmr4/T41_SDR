//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

#define MAX_MESSAGE_LENGTH       33     // Max size for each message
#define MAX_MESSAGES             10

extern char keyerMessages[MAX_MESSAGES][MAX_MESSAGE_LENGTH + 1];
extern int selectedMsg;
extern bool keyerMessagesActive;

extern int keyerState;
extern uint8_t msgBuffer[50];
extern int msgIndexIn;

extern elapsedMillis msec;
extern unsigned long transmitDitLength;
extern float cwRampUp[128], cwRampDown[128];


//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

void SetTransmitDitLength();

void Dit();
void Dah();
void SendCode(char code);

void KeyerSetup();
void KeyerLoop();
