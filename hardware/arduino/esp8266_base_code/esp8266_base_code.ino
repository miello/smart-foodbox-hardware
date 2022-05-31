#include <Wire.h>
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>

ESP8266WiFiMulti WiFiMulti;

// Replace with your network credentials
const char* ssid = "********";
const char* password = "********";
const char* endpoint = "********";

const int ledPin = 4;
const int extLedPin = D8;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  Wire.begin(D5, D4);

  for (uint8_t t = 4; t > 0; t--) {
    Serial.printf("[SETUP] WAIT %d...\n", t);
    Serial.flush();
    delay(1000);
  }
  
  // Connect to Wi-Fi
  WiFi.mode(WIFI_STA);
  WiFiMulti.addAP(ssid, password);

  // Print ESP Local IP Address
  Serial.println(WiFi.localIP());
}

void loop() {
  // put your main code here, to run repeatedly:
  // request size of data
  Wire.requestFrom(1, 2);
  
  if(!Wire.available()) {
    delay(250);  
    return;
  }

  String ch_sz = "";
  while(Wire.available()) {
    char c = Wire.read();
    ch_sz += c;
    delay(100);
  }
  
  Serial.println();
  
  // request data
  int dat_sz = (ch_sz[0] - '0') * 10 + (ch_sz[1] - '0');
  Serial.println(dat_sz);
  Wire.requestFrom(1, dat_sz);

  if(!Wire.available()) {
    delay(10);  
    return;
  }

  ch_sz = "";
  while(Wire.available()) {
    char c = Wire.read();
    ch_sz += c;
    delay(100);
  }

  Serial.println(ch_sz);

  if(ch_sz == "Test") return;
  
  if ((WiFiMulti.run() == WL_CONNECTED)) {
  
    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;

    Serial.print("[HTTP] begin...\n");
    if (http.begin(client, endpoint)) {  // HTTP
      Serial.print("[HTTP] POST Testing\n");
      http.addHeader("Content-Type", "application/json");
      // start connection and send HTTP header
      
      int httpCode = http.POST("{\"weight\": \"" + ch_sz + "\", \"img\": \"0\" }");

      // httpCode will be negative on error
      if (httpCode > 0) {
        // HTTP header has been send and Server response header has been handled
        Serial.printf("[HTTP] POST result: code: %d\n", httpCode);

        // file found at server
        if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
          String payload = http.getString();
          Serial.println(payload);
        }
      } else {
        Serial.printf("[HTTP] POST... failed, error: %s\n", http.errorToString(httpCode).c_str());
      }

      http.end();
    } else {
      Serial.printf("[HTTP} Unable to connect\n");
    }
  }
  delay(500);
}
