// ESP32 Bluetooth library:
//  https://github.com/espressif/arduino-esp32/tree/master/libraries/BluetoothSerial

// ESP32 Bluetooth examples:
// from: https://github.com/espressif/arduino-esp32/blob/master/libraries/BluetoothSerial/examples/SerialToSerialBTM/SerialToSerialBTM.ino
// from: https://github.com/espressif/arduino-esp32/blob/master/libraries/BluetoothSerial/examples/SerialToSerialBT/SerialToSerialBT.ino
// other bt examples https://github.com/espressif/esp-idf/tree/23e4823f17a8349b5e03536ff7653e3e584c9351/examples/bluetooth

#include "BluetoothSerial.h"

// Check if Bluetooth is available
#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif

// Check Serial Port Profile
#if !defined(CONFIG_BT_SPP_ENABLED)
#error Serial Port Profile for Bluetooth is not available or not enabled. It is only available for the ESP32 chip.
#endif

//===============================================================================
//  Data
//===============================================================================

// Bluetooth Mac Addresses
// NodeMCU ESP32-S Development Boards
uint8_t rhBTMacAddress[6] = {0xC0, 0x49, 0xEF, 0xA8, 0x83, 0x8E};  // in Project System; WiFi base: C0:49:EF:A8:83:8C
//uint8_t t41BTMacAddress[6] = {0xC4, 0xDE, 0xE2, 0x17, 0xB4, 0x36}; // in 3.2" ProtoTyping System; Wifi base: C4:DE:E2:17:B4:34
uint8_t t41BTMacAddress[6] = {0xE4, 0x65, 0xB8, 0x71, 0x9B, 0x86}; // in T41; Wifi base: E4:65:B8:71:9B:84

// ESP32_DevKitC_V4
//uint8_t t41BTMacAddress[6] = {0x08, 0x3A, 0x8D, 0x14, 0x7F, 0x5E}; // base: 08:3A:8D:14:7F:5C

const int LED_PIN = 2;  // We will light the LED when Scan is in process.

String device_name = "ESP32-BT-Slave";

BluetoothSerial SerialBT;

//===============================================================================
//  Code
//===============================================================================

void bt_setup() {
  SerialBT.begin(device_name);  //Bluetooth device name
  //SerialBT.deleteAllBondedDevices(); // Uncomment this to delete paired devices; Must be called after begin
  Serial.printf("The device with name \"%s\" is started.\nNow you can pair it with Bluetooth!\n", device_name.c_str());
  pinMode(LED_PIN, OUTPUT);
}

void bt_loop() {
  digitalWrite(LED_PIN, HIGH);   // turn the LED on
  if (Serial.available()) {
    SerialBT.write(Serial.read());
  }
  if (SerialBT.available()) {
    Serial.write(SerialBT.read());
  }
  delay(20);
  digitalWrite(LED_PIN, LOW);    // turn the LED off
}
