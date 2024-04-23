
//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

#define XPIXELS                     800           // This is for the 5.0" display
#define YPIXELS                     480
#define CHAR_HEIGHT                  32
#define PIXELS_PER_EQUALIZER_DELTA   10           // Number of pixels per detent of encoder for equalizer changes
#define PIXELS_PER_AUDIO_DELTA       10

#define SPECTRUM_RES          512
#define SPECTRUM_HEIGHT       150                 // This is the pixel height of spectrum plot area without disturbing the axes

#define SPEC_BOX_L            0
#define SPEC_BOX_T            99
#define SPEC_BOX_W            SPECTRUM_RES + 2
#define SPEC_BOX_H            SPECTRUM_HEIGHT + 2

#define SPECTRUM_LEFT_X       SPEC_BOX_L + 1            // Used to plot left edge of spectrum display
#define SPECTRUM_TOP_Y        SPEC_BOX_T + 1            // Start of spectrum plot space
#define SPECTRUM_BOTTOM       SPECTRUM_TOP_Y + SPECTRUM_HEIGHT - 1

#define SPEC_BOX_LABELS       (SPECTRUM_TOP_Y + SPECTRUM_HEIGHT + 5)

#define FREQUENCY_X           0
#define FREQUENCY_Y           28
#define FREQUENCY_X_SPLIT     280
#define VFO_B_ACTIVE_OFFSET   FREQUENCY_X_SPLIT - 60
#define VFO_B_INACTIVE_OFFSET FREQUENCY_X_SPLIT + 60

#define WATERFALL_L           SPECTRUM_LEFT_X
#define WATERFALL_T           (SPECTRUM_TOP_Y + SPECTRUM_HEIGHT + 25)
#define WATERFALL_W           SPECTRUM_RES            // Pixel width of waterfall
#define WATERFALL_H           YPIXELS - WATERFALL_T       // use up remainder of 480 rows

#define WATERFALL_BOTTOM      (WATERFALL_T + WATERFALL_H)

#define TEMP_X_OFFSET         15
#define TEMP_Y_OFFSET         465                                           // 480 * 0.97 = 465

#define AUDIO_SPEC_BOX_L      (SPECTRUM_LEFT_X + SPECTRUM_RES + 15)
#define AUDIO_SPEC_BOX_T      SPECTRUM_BOTTOM - 118
//#define AUDIO_SPEC_BOX_W      260
#define AUDIO_SPEC_BOX_W      XPIXELS - AUDIO_SPEC_BOX_L // use up rest of screen right
#define AUDIO_SPEC_BOX_H      118
#define AUDIO_SPEC_BOTTOM     SPECTRUM_BOTTOM

#define CLIP_AUDIO_PEAK       115           // The pixel value where audio peak overwrites S-meter

#define OPERATION_STATS_L     5
#define OPERATION_STATS_T     FREQUENCY_Y + 47
#define OPERATION_STATS_W     SPEC_BOX_W - OPERATION_STATS_L
#define OPERATION_STATS_H     25

#define OPERATION_STATS_CF    100 // center frequency
#define OPERATION_STATS_BD    180 // band
#define OPERATION_STATS_MD    220 // mode
#define OPERATION_STATS_CWF   245 // CW filter
#define OPERATION_STATS_DMD   310 // demod mode
#define OPERATION_STATS_PWR   405 // power level

#define X_R_STATUS_X          730
#define X_R_STATUS_Y          70

#define SMETER_X              SPECTRUM_LEFT_X + SPECTRUM_RES + 15
#define SMETER_Y              YPIXELS * 0.22                // 480 * 0.22 = 106
#define SMETER_BAR_HEIGHT     18
#define SMETER_BAR_LENGTH     180
#define SPECTRUM_NOISE_FLOOR  (SPECTRUM_TOP_Y + SPECTRUM_HEIGHT - 3)
#define TIME_X                (XPIXELS * 0.73)                            // Upper-left corner for time
#define TIME_Y                (YPIXELS * 0.07)
#define FILTER_PARAMETERS_X   (XPIXELS * 0.22)
#define FILTER_PARAMETERS_Y   (YPIXELS * 0.213)
#define DEFAULT_EQUALIZER_BAR 100                                         // Default equalizer bar height
#define VFO_A                 0
#define VFO_B                 1
#define VFO_SPLIT             2
#define VFOA_PIXEL_LENGTH     275
#define VFOB_PIXEL_LENGTH     280
#define FREQUENCY_PIXEL_HI    45
#define SPLIT_INCREMENT       500L

