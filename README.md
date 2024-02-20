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

  * 

* feature/menu - dev/v0.1 with T41EEE menu functionality.

  * 
