# T41_SDR

Initial "fork" from T41-EP software version SDTVer049.2K, based on the T41-EP developed by Albert Peter and Jack Purdum.

Ultimately, it would be nice to be able to add or remove features from the T41 simply by adding or removing a file.  The software is far from that goal.  The structure of the original software also makes it difficult to collaborate.  Thus this version.

I've focused on a few areas of interest to me:

* cleaning up and making better use of the display *(within the bounds of the original software)*
* adding new input capability *(mouse and keyboard)*
* adding new modes *(NFM demodulation and some data modes)*
* adding new features *(beacon monitor, CW message keyer, CAT control, remote display, USB host connection to another T41)*

The structure of my version has changed significantly from the original.  The features I've added can't be incorporated back into the original software without some work.  Still, these show what's possible with the 4SQRP kit hardware.  I think I've reached the limit of the Teensy onboard memory.  More should be possible with added PSRAM.  I plan some experiments with that using my v12 radio, especially with those features that require a lot of memory, like FT8.  If that proves fruitful I may apply that to my 4SQRP radio as well.

This is a work in progress.  I finally complete my goal of simplifying the sketch and SDT.h files.  The sketch file is now a bit over 600 lines, including new functionality, compared to over 2900 in the original code.  The SDT.h header file is now a bit over 100 lines compared to about 2300 in the original code.  The new files are very readable compared to the original.

This simplification isn't costless.  The original code had 25 files.  Mine now has 88, though many of those reflect added features.  The simplification also doesn't mean that the number of global variables in the code is reduced much from my cleanup effort of a year ago. Like then, most of the remaining global variables just moved to the code files most associated with them, with a extern reference in its header file so it can be include where needed. Reducing the number of global variables further will require more work, but likely isn't practical without a major restructuring of the code.  I probably won't attempt that.

Some functions from the original version may be broken.  If so, they will likely remain so until they are of interest to me.  Use at your own risk.

