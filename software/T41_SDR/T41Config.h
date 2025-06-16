//====================== User Specific Preferences =============

//#define DEBUG    // Uncommented for debugging, comment out for normal use
//#define DEBUG_SW // debug switch matrix false presses
//#define DEBUG1

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

#define RIGNAME             "T41-EP SDT"

// select display type
#define RA8875_DISPLAY      1
//#define ILI9488_DISPLAY     1
//#define NO_DISPLAY          1

#define Si_5351_crystal     25000000L
#define MASTER_CLK_MULT     4ULL            // FOURSQRP QSD frontend requires 4x clock

// Audio chain configuration options
//#define USE_MIC_COMPRESSION 1
//#define AUDIO_STATS         1


#define DECODER_STATE							0						                              // 0 = off, 1 = on
#define DEFAULT_KEYER_WPM   			15                                        // Startup value for keyer wpm
#define FREQ_SEP_CHARACTER  			'.'					                              // Some may prefer period, space, or combo
#define MAP_FILE_NAME   					"sf.bmp"                                  // Name you gave to BMP map file. Max is 50 chars
#define MY_LAT										37.5                                      // Coordinates for QTH actually CM87
#define MY_LON										-123.0
#define MY_CALL										"Your Call"                                   // Default max is 10 chars
#define MY_TIMEZONE          			"PST: "                                   // Default max is 10 chars
//DB2OO, 29-AUG-23: TIME_24H to display 24 hour times (Utility.cpp). If this is not defined 12h display will be used
#define TIME_24H                  1
//DB2OO, 29-AUG-23: ITU_REGION to determine band borders: Upper band limits on 80m (3.8MHz vs 4.0MhHz) and 40m (7.2MHz vs. 7.3MHz)
//#define ITU_REGION                1 //for Europe
#define ITU_REGION    2   // for USA
//#define ITU_REGION    3   // Asia/Oceania
// DB2OO, 29.823:. Analog Signal on this pin will be used for an analog S-Meter (250uA full scale) connected via 10kOhm to this output. 1uF capacitor paralle to the S-Meter. --> Display.cpp.
// This might conflict with other hardware modifications, that might use Pin 33 for a different purpose --> please check, before defining this
//#define HW_SMETER              33
// DB2OO, 30-AUG-23: with TCVSDR_SMETER (TCVSDR=Teensy Convolution SDR) defined the S-Meter bar will be consistent with the dBm value and it will not go over the box for S9+40+
#define TCVSDR_SMETER             1
//DB2OO, 10-SEP-23: TCXO_25MHZ defined sets the default EEPROMData.freqCorrectionFactor = 0, as the TCXO is supposed to deliver 25.00000MHz
//#define TCXO_25MHZ                1

#define PADDLE_FLIP								0						                              // 0 = right paddle = DAH, 1 = DIT
#define STRAIGHT_KEY_OR_PADDLES		0						                              // 0 = straight, 1 = paddles

#define CURRENT_FREQ_A            7048000                                   // VFO_A
#define CURRENT_FREQ_B            7030000                                   // VFO_B

                                                                            //            0   1   2     3     4      5       6        7
#define DEFAULTFREQINDEX          6                                         // Default: (10, 50, 100, 250, 1000, 10000, 100000, 1000000)
                                                                            //            0   1   2     3
#define DEFAULT_FT_INDEX          3                                         // Default: (10, 50, 250, 500)

#define DEFAULT_POWER_LEVEL       10                                        // Startup power level. Probably 20 for most people

#define SPLASH_DELAY              4000L                                     // How long to show Splash screen

#define STARTUP_BAND        			1                                         // This is the 40M band (EEPROM.h)

#define CENTER_SCREEN_X           400
#define CENTER_SCREEN_Y           245
#define IMAGE_CORNER_X            162                                       // ImageWidth = 378 Therefore 800 - 378 = 422 / 2 = 211
#define IMAGE_CORNER_Y            0                                        // ImageHeight = 302 Therefore 480 - 302 = 178 / 2 = 89
#define RAY_LENGTH                190

// ==== Pick one of the following encoder configurations
//#define                           NORM_ENCODER
#define                           FOURSQRP
//#define                           PROJECTSYSTEM

#define USE_FULL_MENU             0   // 0 - use top line menus; 1 - use full screen menus
//#define USE_LIVE_NOISE_FLOOR      1   // 0 - don't use, 1 - use live display when setting noise floor

#define SDCARD_MESSAGE_LENGTH     3000L                                     // The number of milliseconds to leave error message on screen

#define BEACON_FILE_NAME          "beacon.bmp"

// uncomment below for USB Host support
#define USB_HOST_SUPPORT

// uncomment below for specific USB Host device support
#define HOST_KEYBOARD_MOUSE_SUPPORT
#define HOST_SERIAL_SUPPORT
//#define HOST_CAT_CONTROL_SUPPORT

//#define T41_USB_AUDIO

// For use with PC apps and connecting to other CAT controlled units over the USB host connector.
// Select one of the noted serial objects according to compile options for enabled services.
// Set disabled services to Serial. Any messages from these services are sent to Arduino serial monitor.
//   Serial when multiple services are enabled and compiling with Serial+MIDI+Audio (Arduino IDE must be closed to connect to T41Server)
//   SerialUSB1 when compiling with Dual or Triple Serial (need to figure which COM port is associated with each)
//   SerialUSB2 when two services enabled and compiling with Triple Serial (need to figure which COM port is associated with each)
//
// *** note: debug messages go out over Serial and will be transmitted if controlSerial
// is set to Serial and the unit is connected to the USB host of another unit ***
#define controlSerial Serial // Serial or SerialUSB1 for USB port or usbHostSerial for USB Host port
#define beaconSerial Serial // Serial or SerialUSB2
#define wsjtSerial Serial // Serial or SerialUSB1 or SerialUSB2

struct maps {
  char mapNames[50];
  float lat;
  float lon;
};

extern struct maps myMapFiles[];
