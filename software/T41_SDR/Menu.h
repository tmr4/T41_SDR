
//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

#define PRIMARY_MENU                  0
#define SECONDARY_MENU                1

#define PRIMARY_MENU_X                16
#define SECONDARY_MENU_X              260
#define MENUS_Y                       0
#define EACH_MENU_WIDTH               260
#define BOTH_MENU_WIDTHS             (EACH_MENU_WIDTH * 2)

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

#define NO_MENUS_ACTIVE             0      // No menus displayed 
#define PRIMARY_MENU_ACTIVE         1      // A primary menu is active
#define SECONDARY_MENU_ACTIVE       2      // Both primary and secondary menus active

#define TOP_MENU_COUNT              13     // Menus to process
#define START_MENU                  0

extern int32_t mainMenuIndex;
extern int32_t subMenuMaxOptions;
extern int32_t secondaryMenuIndex; // -1 means haven't determined secondary menu

extern const char *topMenus[];
extern const char *secondaryChoices[][8];
extern const int secondaryMenuCount[];
extern int8_t menuStatus;                       // 0 = none, 1 = primary, 2 = secondary

extern void (*functionPtr[])();

extern int receiveEQFlag;
extern int xmitEQFlag;

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

void Cancel();
void ShowMenuBar(int menu, int change);
inline void ShowMenuBar() { ShowMenuBar(0,0); }
void MenuBarChange(int change);
void ShowMenu(const char *menu[], int where);
void MenuBarSelect();
int DrawMenuDisplay();
int SetPrimaryMenuIndex();
int SetSecondaryMenuIndex();
int SubmenuSelect(const char *options[], int numberOfChoices, int defaultStart);
