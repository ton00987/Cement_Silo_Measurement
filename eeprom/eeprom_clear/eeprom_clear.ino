#include "EEPROM.h"

#define EEPROM_SIZE 512

float data_;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  EEPROM.begin(EEPROM_SIZE);
}

void loop() {
  // put your main code here, to run repeatedly:
  for (int i = 0 ; i < EEPROM_SIZE ; i++) {
    EEPROM.put(i, 0);
  }
  Serial.println("Clear ROM");

  for (int i = 0 ; i < EEPROM_SIZE ; i++) {
    EEPROM.get(i, data_);
    Serial.println(data_);
  }
}