![4SQRP T41 Teensy Pin Usage](https://github.com/tmr4/T41_SDR/blob/dev/v0.1/images/4SQRP_Teensy_Pin_Usage.png)

## Branches

* main - dev/v0.1 as below through feature/psk31

* dev/v0.1 - SDR.1 with the following branches (see branch details below for what's included in each) and other features that don't have a separate branch:

  * feature/menu
  * feature/liveNoiseFloor
  * feature/infoBoxCons
  * feature/NFMDemod
  * feature/dataMode
  * feature/keyboard
  * feature/mouse
  * feature/psk31
  * feature/USB
  * feature/beacon
  * feature/wsjtx
  * feature/MorseCodeKeyer
  * other features included that don't have a separate brach:
    * this version has the option to automatically set the noise flow.  The noise floor button toggles between Off, Auto and On.  When set to On you can adjust the noise floor with the Filter/Menu/Change encoder live while the radio is operating.  This noise floor setting for each band is preserved to the EEPROM when the noise floor button is toggled back to Off.  When set to Auto, the T41 will maintain the noise floor at the bottom of the frequency display.  This auto noise floor setting is not preserved.
    * expanded waterfall, audio spectrum and info box. Added stack and heap info box items.
    * added DebugSerial objects to use when a single USB serial port is available.
    * deleted the Clear Buffers block of code in ProcessIQData as it caused audio artifacts without a good explanaition for the restrictive limit on audio buffer size.

* feature/remote - Adds code to enable connection to a remote display over Bluetooth.  See this [Reddit post](https://www.reddit.com/r/T41_EP/comments/1etxkq8/t41_wireless_remote_display/) for a demo and discussion.  The code for the remote head is in this [repository](https://github.com/tmr4/T41_Remote_Head).

* feature/MorseCodeKeyer - Adds a keyboard/memory CW keyer.  It currently requires the keyboard feature but could be simplified to be used on a standalone T41.  It has the following features:

  * Set up to 10 preset messages in code.  The selected preset message is highlighted in green at the bottom of the info box.
  * Select desired message to transmit by using the left/right arrow keys on keyboard.
  * Transmit selected preset message by pressing enter key.
  * Press the insert key to edit the selected message. The color of the selected message will change to yellow to indicate that you are in edit mode.  The cursor is at the end of the message.  You can add to the message by typing.  You can overwrite the message using the backspace key.  To keep any changes, use the enter key to exit the edit mode.  If you want to discard the changes, press the escape key.  Any changes made are lost after you turn off your T41.  Note that the normal keyer navigation keys are not active in edit mode.
  * Enter keyboard mode by pressing the up/down arrow key on the keyboard.  The preset message color changes to white to indicate it's no longer active.  You can return to the preset message with the up/down arrow key on the keyboard.
  * Type a message on the keyboard to create a custom CW message. It will be highlighted in green indicating the keyboard mode is active. Press the enter key to send the message or the escape key to erase it.
  * WPM and Sidetone volume are adjustable with the T41 CW WPM and Sidetone menu items.
  * The keyer produces fairly accurately timed CW at 15 WPM.  It's been successfully decoded with another radio.  I need to verify the timing at other WPM rates.  I may need to do some adjustments to the timing.
  * CW signals (dit/dah) are shaped with a 5 ms raised cosine at the start/end to reduce bandwidth and minimize key clicks.
  * Note: I scaled the keyer sidetone volume to give a comfortable volume over the entire RF power range at a setting of 20.  This isn't consistent with the current T41 sidetone volume, so if you've increased that you'll want to reduce it before you try the keyer.

* feature/wsjtx - Communicate with WSJT-X over USB using `TS-890S` as the *Rig* on the WSJT-X *Radio* tab.  Must compile with one of the USB Types that includes both `Serial` and `Audio` (`Audio` isn't required if you connect the audio from the T41 to your PC in another way).

  * Note that WSJT-X doesn't use `TS-890S` commands, but rather `TS-2000` commands.  However, selecting the latter rig results in errors.
  * Communicating with Audio over USB exposes at most one Serial port.  Check out my (T41 Server)[https://github.com/tmr4/T41Server] app to allow communications with multiple apps over a single serial connection to the T41.
  * Due to the above, the WSJT-X feature is incompatible with the USB and Beacon features.  I'm adapting the T41 Server app to make these apps work at the same time.

![Beacon Monitor](https://github.com/tmr4/T41_SDR/blob/feature/beacon/images/beacon_monitor.jpg)

* feature/beacon - T41 beacon monitor

  * a world map view with the SNR for each frequency located in a small colored square below the call sign for each beacon location (image below with random SNR values to show effect).  I've only show three bands above but all five bands could be displayed with a bit of fiddling.  This view includes some monitor info at the bottom left of the display (band and beacon currently being monitored and the audio volume).  The information, including any volume changes, is currently only updated every 10 seconds. There is room for other information.
  * the beacon monitor can be accessed via button 18 (sorry Bearing map!) or the *Beacon Monitor* menu item.  You can exit the Beacon Monitor by pressing button 18 again or via the menu.
  * Notes:
    * world view bitmap for beacon monitor is in the images folder.
    * an accurate clock is assumed.  More work could be done in this area.
    * The SNR calculation needs work.  Aging SNRs in some way could be an option.
    * Bill has described a fairly minimal beacon receiver.  The addition of a USB host connection would allow mouse input which opens many options for user interaction.
    * This version *completes(?)* the transition to an operating T41 radio without normal screen updates. It's still a work in progress as there might be other areas of code that I need to *discover* still updating the screen.
  * The following alternate display options are coded but not fully developed or exposed for user selection:
    * prints the following to the T41 display:
      * the active beacon for each frequency (1st column)
      * a rolling list of beacons showing the calculated SNR for 20m, 15m and 10m (2nd column)
    * an azimuthal map showing the bearing to the currently active beacons
    * a world map view with the call sign highlighted according to the SNR for the frequency shown (cycles through the five bands, one every 10 seconds)

![Beacon Monitor with random SNR](https://github.com/tmr4/T41_SDR/blob/feature/beacon/images/bm_random_snr.jpg)

* feature/USB - adds communications with PC control app over SerialUSB1 (must select `Dual` or `Triple` USB Type when compiling).  A separate control app running on your PC is required (I'm still determining the best way to make this app available).  The control app has the following features:

  * Live view of frequency and audio spectrums, S-meter, waterfall, and filter bandwidth
  * Live updates can be paused or started with the button at the lower left of the waterfall
  * T41 clock set to PC time upon connection
  * Change frequency of active VFO by the active increment with the mouse wheel
  * Change the active increment with a mouse click (center or fine tune indicated by the green highlight in the info box)
  * Zero out the 1000s portion of the active VFO with a right-mouse click
  * Reset tuning of the active VFO with a mouse click on the Center Frequency
  * Switch to the inactive VFO with a mouse click on the inactive VFO
  * Set the noise floor with a mouse click on NF Set and a mouse wheel in the frequency spectrum (this occurs live unlike the base T41 software which stops operation while the noise floor is adjusted)
  * Change the following up or down with the mouse wheel (on the corresponding indicator):
    * Band
    * Operating mode
    * Demodulation mode
    * Transmit power
    * Volume
    * AGC
    * Center and fine tune increment

* feature/psk31 - adds PSK31 data mode. Still a work in progress.

  * removed optional FT8 support as I intend to make this a permanent feature of my version
  * Wav test files now playable with the User 2 button
  * Made wav file support more generic

* feature/mouse - added mouse support.   Currently the mouse can be used as follows:

  * Within the menu area:
    * Open menu with right click
    * Scroll through menu options with the mouse wheel
    * Select menu option with left click
    * Adjust entry value with mouse wheel

  * Within the frequency area:
    * Adjust the frequency in deciles with the mouse wheel
    * Zero out the lower portion of the frequency with a right click
    * Switch VFO with a left click on non-active VFO

  * Within the the operating stats area:
    * Center the frequency with a left click on the center frequency
    * Change the band up or down with a left or right click on the band indicator
    * Toggle through mode and demodulation with a left click on the respective item

  * Within the the audio spectrum box:
    * Adjust the audio filter width with the mouse wheel
    * Toggle active filter end with a left click

  * Within the the spectrum/waterfall:
    * Set the frequency with a left mouse click
    * Adjust the frequency up or down by the active increment with the mouse wheel

  * Within the the info box:
    * Change the volume up or down with the mouse wheel
    * Rotate up or down through frequency increments with the mouse wheel
    * Select the active frequency increment with a left click (label of active increment is highlighted in green)
    * Adjust the zoom level with left or right click or mouse wheel
    * Toggle the noise floor setting with a left click on the NF Set item
    * Adjust the noise floor with the mouse wheel when NF Set is on

  * Within the the Data mode message area:
    * Select active message with a left click
    * Cycle through messages with mouse wheel

* feature/keyboard - adds optional keyboard support to the T41. It uses about 7k RAM.  Keyboard connects to the Teensy USB Host connection.

* feature/dataMode - adds DATA as a separate receiver mode with FT8 as the default demodulation mode.  Transmit DATA mode to come in the future.  FT8 wav files can be played by pressing the Demod button.  Currently FT8.wav decodes the file *"test.wav"* from the SD card and then switches back to regular FT8 decoding.  I plan to move forward with this branch for FT8 rather than tge feature/ft8 branch.

  * Note that similar to the SSB and CW modes, the DATA mode doesn't change when you change bands or VFOs.
  * Fixed DrawBandwidthBar to reset tuning if it will draw outside frequency spectrum box
  * Refined how FT8 messages are printed on screen.  This will be ongong as I work on this feature.
  * FT8 messages are selectable using the filter encoder after toggling this option with the filter button
  * Added detail of the selected FT8 message to the info box.  Information currently included is time message received, frequency, snr and distance from specified grid location.

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
