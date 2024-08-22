#include "Wire.h"

#define I2C_DEV_ADDR_ESP32 0x55
#define I2C_DEV_ADDR_T41 0x54

// The Arduino implimentation of ESP32 seems to require particular placement of classic BT code to enable the mode within the IDE.
// Placement within the ino sketch file works.  Within another file you may get compile errors such as:
// In member function 'void BluetoothSerial::memrelease()': error: 'ESP_BT_MODE_BTDM' was not declared in this scope;
//
// Also BT takes up a large amount of the ESP32 program memory, 84%.  WiFi Now takes up 68%.  You can't accomodate both.
// Simple BT sketch memory usage:
//    Sketch uses 1108725 bytes (84%) of program storage space. Maximum is 1310720 bytes.
//    Global variables use 39484 bytes (12%) of dynamic memory, leaving 288196 bytes for local variables. Maximum is 327680 bytes.
// Simple ESP-Now sketch memory usage:
//    Sketch uses 896685 bytes (68%) of program storage space. Maximum is 1310720 bytes.
//    Global variables use 43580 bytes (13%) of dynamic memory, leaving 284100 bytes for local variables. Maximum is 327680 bytes.
// Limited memory with these, so not as much need to have a more complex file structure.

// uncomment the desired communication mode
#define USE_CLASSIC_BT
//#define USE_ESP_NOW

#ifdef USE_CLASSIC_BT
#include "use_classic_bt.h"
#endif

#ifdef USE_ESP_NOW
#include "use_esp_now.h"
#endif

//===============================================================================
//  I2C
//===============================================================================

volatile uint8_t  i2cBuf[2048];
volatile int bufIn = 0;
int bufOut = 0;
volatile int totalReceived = 0;

uint32_t i = 0;

bool dataAvailable = false;
bool rhConnected = false;
bool btConnected = false;

void onRequest() {
  if(!rhConnected) {
    if(btConnected) {
      Wire.write(1);
      rhConnected = true;
    } else {
      Wire.write(0);
    }
  } else {
    //Wire.print(i++);
    //Wire.print(" Packets.");
    Serial.println("ESP_t41 received unknown request from T41...");
  }
}

void onReceive(int len) {
  //Serial.printf("onReceive[%d]\n", len);
  //int tmp = bufIn;
  if(rhConnected) {
    //Serial.printf("Received from T41_RH[%d]: ", len);
    //if(Wire.available()) Serial.print("Received from T41_RH: ");
    //while (Wire.available()) {
    for(int i = 0; i < len; i++) {
      // put data in buffer
      i2cBuf[bufIn] = Wire.read();
      //Serial.println(i2cBuf[bufIn]);
      bufIn++;
      if(bufIn >= 2028) {
        bufIn = 0;
      }
      //Serial.printf("%d, ", i2cBuf[bufIn-1]);
    }
    //totalReceived += len;
    //if(bufIn > tmp) {
    //  Serial.println("");
    //  i2cBuf[bufIn] = 0;
    //  dataAvailable = true;
    //}
  }
}

void callback(esp_spp_cb_event_t event, esp_spp_cb_param_t *param){
  switch(event) {
    case ESP_SPP_INIT_EVT:  // Enum 0 - When SPP is initialized
    case ESP_SPP_UNINIT_EVT:  // Enum 1 - When SPP is deinitialized
    case ESP_SPP_DISCOVERY_COMP_EVT:  // Enum 8 - When SDP discovery complete

    case ESP_SPP_OPEN_EVT:  // Enum 26 - When SPP Client connection open
      Serial.println("Client Connected");
      break;

    case ESP_SPP_CLOSE_EVT:  // Enum 27 - When SPP connection closed
      Serial.println("Client disconnected");
      break;

    case ESP_SPP_START_EVT:  // Enum 28 - When SPP server started
    case ESP_SPP_DATA_IND_EVT:  // Enum 30 - When SPP connection received data, only for ESP_SPP_MODE_CB
    case ESP_SPP_CONG_EVT:  // Enum 31 - When SPP connection congestion status changed, only for ESP_SPP_MODE_CB
    case ESP_SPP_WRITE_EVT:  // Enum 33 - When SPP write operation completes, only for ESP_SPP_MODE_CB
    case ESP_SPP_SRV_OPEN_EVT:  // Enum 34 - When SPP Server connection open
    case ESP_SPP_SRV_STOP_EVT:  // Enum 35 - When SPP server stopped
    case ESP_SPP_VFS_REGISTER_EVT:  // Enum 36 - When SPP VFS register
    case ESP_SPP_VFS_UNREGISTER_EVT:  // Enum 37 - When SPP VFS unregister
    default:
      Serial.print("Client event: "); Serial.println(event);
      break;
  }
}

