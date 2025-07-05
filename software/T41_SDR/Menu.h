
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

#define NO_MENUS_ACTIVE             0      // No menus displayed
#define PRIMARY_MENU_ACTIVE         1      // A primary menu is active
#define SECONDARY_MENU_ACTIVE       2      // Both primary and secondary menus active

#define TOP_MENU_COUNT              14     // Menus to process
#define START_MENU                  0

extern int32_t mainMenuIndex;
extern int32_t subMenuMaxOptions;
extern int32_t secondaryMenuIndex; // -1 means haven't determined secondary menu

extern const char * topMenus[];
extern const char * secondaryChoices[][8];
extern const int secondaryMenuCount[];
extern int8_t menuStatus;                       // 0 = none, 1 = primary, 2 = secondary

extern const char * menuOptions[][6];

extern void (*functionPtr[])();

extern int receiveEQFlag;
extern int xmitEQFlag;

extern bool getMenuValueActive;
extern bool getMenuOptionActive;
extern bool getMenuSelected;
extern int getMenuInc;
extern void (*ptrMenuLoop)();
extern void (*ptrMenuFollowup)();

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

void Cancel();
void ShowMenu(const char *menu[], int where);
void MenuBarChange(int change);
void ShowMenuBar(int menu, int change);
inline void ShowMenuBar() { ShowMenuBar(0,0); }
void MenuBarSelect();

void GetMenuValue(int minValue, int maxValue, int *currentValue, int increment, const char *prompt, int offset, void (*ptrSetup)(), void (*ptrValue)(), void (*ptrFollowup)());
void GetMenuValueLoop();
void GetMenuOption(int menuIndex, int *ptrCurrentValue, void (*ptrSetup)(), void (*ptrValue)(), void (*ptrFollowup)());
void GetMenuOptionLoop();

void DrawMenuDisplay();
void SetPrimaryMenuIndex();
void SetSecondaryMenuIndex();
