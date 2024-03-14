# T41_SDR

Software designed receiver based on the T41-EP developed by Albert Peter and Jack Purdum.

Initial "fork" from T41-EP software version SDTVer049.2K.

This is a work in progress.  Use at your own risk.

Try out `Tag SDR.1` if you like the T41EEE switch matrix routine *(for better button response)* but want to maintain version SDTVer049.2K functionality.  *Note that the EEPROM configuration file is different and is not compatible with software version SDTVer049.2K.  You should take note of any calibration settings you'd like to retain to make restoring these easier.  You should load this branch after a full memory erase.*

## Tags

* V049.2K - original T41 software version SDTVer049.2K
* SDR.1 - adds T41EEE switch matrix functionality

  * #pragma once for SDT.h
  * separate change log
  * consistent EOL and EOF for all files
  * cleared compiler warnings (commented out unused code; should probably just be removed)
  * added EEPROMWriteSize(), EEPROMReadSize(), ButtonISR(), EnableButtonInterrupts() from T41EEE
  * modified EEPROMWrite()EEPROMRead(), , EEPROMStartup(), SaveAnalogSwitchValues() and ReadSelectedPushButton() to incorporate button interrupts per T41EEE
  * added a temporary function, LoadOpVars() to initialize SDTVer049 global variables to those in EEPROMData (needed until all functions pull from EEPROMData and individual global variables can be eliminated)
  * modified setup() to call LoadOpVars() and EnableButtonInterrupts() and remove extraneous global variable initialization
  * some cleanup in MyConfigurationsFile
  * modified splash screen
  * some code cleanup (mostly removing unused variables/code)


## Branches

* main - currently SDR.1
* dev/v0.1 - SDR.1 with:

  * feature/menu
  * feature/liveNoiseFloor
  * feature/infoBoxCons

* feature/liveNoiseFloor - modifies Noise Floor pushbutton to toggle live setting of noise floor.  Requires configuration setting USE_LIVE_NOISE_FLOOR set to 1.  Includes feature/menu.

* feature/menu - SDR.1 with option to select top line menu functionality (similar to T41EEE) or retain full screen menu (SDTVer049.2K).

* feature/infoBoxCons - consolidates all information box functions into separate code and header files.  Addes info box item structure to facilitate creation and updating of info box.

  * also removes some unused functions and some cleanup

* feature/NFMDecom - adds narrow-band FM demodulation

  * also some cleanup

## Previous changes

* previous dev/v0.1 changes

  * SDR.1 with the following changes:
  * feature/menu
  * restructured program:
    * moving code from SDT.h and T41_SDR.ino to individual header and code files/functions *(need a second pass; much more to do with both files; likely more work to do moving local variables to functions)*
    * removing unused variables and some functions *(and noting others that aren't used)*
    * replaced some variables that serve a duplicate purpose with a single variable or none at all where the variable appears to serve no purpose
  * correct placement of VFO B frequency
  * added some debugging capability to examing switch matrix functionality and anomalies
  * corrected full screen main menu cancel feature
  * fixed some coding errors and shortcomings in original software
  * removed many DEBUG sections that seem outdated
  * some clean up (removing useless and out of date comments)
  * *KIM NR gain is higher as a result of these changes.  Will investigate.*

* previous feature/menu changes

  * Added USE_FULL_MENU option to MyConfigurationFile.h to allow user to select whether to use the STDVer049.2K full screen menus or top line menus similar to T41EEE *(this seems to be the original T41 menu system)*.  Need to verify continued functionality of full screen menu option.
  * Modified ExecuteButtonPress():
  
    * Menu up or Menu down buttons select the main menu
    * Restored/enhanced top line menu functionality
  * Renamed ButtonMenuIncrease and ButtonMenuDecrease to ButtonMenuDown and ButtonMenuUp to reflect the actual button names.  Replaced if/else with switch.
  * Modified ButtonSetNoiseFloor(), ButtonFrequencyEntry(), SetSideToneVolume(), SetCompressionLevel(), SetCompressionRatio(), SetCompressionAttack(), SetCompressionRelease(), SetWPM(), SetTransmitDelay(), MicGainSet(), DoPaddleFlip(), SubmenuSelect() to correct menu display errors in top line menus.
  * Modified EraseMenus() to delete change of menuStatus to NO_MENUS_ACTIVE as this should take place elsewhere.
  * Added Cancel to main menu.
  * Modified ShowMenu() to better display top line menus.
  * Modified some default values to ones that suit me.
  * Made top line menu widths consistent.
  * Added secondaryMenuCount array. *Probably should automate.*
  * Moved secondaryChoices to T41_SDR.ino with other menu options though these are likely more appropriate in the Menu.cpp.
  * Started to clean up initializers that have no effect *(there variables are reset elsewhere on startup)*.
