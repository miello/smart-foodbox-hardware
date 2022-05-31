#include "esp_camera.h"
#include <Wire.h>
#include <Arduino.h>
#include <HTTPClient.h>
#include <base64.h>

//
// WARNING!!! PSRAM IC required for UXGA resolution and high JPEG quality
//            Ensure ESP32 Wrover Module or other board with PSRAM is selected
//            Partial images will be transmitted if image exceeds buffer size
//

// Select camera model
//#define CAMERA_MODEL_WROVER_KIT // Has PSRAM
//#define CAMERA_MODEL_ESP_EYE // Has PSRAM
//#define CAMERA_MODEL_M5STACK_PSRAM // Has PSRAM
//#define CAMERA_MODEL_M5STACK_V2_PSRAM // M5Camera version B Has PSRAM
//#define CAMERA_MODEL_M5STACK_WIDE // Has PSRAM
//#define CAMERA_MODEL_M5STACK_ESP32CAM // No PSRAM
#define CAMERA_MODEL_AI_THINKER // Has PSRAM
//#define CAMERA_MODEL_TTGO_T_JOURNAL // No PSRAM

#include "camera_pins.h"

const char* ssid = "********";
const char* password = "********";
const int LED_PIN = 33;
const int FLASH_LIGHT_PIN = 4;
const int FLASH_INPUT_PIN = 15;
const int SDA_PIN = 12;
const int SCK_PIN = 13;
const char* endpoint = "<API URL>";
void startCameraServer();

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();
  pinMode(LED_PIN, OUTPUT);
  pinMode(FLASH_LIGHT_PIN, OUTPUT);
  pinMode(FLASH_INPUT_PIN, INPUT);
  Wire.begin(SDA_PIN, SCK_PIN);
  
  for (uint8_t t = 4; t > 0; t--) {
    Serial.printf("[SETUP] WAIT %d...\n", t);
    Serial.flush();
    delay(1000);
  }

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  
  // if PSRAM IC present, init with UXGA resolution and higher JPEG quality
  //                      for larger pre-allocated frame buffer.
  if(psramFound()){
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }
  digitalWrite(LED_PIN, HIGH); 

#if defined(CAMERA_MODEL_ESP_EYE)
  pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);
#endif

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  sensor_t * s = esp_camera_sensor_get();
  // initial sensors are flipped vertically and colors are a bit saturated
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1); // flip it back
    s->set_brightness(s, 1); // up the brightness just a bit
    s->set_saturation(s, -2); // lower the saturation
  }
  // drop down frame size for higher initial frame rate
  s->set_framesize(s, FRAMESIZE_QVGA);

#if defined(CAMERA_MODEL_M5STACK_WIDE) || defined(CAMERA_MODEL_M5STACK_ESP32CAM)
  s->set_vflip(s, 1);
  s->set_hmirror(s, 1);
#endif

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");

//  startCameraServer();
//
//  Serial.print("Camera Ready! Use 'http://");
//  Serial.print(WiFi.localIP());
//  Serial.println("' to connect");
}


void loop() {
  // put your main code here, to run repeatedly:
  digitalWrite(FLASH_LIGHT_PIN, digitalRead(FLASH_INPUT_PIN));
  
  // request size of data
  Wire.requestFrom(1, 2);
  
  if(!Wire.available()) {
    Serial.println("Cannot reach STM32");
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
  camera_fb_t * fb = esp_camera_fb_get();
  while(fb->len == 0) {
    fb = esp_camera_fb_get();
    delay(10);
  }
  
  if ((WiFi.status() == WL_CONNECTED)) {
  
    WiFiClientSecure client;
    client.setInsecure();
  
    HTTPClient http;

    Serial.print("[HTTP] begin...\n");
    
    if(client.connect(base_url, 443)) {
      String getAll;
      String getBody;
      
      String start_request_img = "", end_request = "", start_request_weight = "";
      start_request_img = start_request_img + "--WeightSensing\r\nContent-Disposition: form-data; name=\"esp32-cam\"; filename=\"Captured.JPG\"; size=" + fb->len + "\r\nContent-Type: image/jpeg\r\n\r\n";
      end_request = end_request + "\r\n--WeightSensing";
      
      start_request_weight = start_request_weight + "Content-Disposition: form-data; name=\"weight\" \r\n\r\n";
      
      uint16_t full_length;
      full_length = start_request_img.length() + fb->len + end_request.length() + start_request_weight.length() + ch_sz.length() + end_request.length() + 6;
      client.println("POST /api/weightChange HTTP/1.1");
      client.println("Host: " + String(base_url));
      client.println("Content-Length: " + String(full_length));
      client.println("Content-Type: multipart/form-data; boundary=WeightSensing");
      client.println();
      client.print(start_request_img);

      uint8_t *fbBuf = fb->buf;
      size_t fbLen = fb->len;
      for (size_t n=0; n<fbLen; n=n+1024) {
        if (n+1024 < fbLen) {
          client.write(fbBuf, 1024);
          fbBuf += 1024;
        }
        else if (fbLen%1024>0) {
          size_t remainder = fbLen%1024;
          client.write(fbBuf, remainder);
        }
      }

      client.println(end_request);
      client.print(start_request_weight);
      client.println(ch_sz);
      client.print(end_request + "--");
    
      esp_camera_fb_return(fb);
      
      int timoutTimer = 10000;
      long startTimer = millis();
      boolean state = false;
      
      while ((startTimer + timoutTimer) > millis()) {
        Serial.print(".");
        delay(100);      
        while (client.available()) {
          char c = client.read();
          if (c == '\n') {
            if (getAll.length()==0) { state=true; }
            getAll = "";
          }
          else if (c != '\r') { getAll += String(c); }
          if (state==true) { getBody += String(c); }
          startTimer = millis();
        }
        if (getBody.length()>0) { break; }
      }
      Serial.println();
      client.stop();
      Serial.println(getBody);
    } else {
      Serial.printf("[HTTP} Unable to connect\n");
    }
  }

  esp_camera_fb_return(fb);
  delay(500);
}
