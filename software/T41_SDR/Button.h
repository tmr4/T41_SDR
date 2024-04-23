
//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

extern bool buttonInterruptsEnabled;

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

void EnableButtonInterrupts();
int ProcessButtonPress(int valPin);
int ReadSelectedPushButton();
void ExecuteButtonPress(int val);
void NoActiveMenu();

void ChangeFreqIncrement(int change);
void ChangeFTIncrement(int change);
