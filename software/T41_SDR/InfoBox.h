
//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

// info box coordinates and item identifiers
#define INFO_BOX_L        WATERFALL_RIGHT_X + 15
#define INFO_BOX_T        WATERFALL_TOP_Y + 35
#define INFO_BOX_W        260
#define INFO_BOX_H        200

#define IB_ITEM_VOL       0
#define IB_ITEM_AGC       1
#define IB_ITEM_TUNE      2
#define IB_ITEM_FINE      3
#define IB_ITEM_NOTCH     4
#define IB_ITEM_FILTER    5
#define IB_ITEM_ZOOM      6
#define IB_ITEM_COMPRESS  7
#define IB_ITEM_KEY       8
#define IB_ITEM_DECODER   9
#define IB_ITEM_FLOOR     10
#define IB_ITEM_TEMP     11
#define IB_ITEM_LOAD     12

typedef  struct {
  const char *label;      // info box label
  const char **Options;   // label options
  int *option;            // pointer to option selector
  int fontSize;           // 0 - small or 1 - large font (large font takes two rows, adjust item rows and/or IB_ROW_#_Y accordingly)
  int clearWidth;         // maximum number of characters to clear when updating field
  int highlightFlag;      // 0 - highlight all options in green, 1 - don't highlight first option

  // specifying row and col index is easiest but less flexible especailly if you use both small and large fonts
  // as in the fefault info box
  //int col, row;           // item column and row (up to 10 rows, 2 columns)
  int col, row;           // item placement by screen pixel (up to 10 rows with small font)
  void (*followFnPtr)();  // function to run after info box field is updated (note that these may be hard-coded to a particular location
                          // and will need updated if the underlying item is moved)
} infoBoxItem;

extern const infoBoxItem infoBox[];

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

void UpdateInfoBox();
void UpdateInfoBoxItem(const infoBoxItem *item);
void UpdateIBWPM();
void UpdateDecodeLockIndicator();
void DrawInfoBoxFrame();
