
#include "debugSerial.h"

//-------------------------------------------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------------------------------------------

int DebugSerial::count = 0;

// two instances of DebugSerial are provided, create more as needed
#ifdef DEBUG_ENABLED
  DebugSerial *dbSerial = new DebugSerial();
  DebugSerial *dbSerial2 = new DebugSerial();
#endif

//-------------------------------------------------------------------------------------------------------------
// Code
//-------------------------------------------------------------------------------------------------------------

DebugSerial::DebugSerial() {
  id = '<';
  if(count < 10) {
    id += '0';
  }
  id += String(count);
  //id += '>';
  count++;
}
