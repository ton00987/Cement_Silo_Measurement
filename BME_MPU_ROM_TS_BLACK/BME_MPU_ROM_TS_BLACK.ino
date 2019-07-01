#include "ThingSpeak.h"
#include <WiFi.h>
#include "MPU6050.h"
#include "Wire.h"
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <EEPROM.h>

#define AVG_SIZE 100
#define EEPROM_SIZE 512
#define SEALEVELPRESSURE_HPA (1013.25)
#define LED_RED 26
#define LED_GREEN 27
#define LED_BLUE 14

//char ssid[] = "POCOPHONE"; // your network SSID (name)
//char pass[] = "0805853144"; // your network password
char ssid[] = "Krittanat"; // your network SSID (name)
char pass[] = "gumehotspot"; // your network password
//char ssid[] = "Leko_Smart_Farm_5G"; // your network SSID (name)
//char pass[] = "lnwzaleko"; // your network password
int keyIndex = 0; // your network key Index number (needed only for WEP)

WiFiClient client;

unsigned long myChannelNumber = 000000; //update
const char * myWriteAPIKey = "FE8FGBAVLZPIM51Z"; //update

float VaPut[8] = {};
float VaGet[8] = {};

int rom_status;
int saveValues(int z = 0);

MPU6050 accelgyro(0x69);
int16_t ax, ay, az;
int16_t gx, gy, gz;

unsigned long start_time = 0;
unsigned long stop_time = 0;
unsigned long capture_time;

float gravity = 9.80665;
float f_ax, f_ay, f_az;
float gax, gay, gaz;
float height;
float sum = gravity;

float avgTemperature;
float avgPressure;
float avgAltitude;
float sumTemp = 0;
float sumPress = 0;
float sumAlti = 0;

Adafruit_BME280 bme; // I2C
TwoWire bmeTwoWire = TwoWire(0);

void setup()
{
  Wire.begin();
  EEPROM.begin(EEPROM_SIZE);
  Serial.begin(115200); //Initialize serial
  bmeTwoWire.begin(16, 17);
  while (!bme.begin(&bmeTwoWire)) {
    //  while (!bme.begin(0x76)) {
    Serial.println("Could not find a valid bme280 sensor, check wiring!");
  }
  // initialize device
  Serial.println("Initializing I2C devices...");
  accelgyro.initialize();
  // verify connection
  Serial.println("Testing device connections...");
  Serial.println(accelgyro.testConnection() ? "MPU6050 connection successful" : "MPU6050 connection failed");
  delay(10);
  WiFi.mode(WIFI_STA);
  ThingSpeak.begin(client); // Initialize ThingSpeak

  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);
  digitalWrite(LED_RED, HIGH);
  digitalWrite(LED_GREEN, HIGH);
  digitalWrite(LED_BLUE, HIGH);
}

void loop() {
  if (romReady() == true) {
    delay(10000);
    readandprintbme();

    VaPut[0] = sumTemp / AVG_SIZE;
    VaPut[1] = sumPress / AVG_SIZE;
    VaPut[2] = sumAlti / AVG_SIZE;
    Serial.print("Average Temperature = ");
    Serial.print(VaPut[0]);
    Serial.println(" *C");
    Serial.print("Pressure = ");
    Serial.print(VaPut[1]);
    Serial.println(" Pa");
    Serial.print("Approx. Altitude = ");
    Serial.print(VaPut[2]);
    Serial.println(" m");
    Serial.println();

    Serial.println("Save high ground bme succcess!");
    Serial.println();
    digitalWrite(LED_RED, HIGH);
    digitalWrite(LED_GREEN, LOW);
    digitalWrite(LED_BLUE, HIGH);

    readandprintmpu();
    VaPut[7] = height;

    delay(5000);

    readandprintbme();

    VaPut[3] = sumTemp / AVG_SIZE;
    VaPut[4] = sumPress / AVG_SIZE;
    VaPut[5] = sumAlti / AVG_SIZE;
    Serial.print("Average Temperature = ");
    Serial.print(VaPut[3]);
    Serial.println(" *C");
    Serial.print("Pressure = ");
    Serial.print(VaPut[4]);
    Serial.println(" Pa");
    Serial.print("Approx. Altitude = ");
    Serial.print(VaPut[5]);
    Serial.println(" m");
    Serial.println();

    Serial.println("Save low ground bme succcess!");
    Serial.println();
    digitalWrite(LED_RED, HIGH);
    digitalWrite(LED_GREEN, HIGH);
    digitalWrite(LED_BLUE, LOW);
    Serial.print("Height diff: ");
    Serial.print(VaPut[2] - VaPut[5]);
    Serial.println(" m");
    Serial.println();
    VaPut[6] = VaPut[2] - VaPut[5];

    saveValues();

  }
  else {
    getdata();
  }
}