// commented out colors are predefined
#define  BLACK                    0x0000      /*   0,   0,   0 */
//#define  RA8875_BLUE              0x000F      /*   0,   0, 128 */
#define  DARK_GREEN               0x03E0      /*   0, 128,   0 */
#define  DARKCYAN                 0x03EF      /*   0, 128, 128 */
#define  MAROON                   0x7800      /* 128,   0,   0 */
#define  PURPLE                   0x780F      /* 128,   0, 128 */
#define  OLIVE                    0x7BE0      /* 128, 128,   0 */
//#define  RA8875_LIGHT_GREY        0xC618      /* 192, 192, 192 */
#define  DARK_RED                 tft.Color565(64,0,0)
#define  DARKGREY                 0x7BEF      /* 128, 128, 128 */
#define  BLUE                     0x001F      /*   0,   0, 255 */
//#define  RA8875_GREEN             0x07E0      /*   0, 255,   0 */
#define  CYAN                     0x07FF      /*   0, 255, 255 */
#define  RED                      0xF800      /* 255,   0,   0 */
#define  MAGENTA                  0xF81F      /* 255,   0, 255 */
#define  YELLOW                   0xFFE0      /* 255, 255,   0 */
#define  WHITE                    0xFFFF      /* 255, 255, 255 */
#define  ORANGE                   0xFD20      /* 255, 165,   0 */
//#define  RA8875_GREENYELLOW       0xAFE5      /* 173, 255,  47 */
#define  PINK                     0xF81F
#define  FILTER_WIN               0x10       // Color of SSB filter width

//========================================= Display pins
#define BACKLIGHT_PIN               6
#define TFT_DC                      9
#define TFT_CS                      10
#define TFT_MOSI                    11
#define TFT_MISO                    12
#define TFT_SCLK                    13
#define TFT_RST                     255

extern int centerLine;

extern int16_t pixelCurrent[SPECTRUM_RES];
extern int16_t pixelnew[SPECTRUM_RES];
extern int16_t pixelold[SPECTRUM_RES];
extern int16_t pixelnew2[];
extern int16_t pixelold2[];
extern int newFilterX;
extern int oldFilterX;
extern int updateDisplayFlag;
extern int wfRows;

extern RA8875 tft;

typedef struct {
  const char *dbText;
  float32_t   dBScale;
  uint16_t    pixelsPerDB;
  uint16_t    baseOffset;
  float32_t   offsetIncrement;
} dispSc;

extern dispSc displayScale[];

extern int newSpectrumFlag;

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

// static items
void ShowName();
void DrawSpectrumFrame();
//void DrawSMeterContainer();
//void DrawAudioSpectContainer();

void DrawStaticDisplayItems();

// mainly updated each loop during call to ShowSpectrum
void ShowFrequency();
void ShowOperatingStats();
void ShowSpectrumFreqValues();
void DrawSmeterBar();
void DrawBandwidthBar();
void ShowBandwidthBarValues();

// 
void ShowSpectrumdBScale();
void ShowAnalogGain();
void ShowTransmitReceiveStatus();
void SetZoom(int zoom);
void ShowCurrentPowerSetting();

void ShowSpectrum();
void UpdateCWFilter();

void RedrawDisplayScreen();

// erase various portions of the screen
void EraseSpectrumDisplayContainer();
void EraseMenus();
void ErasePrimaryMenu();
void EraseSecondaryMenu();
void EraseSpectrumWindow();

void MyDrawFloat(float val, int decimals, int x, int y, char *buff);
void MyDrawFloatP(float val, int decimals, int x, int y, char *buff, int width);

void PrintKeyboardBuffer();
