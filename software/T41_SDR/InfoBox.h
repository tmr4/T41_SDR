
//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

// info box coordinates and item identifiers
#define INFO_BOX_L        SPECTRUM_LEFT_X + SPECTRUM_RES + 15
#define INFO_BOX_T        SPECTRUM_TOP_Y + SPECTRUM_HEIGHT + 40
#define INFO_BOX_W        XPIXELS - INFO_BOX_L // use up remainder of screen right
#define INFO_BOX_H        YPIXELS - INFO_BOX_T // use up remainder of screen bottom

#define IB_ITEM_VOL       0
#define IB_ITEM_AGC       1
#define IB_ITEM_TUNE      2
#define IB_ITEM_FINE      3
#define IB_ITEM_ZOOM      4
#define IB_ITEM_DECODER   5
#define IB_ITEM_FLOOR     6
#define IB_ITEM_TEMP      7
#define IB_ITEM_LOAD      8
#define IB_ITEM_FT8       9
#define IB_ITEM_STACK     10
#define IB_ITEM_HEAP      11

// set not used items to a value greater than the total and any update calls
// will be ignored
#define IB_ITEM_NOTCH     20
#define IB_ITEM_FILTER    20
#define IB_ITEM_COMPRESS  20
#define IB_ITEM_KEY       20

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

void UpdateInfoBox();
void UpdateInfoBoxItem(uint8_t item);
void UpdateIBWPM();
void UpdateDecodeLockIndicator();
void DrawInfoBoxFrame();

void MouseButtonInfoBox(int button, int cursorX, int cursorY);
void MouseWheelInfoBox(int wheel, int x, int y);
void HighlightIBItem(uint8_t item, int color);
