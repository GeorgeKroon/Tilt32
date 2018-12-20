#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <BLEBeacon.h>
#define ENDIAN_CHANGE_U16(x) ((((x)&0xFF00)>>8) + (((x)&0xFF)<<8))

BLEScan* pBLEScan;
int scanTime = 5;

struct tilt {
    int index;
    String uuid;
    String color;
    int gravity;
    int temperature;
};
const int tiltCount = 8;
tilt tilt_array[] = {
  {0, "de742df0-7013-12b5-444b-b1c510bb95a4", "red"},
  {1, "de742df0-7013-12b5-444b-b1c520bb95a4", "green"},
  {2, "de742df0-7013-12b5-444b-b1c530bb95a4", "black"},
  {3, "de742df0-7013-12b5-444b-b1c540bb95a4", "purple"},
  {4, "de742df0-7013-12b5-444b-b1c550bb95a4", "orange"},
  {5, "de742df0-7013-12b5-444b-b1c560bb95a4", "blue"},
  {6, "de742df0-7013-12b5-444b-b1c570bb95a4", "yellow"},
  {7, "de742df0-7013-12b5-444b-b1c580bb95a4", "pink"}
};

#if OLED_LCD
#include "SSD1306Wire.h"
#define MAX_OUT_CHARS 16 
char buffer[MAX_OUT_CHARS + 1]; 
SSD1306Wire  display(0x3c, 5, 4);

int displayTime = 25;
const int displayTimeReset = 25;
int currentTilt = 0;

void oledTask(void *pvParameters){
  while (1){
    Serial.printf("DisplayTime: %d\n", displayTime);
    Serial.printf("currentTilt: %d\n", currentTilt);
    Serial.println("-----------");
    if(displayTime == 0){
      displayTime = displayTimeReset;
      if(currentTilt >= (tiltCount-1)){
        currentTilt = 0;
      } else {
        currentTilt = currentTilt+1;
      }
      int attempt = 0;
      while(tilt_array[currentTilt].gravity == NULL){
        if(currentTilt >= (tiltCount-1)){
          currentTilt = 0;
        } else {
          currentTilt = currentTilt+1;
        }
        attempt = attempt+1;
        if(attempt >= 10){
          Serial.printf("No tilts found\n");
          break;
        }
      };
    }else{
      displayTime = displayTime-1;
    }
    
    tilt tiltObject = tilt_array[currentTilt];

    if(tiltObject.gravity != NULL){
      display.clear();
      display.setFont(ArialMT_Plain_16);
      display.setTextAlignment(TEXT_ALIGN_LEFT);
      display.drawString(0, 5, " Tilt: " + tiltObject.color);
      sprintf(buffer,"Gravity: %d",tiltObject.gravity);  
      display.drawString(0, 23, buffer);
      sprintf(buffer,"Temperature: %d",tiltObject.temperature);  
      display.drawString(0, 40, buffer);
      display.display();
    }
		vTaskDelay(500 / portTICK_PERIOD_MS);
  };
}
#endif

class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
      //Serial.printf("Advertised Device: %s \n", advertisedDevice.toString().c_str());
      if (advertisedDevice.haveManufacturerData()==true) {
        std::string strManufacturerData = advertisedDevice.getManufacturerData();
        
        uint8_t cManufacturerData[100];
        strManufacturerData.copy((char *)cManufacturerData, strManufacturerData.length(), 0);
        
        if (strManufacturerData.length()==25 && cManufacturerData[0] == 0x4C  && cManufacturerData[1] == 0x00 ) {
          BLEBeacon oBeacon = BLEBeacon();
          oBeacon.setData(strManufacturerData);
          
          //Serial.printf("ID: %04X Major: %d Minor: %d UUID: %s Power: %d\n",oBeacon.getManufacturerId(),ENDIAN_CHANGE_U16(oBeacon.getMajor()),ENDIAN_CHANGE_U16(oBeacon.getMinor()),oBeacon.getProximityUUID().toString().c_str(),oBeacon.getSignalPower());
          
          for(auto tiltObject: tilt_array) {
            String stringOne = String(oBeacon.getProximityUUID().toString().c_str());
            String stringTwo = tiltObject.uuid;

            if(stringTwo.equals(stringOne)){
              Serial.printf("%s Tilt Frame\n", tiltObject.color.c_str());
              Serial.printf("Gravity: %d\n", ENDIAN_CHANGE_U16(oBeacon.getMinor()));
              Serial.printf("Temperature: %d\n", ENDIAN_CHANGE_U16(oBeacon.getMajor()));
              Serial.println("-----------");
              tiltObject.gravity = ENDIAN_CHANGE_U16(oBeacon.getMinor());
              tiltObject.temperature = ENDIAN_CHANGE_U16(oBeacon.getMajor());
              //tilt[2] = std::to_string(ENDIAN_CHANGE_U16(oBeacon.getMinor()));
              tilt_array[tiltObject.index] = tiltObject;
            }
          }
        }
      }
    }
};

void setup() {
  Serial.begin(115200);

  #if OLED_LCD

  if (display.init()) {
		Serial.println("oled init done");
    display.clear();
    display.flipScreenVertically();
    display.setFont(ArialMT_Plain_16);
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.drawString(0, 5, "Waiting for");
    display.drawString(0, 23, "Tilt beacons");
    display.display();
    xTaskCreatePinnedToCore(&oledTask, "oledTask", 2048, NULL, 5, NULL, 1);
	} else {
		Serial.println("oled init failed");
	}
  #endif

  BLEDevice::init("");
  pBLEScan = BLEDevice::getScan(); //create new scan
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);

  
}

void loop() {
  BLEScanResults foundDevices = pBLEScan->start(scanTime, false);
  pBLEScan->clearResults();
  delay(2000);
}

