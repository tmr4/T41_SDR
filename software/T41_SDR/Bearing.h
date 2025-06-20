
//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

#define BUFFPIXEL                   20  // Use buffer to read image rather than 1 pixel at a time

#define DEGREES2RADIANS             0.01745329
#define RADIANS2DEGREES             57.29578

extern char keyboardBuffer[];
extern int selectedMapIndex;

extern float homeLat, homeLon;

typedef struct {
  char callPrefix[12];
  char country[30];
  double lat;
  double lon;
} cities;

extern cities dxCities[];

typedef struct {
  char mapNames[50];
  float lat;
  float lon;
} maps;

extern maps myMapFiles[];

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
void BearingMaps();

int FindCountry(char *prefix);
float HaversineDistance(float lat2, float lon2);

void ButtonBearing();