//===============================================================================
//  Initialization
//===============================================================================

void setup() {
  Serial.begin(115200);

  // setup comms with T4.1
  Serial2.begin(115200, SERIAL_8N1, 16, 17);
  Serial.println("Beginning I2C (s) on T41 ESP32...");
  Wire.onReceive(onReceive);
  Wire.onRequest(onRequest);
  Wire.begin((uint8_t)I2C_DEV_ADDR_ESP32);

#if CONFIG_IDF_TARGET_ESP32
  char message[64];
  snprintf(message, 64, "%lu Packets.", i++);
  Wire.slaveWrite((uint8_t *)message, strlen(message));
#endif

#ifdef USE_ESP_NOW
  espnow_init();
#endif
#ifdef USE_CLASSIC_BT
  SerialBT.register_callback(callback);
  bt_setup();
#endif
}

uint8_t data[795];
int count = 0;
int fCount = 0;

//===============================================================================
//  Main
//===============================================================================
void loop() {
  //static uint8_t data = 0;

  if(Serial2.available()) {
    char command = Serial2.read();
    Serial.print("got T41 command "); Serial.println(command);
    switch(command) {
      case 'S': // connected
        //SerialBT.print("S");
        break;

      default:
        break;
    }
  }

  if(!btConnected && SerialBT.available()) {
    char command = SerialBT.read();
    Serial.print("got bt command "); Serial.println(command);
    switch(command) {
      case 'C': // connected
        btConnected = true;
        rhConnected = true;
        Serial2.print("C"); // connected
        break;

      default:
        break;
    }
  }

#ifdef USE_ESP_NOW
  // send via ESP_Now
  //SendToRemote(&data, 1);
  //data++;
#endif

#ifdef USE_CLASSIC_BT
  //if(btConnected && dataAvailable) {
  //if(btConnected) {
  if(btConnected) {
    while(bufOut != bufIn) {
      //Serial.write(Wire.read());
      //Serial.println((char *)i2cBuf[bufOut]);
      data[count++] = i2cBuf[bufOut++];
      if(bufOut >= 2028) {
        bufOut = 0;
      }
      if(count == 795) break;
    }
    //dataAvailable = false;

    if(count == 795) {
      Serial.printf("Sending data via BT: %d\n", ++fCount);
      //SerialBT.write(data, 518); // no delay needed

      // with both frequency and audio spectrums, fail
      SerialBT.write(data, 795);
      //delay(20);  // no delay misses a good chunk of audio spectrum; 10 delay gets 206 bytes of audio spectrum
                  // 20 didn't help

      // try splitting in two
      // 10 ms delay only transfered 65 bytes of second set
      // same for 20 ms
      //SerialBT.write(data, 518);
      //delay(100); // gets to 227 on audio data
      //delay(250);
      //delay(300);
      //SerialBT.write(&data[518], 273);
      //delay(20);
      //delay(100);
      //delay(250);

      count = 0;
      //totalReceived = 0;
    }
  }

  //if(btConnected) {
  //  Serial.println("Sending hello world...");
  //  SerialBT.println("hello world");
  //  delay(100);
  //}
#endif
}
