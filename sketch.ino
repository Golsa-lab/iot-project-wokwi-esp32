#include <WiFi.h>
#include <esp_now.h>
#include "esp_sleep.h" //library included to use deep sleep functions

#define PIR_PIN 13 //digital input connected to the PIR output
#define LDR_PIN 34 //port 34 is part of ADC1 so it functions correctly while wifi is active

uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

esp_now_peer_info_t peerInfo;

const float SLEEP_TIME_S = 5.4; //my personal code :10821199 -> X=5.4
const uint64_t SLEEP_TIME_US = 5400000ULL;

unsigned long t0, t1, t2, t3; //I use unsigned long because micros() returns a large integer value

void GoToSleep() {
  esp_sleep_enable_timer_wakeup(SLEEP_TIME_US);
  Serial.println("Going to deep sleep...");
  Serial.flush(); //for avoiding to lose messages forces to get the output before the node goes to sleep
  esp_deep_sleep_start();
}

void OnDataSent(const wifi_tx_info_t* info, esp_now_send_status_t status) {
  Serial.print("Send status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "ok" : "error");
}

void setup() {

  // one full sensing/transmission cycle is executed in setup();
  // after deep sleep, execution restarts from setup()

  t0 = micros(); //the moment the program starts
  Serial.begin(115200);
  delay(200);

  pinMode(PIR_PIN, INPUT); //read what you get from port 13 as an input

  //Wifi is set on station mode:
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  //if ESP-NOW initialization fails, print an error and go to deep sleep
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP_NOW initialization failed");
    delay(1000);
    GoToSleep();
  }

  esp_now_register_send_cb(OnDataSent);

  //now i configure the peer:

  memset(&peerInfo, 0, sizeof(peerInfo));
  memcpy(peerInfo.peer_addr, broadcastAddress, 6); //assign the broadcast address as the peer MAC address
  peerInfo.channel = 0; //use the current Wi-Fi channel or default behavior
  peerInfo.encrypt = false; // no encrypting


  //if you can't add the peer successfully go to sleep for the next cycle
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("failed to add peer");
    delay(1000);
    GoToSleep();
  }

  t1 = micros(); //end of initializing

  // now i read the sensors :
  int motion = digitalRead(PIR_PIN);   // high=1 low =0
  int light = analogRead(LDR_PIN);   // raw ADC value, usually in the range 0..4095

  // approximate conversion from ADC value to lux
  // this is only a simplified estimation for simulation purposes
  int apxlux = map(light, 0, 4095, 0, 1000);


  //creating a data packet that combines the motion status and light value
  // the packet will be sent by ESP32
  String msg;
  if (motion == HIGH) {
    msg = "MOTION_DETECTED-LUX:" + String(apxlux);
  } else {
    msg = "MOTION_NOT_DETECTED-LUX:" + String(apxlux);
  }

  t2 = micros(); //sensor reading time finished

  Serial.println(msg); //reading the motion and light status


  //send packet
  esp_err_t result = esp_now_send(broadcastAddress, (uint8_t*)msg.c_str(), msg.length() + 1);

  if (result == ESP_OK) {
    Serial.println("Message queued correctly");
  } else {
    Serial.println("ERROR while sending the message");
  }

  delay(200); // time for callback and completing the sending process

  t3 = micros(); //time after the sending phase

  Serial.println("Summary of timing results: [us]"); //bc micros() is in microseconds

  Serial.print("Initialization phase: ");
  Serial.println(t1 - t0);

  Serial.print("Sensor reading + packet creation: ");
  Serial.println(t2 - t1);

  Serial.print("Transmission phase: ");
  Serial.println(t3 - t2);

  Serial.print("Configured deep sleep time : ");
  Serial.println((unsigned long)(SLEEP_TIME_S * 1000000.0));

  //going to deep sleep:
  GoToSleep();
}

void loop() {
  //we technically don't reach this part because the device performs one sensing/transmission cycle,
  //then enters deep sleep;
  //after wake-up, execution starts again from setup()
}
