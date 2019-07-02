#include "EEPROM.h"

#define EEPROM_SIZE 512

float data_;

void setup() {
  Serial.begin(115200);
  EEPROM.begin(EEPROM_SIZE);
}

void loop() {
  for (int i = 0 ; i < EEPROM_SIZE ; i++) {
    EEPROM.get(i, data_);
    Serial.println(data_);
  }
}
