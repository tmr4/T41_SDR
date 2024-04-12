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
  * feature/NFMDemod
  * expanded waterfall, audio spectrum and info box

* feature/keyboard - adds optional keyboard support to the T41. It uses about 7k RAM.  Keyboard connects to the Teensy USB Host connection.

* feature/dataMode - adds DATA as a separate receiver mode with FT8 as the default demodulation mode.  Transmit DATA mode to come in the future.  FT8 wav files can be played by pressing the Demod button.  Currently FT8.wav decodes the file *"test.wav"* from the SD card and then switches back to regular FT8 decoding.  I plan to move forward with this branch for FT8 rather than tge feature/ft8 branch.

  * Note that similar to the SSB and CW modes, the DATA mode doesn't change when you change bands or VFOs.
  * Fixed DrawBandwidthBar to reset tuning if it will draw outside frequency spectrum box
  * Refined how FT8 messages are printed on screen.  This will be ongong as I work on this feature.

* feature/ft8 - adds FT8 and FT8.wav decoding as optional demodulation modes. FT8.wav selectable while in FT8 mode by pressing the Mode button.  Currently FT8.wav decodes the file *"test.wav"* from the SD card and then switches back to FT8 mode.

  * Created a few shared memory block for use by mutually exclusive modes like CW and FT8
  * deleted many unused functions
  * consolidated buffers where possible to free up memory for FT8
  * moved many non-critical functions to FLASHMEM and constants to PROGMEM
  * added DEBUG_SW define to investigate switch matrix phantom press issues
  * added memInfo() debug routine to help track memory usage
  * modified way audio filters are set up when changing demodulation modes (in SetupMode()) as old method of trying to translate current settings to new mode were hard to maintain as new modes were added.  This needs reworked as the values are currently hardcoded.

* feature/liveNoiseFloor - modifies Noise Floor pushbutton to toggle live setting of noise floor.  Requires configuration setting USE_LIVE_NOISE_FLOOR set to 1.  Includes feature/menu.

* feature/menu - SDR.1 with option to select top line menu functionality (similar to T41EEE) or retain full screen menu (SDTVer049.2K).

* feature/infoBoxCons - consolidates all information box functions into separate code and header files.  Addes info box item structure to facilitate creation and updating of info box.

  * also removes some unused functions and some cleanup

* feature/NFMDemod - adds narrow-band FM demodulation

  * added variable demod filter (use filter button to toggle this filter and the high and low audio spectrum filters)
  * Renamed/modified functions and variables to better reflect their actual use
  * Eliminated some functions/variables/menus that weren't used.  I removed the Noise Floor menu in favor of the live update with the push button.
  * Streamlined display updates, eliminating redundant updates, including updates that overwrote static portions of the display causing them to have to be redrawn
  * Updated spectrum box routines to more fully utilize display and properly show spectrum
  * Refactored some functions to remove display updates that were better consolidated with display update functions (exception when doing so added an needless function call)
  * Consolidated band increase/decrease functions
  * Made selecting display layers consistent throughout (if a function writes to layer 2 it switches to layer 1 prior to returning)
  * Replaced MyDelay() function with standard delay().  I hoped this would more efficiently use the processor but it didn't.
  * Eliminated *some* font/color changes at the end of function, functions that write to display are expected to set font and color
  * Adjusted tuning reset when fine tuning past edge of spectrum
  * Moved some functions to other files to better reflect their use
  * Eliminated some redundant function use checks opting that such checks be made prior to the function call
  * Consolidated some functions within their file to better group related functions
  * Updated some function comment blocks and added display routine description at the top of display.cpp
  * Simplified some loop variables to make code more readable
  * More general cleanup, including eliminating more meaningless comments
  * *Some display updates remain (particularly in the Bearing and EEPROM functions).  I may have missed a few others.*




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


* previous feature/NFMDemod changes

  * added fixed 6 kHz demodulation filter (not shown on spectrum display)
  * separate, adjustable filter for audio output
  * cleaned up audio spectrum box including correcting operation of filter bars

    * likely some cleanup for CW transmit still needed

  * also some general cleanup