void getdata() {
  // Connect or reconnect to WiFi
  if (WiFi.status() != WL_CONNECTED) {
    Serial.print("Attempting to connect to SSID: ");
    //Serial.println(SECRET_SSID);
    while (WiFi.status() != WL_CONNECTED) {
      WiFi.begin(ssid, pass); // Connect to WPA/WPA2 network. Change this line if using open or WEP network
      Serial.print(".");
      delay(10000);
    }
    Serial.println("\nConnected.");
  }
  readValues();
  // Write to ThingSpeak. There are up to 8 fields in a channel, allowing you to store up to 8 different
  // pieces of information in a channel.
  ThingSpeak.setField(1, VaGet[0]); // Temp HIGH
  ThingSpeak.setField(2, VaGet[3]); // Temp LOW
  ThingSpeak.setField(3, VaGet[1]); // Pressure HIGH
  ThingSpeak.setField(4, VaGet[4]); // Pressure LOW
  ThingSpeak.setField(5, VaGet[2]); // Altitude HIGH
  ThingSpeak.setField(6, VaGet[5]); // Altitude LOW
  ThingSpeak.setField(7, VaGet[6]); // BME280_HEIGHT
  ThingSpeak.setField(8, VaGet[7]); // MPU6050_HEIGHT

  // write to the ThingSpeak channel
  int x = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
  if (x == 200) {
    digitalWrite(14, HIGH);
    digitalWrite(26, LOW);
    Serial.println("Channel update successful.");
    Serial.println();
    //      Serial.println("EEPROM.length() " + String(EEPROM.length()));
    for (int i = 0 ; i < EEPROM_SIZE ; i++) {
      EEPROM.put(i, 0);
    }
    Serial.println("Clear ROM");
    digitalWrite(LED_RED, LOW);
    digitalWrite(LED_GREEN, HIGH);
    digitalWrite(LED_BLUE, HIGH);
  }
  else {
    Serial.println("Problem updating channel. HTTP error code " + String(x));
    Serial.println();
  }
  delay(5000); // Wait 20 seconds to update the channel again
}


void calculator() {
  accelgyro.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);
  f_ax = ax;
  f_ay = ay;
  f_az = az;
  gax = (f_ax / 16384) * gravity;
  gay = (f_ay / 16384) * gravity;
  gaz = (f_az / 16384) * gravity;
  sum = sqrt(pow(gax, 2) + pow(gay, 2) + pow(gaz, 2));
  //  Serial.print("Sum = ");
  //  Serial.println(sum);
}

void readandprintmpu() {
  while (!(sum < 5)) {
    calculator();
  }
  start_time = millis();
  Serial.println("Falling");
  while (sum < 20) {
    calculator();
  }
  stop_time = millis();
  capture_time = stop_time - start_time;
  height = (pow(capture_time * 0.001, 2) * gravity) / 2;
  Serial.print("start_time: ");
  Serial.println(start_time);
  Serial.print("stop_time: ");
  Serial.println(stop_time);
  Serial.print("Capture time = ");
  Serial.println(capture_time);
  Serial.print("Height = ");
  Serial.print(height);
  Serial.println(" m");
  Serial.println();

}

int saveValues(int z) {
  int size_data = z;
  for (int i = 0; i < (sizeof(VaPut) / sizeof(VaPut[0])); i++) {
    EEPROM.put(size_data, VaPut[i]);
    size_data += sizeof(VaPut[i]);
  }
  return size_data;
}

void readValues() {
  int size_data = 0;
  for (int i = 0; i < (sizeof(VaGet) / sizeof(VaGet[0])); i++) {
    EEPROM.get(size_data, VaGet[i]);
    size_data += sizeof(VaGet[i]);
    //    Serial.println(VaGet[i]);
  }
}

bool romReady() {
  EEPROM.get(0, rom_status);
  if (rom_status == 0) {
    return true;
  }
  else {
    return false;
  }
}

void readandprintbme() {
  sumTemp = 0;
  sumPress = 0;
  sumAlti = 0;
  for (int i = 0; i < AVG_SIZE; i++) {
    //    Serial.print("bme.readTemperature = ");
    //    Serial.println(bme.readTemperature());
    //    Serial.print("bme.readPressure = ");
    //    Serial.println(bme.readPressure());
    //    Serial.print("bme.readAltitude = ");
    //    Serial.println(bme.readAltitude(SEALEVELPRESSURE_HPA));
    sumTemp += bme.readTemperature();
    sumPress += bme.readPressure();
    sumAlti += bme.readAltitude(SEALEVELPRESSURE_HPA);
  }

}
