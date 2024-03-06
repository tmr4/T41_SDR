
//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

extern char keyboardBuffer[];
extern int selectedMapIndex;

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

void DrawKeyboard();
void CaptureKeystrokes();
float BearingHeading(char *dxCallPrefix);
void bmpDraw(const char *filename, int x, int y);
uint16_t Color565(uint8_t r, uint8_t g, uint8_t b);
inline void writeRect(int x, int y, int cx, int cy, uint16_t *pixels);
int InitializeSDCard();
int BearingMaps();

