
#include "SDT.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

/*****
  Purpose: Formats supported objects as a debug message and send it over Serial
           Each instance of DebugSerial has a unique two digit identifier that
           is included in each debug message to identify the instance.

  Debug message format:
    <idobj>

    where:
      id   member variable
      obj  passed object

  Supported objects include: string, char, char[], and numbers of various types (similar to the Serial object)

  Member variables:
    id - two digit debug stream identifier

  Member functions:
    print - sends debug message over Serial
*****/
class DebugSerial {
  private:
    static int count;
    String id;

  public:
    DebugSerial();

    // *** could be expanded for remaining print functions from Print.h ***
    // *** Note: the debug window app outputs the message with Console.WriteLine so println fuctions aren't supported ***

    // Print a string
    inline size_t print(const String &s) { return Serial.print(id) + Serial.print(s) + Serial.print(">"); }
    // Print a single character
    inline size_t print(char c)				  { return Serial.print(id) + Serial.print(c) + Serial.print(">"); }
    // Print a string
    inline size_t print(const char s[])	{ return Serial.print(id) + Serial.print(s) + Serial.print(">"); }
    // Print a string
    inline size_t print(const __FlashStringHelper *f)	{ return Serial.print(id) + Serial.print(f) + Serial.print(">"); }
    // Print an unsigned number
    inline size_t print(uint8_t b)				{ return Serial.print(id) + Serial.print(b) + Serial.print(">"); }
    // Print a signed number
    inline size_t print(int n)				    { return Serial.print(id) + Serial.print(n) + Serial.print(">"); }
    // Print an unsigned number
    inline size_t print(unsigned int n)	{ return Serial.print(id) + Serial.print(n) + Serial.print(">"); }
    // Print a signed number
    inline size_t print(long n)          { return Serial.print(id) + Serial.print(n) + Serial.print(">"); }
    // Print an unsigned number
    inline size_t print(unsigned long n) { return Serial.print(id) + Serial.print(n) + Serial.print(">"); }
    // Print a signed number
    inline size_t print(int64_t n)       { return Serial.print(id) + Serial.print(n) + Serial.print(">"); }
    // Print an unsigned number
    inline size_t print(uint64_t n)      { return Serial.print(id) + Serial.print(n) + Serial.print(">"); }
};

// debug macros and pointers to two instances of DebugSerial are provided, create more as needed
extern DebugSerial *dbSerial, *dbSerial2;

#define DEBUG_ENABLED

#ifdef DEBUG_ENABLED
#define DEBUG_SERIAL(msg) dbSerial->print(msg);
#define DEBUG_SERIAL2(msg) dbSerial2->print(msg);
#elif
#define DEBUG_SERIAL
#define DEBUG_SERIAL2
#endif

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------
