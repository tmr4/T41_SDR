// ESP-Now
// see: https://github.com/espressif/esp-now and https://github.com/espressif/esp-idf/tree/master/examples/wifi/espnow

#include <esp_now.h>
#include <WiFi.h>

//===============================================================================
//  Data
//===============================================================================

// Mac Addresses
/*
  // find mac address
  #include <WiFi.h>
  uint8_t baseMac[6];
  WiFi.mode(WIFI_STA);
  WiFi.STA.begin();
  Serial.println(WiFi.macAddress());
*/
// Project System ESP32
uint8_t rhBTMacAddress[6] = {0xC0, 0x49, 0xEF, 0xA8, 0x83, 0x8E};  // base: C0:49:EF:A8:83:8C

// ESP32_DevKitC_V4
//uint8_t t41BTMacAddress[6] = {0x08, 0x3A, 0x8D, 0x14, 0x7F, 0x5E}; // base: 08:3A:8D:14:7F:5C

// ProtoTyping System ESP32
uint8_t t41BTMacAddress[6] = {0xC4, 0xDE, 0xE2, 0x17, 0xB4, 0x36}; // base: C4:DE:E2:17:B4:34

const int LED_PIN = 2;  // We will light the LED when Scan is in process.

//===============================================================================
//  Forwards
//===============================================================================

void espnow_deinit();

//===============================================================================
//  Code
//===============================================================================

void espnow_send_cb(const uint8_t *mac_addr, esp_now_send_status_t status) {
  digitalWrite(LED_PIN, HIGH);   // turn the LED on
  Serial.print("\r\nT41 sent data - Status:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
  digitalWrite(LED_PIN, LOW);    // turn the LED off
}

void espnow_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
  Serial.print("\r\nT41 received from Remote Head:\t");
  Serial.println(*data);
}

void espnow_init() {
  esp_now_peer_info_t *peer;

  WiFi.mode(WIFI_STA);

  if(esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW: initialization error");
    return;
  }

  // register send and receive callback functions
  esp_now_register_send_cb(espnow_send_cb);
  esp_now_register_recv_cb(espnow_recv_cb);

  // add broadcast peer information to peer list
  peer = (esp_now_peer_info_t *)malloc(sizeof(esp_now_peer_info_t));
  if(peer == NULL) {
    Serial.println("ESP-NOW: peer malloc error");
    esp_now_deinit();
    return;
  }
  memset(peer, 0, sizeof(esp_now_peer_info_t));
  //peer->channel = CONFIG_ESPNOW_CHANNEL;
  peer->channel = 0;
  //peer->ifidx = ESPNOW_WIFI_IF;
  peer->encrypt = false;
  memcpy(peer->peer_addr, rhMacAddress, 6);
  if(esp_now_add_peer(peer) != ESP_OK) {
    Serial.println("ESP-NOW: peer add error");
    esp_now_deinit();
    return;
  }
  pinMode(LED_PIN, OUTPUT);
}

void espnow_deinit() {
  esp_now_deinit();
}

esp_err_t SendToRemote(uint8_t *data, int len) {
  return esp_now_send(rhMacAddress, data, len);
}
