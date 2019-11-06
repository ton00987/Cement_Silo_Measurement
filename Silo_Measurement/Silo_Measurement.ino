/*
  rtc not connected
  time in thingspeak
  Demo   : Silo measurement
  VERSION: 0.1 (2/8/2019)

  MCU    : ESP32(ESP-WROOM-32)

  Output : Wifi
         : BLYNK
         : ThingSpeak

  Demo   : BLYNK -> "SILO MEASUREMENT"
         : Blynk library V0.6.1
         : BME280
         : MPU6050
         :
  Widget : RESET_B_pin              V1
         : Widget_HEIGHT_BME        V5
         : Widget_HEIGHT_MPU        V6
         : Widget_Terminal          V7
         : Widget_LED_ORANGE_Status V13
         : Widget_LED_RED_Status    V14
         : Widget_LED_GREEN_Status  V27
         : Widget_LED_BLUE_Status   V26
*/

#include <Wire.h>
//#include <task.h>
//UBaseType_t uxTaskGetNumberOfTasks( void );
#include <ThingSpeak.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <BlynkSimpleEsp32.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
//#include <Adafruit_BMP280.h>
#include <MPU6050.h>
#include <EEPROM.h>
#include "time.h"

// *** LED_PIN ***
#define LED_RED 26
#define LED_GREEN 27
#define LED_BLUE 14

// *** BME280 ***
#define SEALEVELPRESSURE_HPA (1013.25)
#define AVG_SIZE 100
float avgTemperature;
float avgPressure;
float avgAltitude;
float sumTemp = 0;
float sumPress = 0;
float sumAlti = 0;
float upAlti = 0;
float lowAlti = 0;
Adafruit_BME280 bme;
TwoWire bmeTwoWire = TwoWire(0);

// *** MPU6050 ***
MPU6050 mpu1(0x68);
MPU6050 mpu2(0x69);
int16_t ax, ay, az;
int16_t gx, gy, gz;
unsigned long start_time1 = 0;
unsigned long stop_time1 = 0;
unsigned long capture_time1;
unsigned long start_time2 = 0;
unsigned long stop_time2 = 0;
unsigned long capture_time2;
float gravity = 9.80665;
float f_ax, f_ay, f_az;
float gax, gay, gaz;
float height1;
float height2;
float sum1 = gravity;
float sum2 = gravity;
int count1_st, count1_sp;
int count2_st, count2_sp;
int timer1[3] = {0, 0, 0};
int timer2[3] = {0, 0, 0};
int timer_o_1[3] = {0, 0, 0};
int timer_o_2[3] = {0, 0, 0};

TaskHandle_t mpuTask = NULL;

int state = 0;
int state1 = 0;
int state2 = 0;

// *** Wifi ***
WiFiClient client;
char ssid[] = "Krittanat";  // your network SSID (name)
char pass[] = "passwordrai"; // your network password
int WifiSignal;
int CurrentWiFiSignal;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

// *** Blynk ***
char auth[] = "S4ZIndHj4QdQdeOU_qS01ACFQ6pXsFIr";
#define BLYNK_PRINT Serial
#define RESET_B_pin V1
#define Widget_HEIGHT_BME V5
#define Widget_HEIGHT_MPU V6
#define Widget_Terminal V7
#define Widget_Battery V8
#define Widget_WifiSignal V10
#define Widget_WifiRawValue V11
#define Widget_LED_ORANGE_Status V13
#define Widget_LED_RED_Status V14
#define Widget_LED_GREEN_Status V27
#define Widget_LED_BLUE_Status V26
WidgetLED LED_ORANGE_B(Widget_LED_ORANGE_Status);
WidgetLED LED_RED_B(Widget_LED_RED_Status);
WidgetLED LED_GREEN_B(Widget_LED_GREEN_Status);
WidgetLED LED_BLUE_B(Widget_LED_BLUE_Status);
WidgetTerminal terminal(Widget_Terminal);
BlynkTimer timer;

// *** EEPROM ***
#define EEPROM_SIZE 512
int addr;
int rom_status;
int a;
int len;
int saveValues(int z = 0);
float VaPut[7] = {};
String VaGet[680] = {};   // (EEPROM Max size // bytes for 1 dataset) * number of data in dataset = (4096//48)*8


// *** Time ***
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 0;
const int   daylightOffset_sec = 0;

// *** ThingSpeak ***
unsigned long myChannelNumber = 000000;         //update
const char *myWriteAPIKey = "FE8FGBAVLZPIM51Z"; //update
int keyIndex = 0;                               // your network key Index number (needed only for WEP)

// *** Battery ***
long readBatTimer = -60000; // Timer For Read Battery
long batVal = 0;            // Get Battery Value
#define battery_adc 35      // Battery Read Pin

int rowIndex = 0;

// *** FUNCTION WifiPercentSignal ***
int WifiPercentSignal()
{
  CurrentWiFiSignal = WiFi.RSSI();
  if (CurrentWiFiSignal > -40)
    CurrentWiFiSignal = -40;
  if (CurrentWiFiSignal < -90)
    CurrentWiFiSignal = -90;
  WifiSignal = map(CurrentWiFiSignal, -40, -90, 0, 100);
  return WifiSignal;
}

void sendEvent()
{
  Serial.print(", Wifi Signal: ");
  Serial.print(WifiPercentSignal());
  Serial.println("%");
  Blynk.virtualWrite(Widget_WifiSignal, WifiPercentSignal());
  Blynk.virtualWrite(Widget_WifiRawValue, WiFi.RSSI());
  Blynk.virtualWrite(Widget_HEIGHT_BME, VaGet[6]);
  Blynk.virtualWrite(Widget_HEIGHT_MPU, VaGet[7]);
  // adding 1 row to table every second
  Blynk.virtualWrite(V9, "add", rowIndex, VaPut[6], VaPut[7]);

  //highlighting latest added row in table
  Blynk.virtualWrite(V9, "pick", rowIndex);

  rowIndex++;
}

void readBattery()
{
  if (millis() - readBatTimer >= 60000)
  { // Read Battery Every 1 Minute
    double batVoltage = 0; // Get Battery Voltage
    for (int i = 0; i < 20; i++)
    { // Read Battery Voltage 20 Rounds
      batVoltage += analogRead(battery_adc);
      delay(20);
    }
    batVoltage = batVoltage / 20;                 // Get Average Battery Voltage
    batVal = map(batVoltage, 1662, 2340, 0, 100); // 1662.80 1724.30 Map to percentage 2340.75 2284.65
    Serial.println();
    Serial.print(" * batVoltage :");
    Serial.print(batVoltage);
    Serial.print("\t");
    Serial.print("batVal :");
    Serial.println(batVal);
    Serial.println();
    //    if (batVal < 10) {                          // Less than 10%
    //      displayBattLow();                         // Display Low Battery in OLED
    //      ledRedOn();                               // On Red LED
    //      sendBattLow();                            // Send Notify to Line
    //    } else if (batVal < 30) {                   // Less than 30%
    //      displayBattLow();                         // Display Low Battery in OLED
    //      ledRedOn();                               // On Red LED
    //    }
    readBatTimer = millis(); // Reset Battery Timer
    Blynk.virtualWrite(Widget_Battery, batVal);
  }
}

// *** RESTART ***
BLYNK_WRITE(RESET_B_pin)
{
  if (param.asInt() == 1)
  {
    Serial.print("RESET state = 1");
    state = 1;
  }
}

// *** Time ***
String getLocalTime()
{
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo))
  {
    Serial.println("Failed to obtain time");
    //    return;
  }
  char timeStringBuff[50]; //50 chars should be enough
  strftime(timeStringBuff, sizeof(timeStringBuff), "%Y-%m-%dT%H:%M:%S", &timeinfo);
  //print like "const char*"
  //  Serial.println(timeStringBuff);

  //Optional: Construct String object
  String formattime = String(timeStringBuff);
  //  Serial.println(formattime);
  //  Serial.println(formattime.length());
  return formattime;
}

// *** EEPROM ***
int writeString(char addr, String data_set);
String read_String(char addr);

// Return usage address
int writeString(char addr, String data_set)
{
  int _size = data_set.length();
  int i;
  for (i = 0; i < _size; i++)
  {
    EEPROM.put(addr + i, data_set[i]);
  }
  EEPROM.put(addr + _size, '\0'); //Add termination null character for String Data
  EEPROM.commit();
  return addr + _size + 1;
}

// Return read value
String read_String(char addr)
{
  int i;
  char data_set[100]; //Max 100 Bytes
  int len = 0;
  unsigned char k;
  EEPROM.get(addr, k);
  while (k != '\0' && len < 500) //Read until null character
  {
    EEPROM.get(addr + len, k);
    data_set[len] = k;
    len++;
  }
  data_set[len] = '\0';
  return String(data_set);
}
bool romReady(int addr)
{
  EEPROM.get(addr, rom_status);
  if (rom_status == 0)
  {
    return true;
  }
  else
  {
    return false;
  }
}
int saveValues(int z)
{
  addr = 0;
  while (!romReady(addr)) {
    addr += 48; // 7 index * 4 bytes + 1 string * 20 bytes
  }
  Serial.print("Rom is empty at address: ");
  Serial.println(addr);

  // Add 7 elements to rom
  for (int i = 0; i < (sizeof(VaPut) / sizeof(VaPut[0])); i++)
  {
    //    String as = String(VaPut[i]);
    EEPROM.put(addr, VaPut[i]);
    Serial.print("Data[");
    Serial.print(i);
    Serial.print("] was saved to rom is: ");
    Serial.print(VaPut[i]);
    Serial.print(" at ");
    Serial.println(addr);
    addr += sizeof(VaPut[i]);
  }

  //  timeClient.update();
  //  String date_time = timeClient.getFormattedDate();
  String date_time = getLocalTime();
  Serial.println("Save values at: " + date_time);
  Serial.print("Address: ");
  Serial.println(addr);
  addr = writeString(addr, date_time);

  Serial.print("now Rom at address: ");
  Serial.println(addr);

  Serial.println();

  //  int size_data = z;
  //  for (int i = 0; i < (sizeof(VaPut) / sizeof(VaPut[0])); i++)  {
  //    EEPROM.put(size_data, VaPut[i]);
  //    size_data += sizeof(VaPut[i]);
  //  }
  //  return size_data;
}
void readValues()
{
  len = 0;
  int i = 0;
  float read_data;
  while (i < addr) {
    //    Serial.println("Dataset number: " + String(i));
    if (len % 8 == 7) {
      VaGet[len] = read_String(i);
      Serial.println("Created at: " + VaGet[len]);
      i = i + VaGet[len].length() + 1;
      len += 1;
    }
    else {
      EEPROM.get(i, read_data);
      VaGet[len] = String(read_data);
      Serial.print("Data[");
      Serial.print(len);
      Serial.print("] was read from rom is: ");
      Serial.print(VaGet[len]);
      Serial.print(" at ");
      Serial.println(i);
      i += sizeof(read_data);
      len += 1;
    }


  }
  //  for (int i = 0; i < addr; i += sizeof(addr))
  //  {
  //    Serial.println(i);
  //    EEPROM.get(i, VaGet[len]);
  //    len += 1;
  //  }

  // int size_data = 0;
  // for (int i = 0; i < (sizeof(VaGet) / sizeof(VaGet[0])); i++)  {
  //   EEPROM.get(size_data, VaGet[i]);
  //   size_data += sizeof(VaGet[i]);
  // }
}
// **************

// READ SERSOR BME280
void read_bme()
{
  sumTemp = 0;
  sumPress = 0;
  sumAlti = 0;
  for (int i = 0; i < AVG_SIZE; i++)
  {
    sumTemp += bme.readTemperature();
    sumPress += bme.readPressure();
    sumAlti += bme.readAltitude(SEALEVELPRESSURE_HPA);
  }
}

// READ SERSOR MPU6050
//void MPU_Read_Task( void * pvParameters ) {
//  Serial.println(String(mpu2.getIntFreefallStatus()) + "MPU Status 2");
//  delay(100);
//  while (1)
//  {
//    readmpu2();
//    if (mpu2.getIntFreefallStatus())
//    {
//      Serial.println("Taskkkkk");
//      break;
//    }
//    //    Serial.println(sum);
//  }
//  Serial.println("Fallingggggggggggg");
//  start_time2 = millis();
//  while (sum2 < 20)
//  {
//    readmpu2();
//  }
//  stop_time2 = millis();
//  capture_time2 = stop_time2 - start_time2;
//  height2 = (pow(capture_time2 * 0.001, 2) * gravity) / 2;
//  sum2 = 0;
//  Serial.print("start_time2: ");
//  Serial.println(start_time2);
//  Serial.print("stop_time2: ");
//  Serial.println(stop_time2);
//  Serial.print("Capture time2 = ");
//  Serial.println(capture_time2);
//  Serial.print("Height2 = ");
//  Serial.print(height2);
//  Serial.println(" m");
//  Serial.println();
//  Serial.println("Finish read MPU2");
//
//  vTaskDelete( NULL );
//
//
//}

void read_mpu()
{
  //  xTaskCreatePinnedToCore(
  //    MPU_Read_Task,           // Task function
  //    "MPU Read Task",         // Name of task
  //    10000,                     // Stack size of task
  //    NULL,                     // Parameter of the task
  //    1,                        // Priority of the task
  //    &mpuTask,                // Task handle
  //    0);                      // Core used

  //    Serial.println(String(mpu1.getIntFreefallStatus()) + "MPU Status 1");
  //    delay(100);
  //  while (!(mpu.getIntFreefallStatus() == 1)) {}
  //  while (1) {
  //    readmpu1();
  //    if (mpu1.getIntFreefallStatus()) {
  //      start_time1 = millis();
  //    }
  //
  //  }
  //  while (1)
  //  {
  //    readmpu1();
  //    if (mpu1.getIntFreefallStatus())
  //    {
  //      Serial.println("Inttttttt");
  //      break;
  //    }
  //    //    Serial.println(sum);
  //  }
  //  Serial.println("Fallingggggggggggg");
  //  start_time1 = millis();
  //  while (sum1 < 20)
  //  {
  //    readmpu1();
  //  }
  //  stop_time1 = millis();
  //  capture_time1 = stop_time1 - start_time1;
  //  height1 = (pow(capture_time1 * 0.001, 2) * gravity) / 2;
  //  sum1 = 0;
  //  Serial.print("start_time1: ");
  //  Serial.println(start_time1);
  //  Serial.print("stop_time1: ");
  //  Serial.println(stop_time1);
  //  Serial.print("Capture time1 = ");
  //  Serial.println(capture_time1);
  //  Serial.print("Height1 = ");
  //  Serial.print(height1);
  //  Serial.println(" m");
  //  Serial.println();
  Serial.println(String(mpu1.getIntFreefallStatus()) + " MPU Status 1");
  Serial.println(String(mpu2.getIntFreefallStatus()) + " MPU Status 2");
  delay(100);
  while (1) {
    if (timer1[2] != 0 && timer2[2] != 0) {
      return;
    }
    float x = readmpu1();
    float y = readmpu2();
    if (timer1[0] == 0) {
      if (mpu1.getIntFreefallStatus()) {
        start_time1 = millis();
        //    Serial.print("V Start timer1 =\t");
        //    Serial.print(start_time1); Serial.print("\t");
        //    Serial.println(count1_st);
        Serial.print("MPU 1 Interrupt");
        //      if (count1_st == 0) {
        timer1[0] = start_time1;
        //      }
        //      count1_st++;
      }
    }
    if (timer1[1] == 0 && timer1[0] != 0) {
      if (sum1 > 20) {
        stop_time1 = millis();
        //    Serial.println(count1_sp);
        //      if (count1_sp == 0) {
        timer1[1] = stop_time1;
        capture_time1 = timer1[1] - timer1[0];
        timer1[2] = capture_time1;
        height1 = (pow(capture_time1 * 0.001, 2) * gravity) / 2;
        Serial.println();
        Serial.print("Start timer1 =\t");
        Serial.print(timer1[0]); Serial.print("\n");
        Serial.print("Stop timer1 =\t");
        Serial.print(timer1[1]); Serial.print("\n");
        Serial.print("Capture Time1 =\t");
        Serial.print(timer1[2]); Serial.print("\n");
        Serial.print("Height1 =\t");
        Serial.println(height1);
        //      }
        //      count1_sp++;
      }
    }

    //  Serial.printf("x =\t%f\tf1 =\t%d\t y =\t%f\t f2 =\t%d\n", x, f1, y, f2);
    if (timer2[0] == 0) {
      if (mpu2.getIntFreefallStatus()) {
        start_time2 = millis();
        //    Serial.print("V Start timer2 =\t");
        //    Serial.print(start_time2); Serial.print("\t");
        //    Serial.println(count2_st);
        Serial.print("MPU 2 Interrupt");
        //      if (count2_st == 0) {
        timer2[0] = start_time2;
        //      }
        //      count2_st++;
      }
    }

    if (timer2[1] == 0 && timer2[0] != 0) {
      if (sum2 > 20) {
        stop_time2 = millis();
        //    Serial.println(count2_sp);
        //      if (count2_sp == 0) {
        timer2[1] = stop_time2;
        capture_time2 = timer2[1] - timer2[0];
        timer2[2] = capture_time2;
        height2 = (pow(capture_time2 * 0.001, 2) * gravity) / 2;
        Serial.println();
        Serial.print("Start timer2 =\t");
        Serial.print(timer2[0]); Serial.print("\n");
        Serial.print("Stop timer2 =\t");
        Serial.print(timer2[1]); Serial.print("\n");
        Serial.print("Capture Time2 =\t");
        Serial.print(timer2[2]); Serial.print("\n");
        Serial.print("Height2 =\t");
        Serial.println(height2);
        //      }
        //      count2_sp++;
      }
    }
    //    if (height1 != 0 && height2 != 0) {
    //      return;
    //    }
  }
  //  if ((height1 < 50 && height1 > 10) && (height2 > 10 && height2 < 50)) {
  //      Serial.print("Height1: ");
  //      Serial.println(height1);
  //      Serial.print("Height2: ");
  //      Serial.println(height2);
  //      count1_st = 0;
  //      count1_sp = 0;
  //      count2_st = 0;
  //      count2_sp = 0;
  //      memset(timer1, 0, sizeof(timer1));
  //      memset(timer2, 0, sizeof(timer2));
  //      return;
  //    }
}

float readmpu1()
{
  mpu1.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);
  f_ax = ax;
  f_ay = ay;
  f_az = az;
  gax = (f_ax / 16384) * gravity;
  gay = (f_ay / 16384) * gravity;
  gaz = (f_az / 16384) * gravity;
  sum1 = sqrt(pow(gax, 2) + pow(gay, 2) + pow(gaz, 2));
  return sum1;
}

float readmpu2()
{
  mpu2.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);
  f_ax = ax;
  f_ay = ay;
  f_az = az;
  gax = (f_ax / 16384) * gravity;
  gay = (f_ay / 16384) * gravity;
  gaz = (f_az / 16384) * gravity;
  sum2 = sqrt(pow(gax, 2) + pow(gay, 2) + pow(gaz, 2));
  return sum2;
  //  sum2 = 10;
}

void ledstatus()
{
  if (!checkWiFi()) {
    return;
  }

  if (state == 9)
  {
    LED_ORANGE_B.on();
    LED_RED_B.off();
    LED_GREEN_B.off();
    LED_BLUE_B.off();
  }
  if (state == 0)
  {
    digitalWrite(LED_RED, HIGH);
    digitalWrite(LED_GREEN, HIGH);
    digitalWrite(LED_BLUE, HIGH);
    LED_RED_B.on();
    LED_GREEN_B.on();
    LED_BLUE_B.on();
  }
  if (state == 1)
  {
    digitalWrite(LED_RED, LOW);
    digitalWrite(LED_GREEN, HIGH);
    digitalWrite(LED_BLUE, LOW);
    LED_ORANGE_B.off();
    LED_RED_B.off();
    LED_GREEN_B.on();
    LED_BLUE_B.off();
  }
  if (state == 2)
  {
    digitalWrite(LED_RED, LOW);
    digitalWrite(LED_GREEN, LOW);
    digitalWrite(LED_BLUE, HIGH);
    LED_ORANGE_B.off();
    LED_RED_B.off();
    LED_GREEN_B.off();
    LED_BLUE_B.on();
  }
  if (state == 3)
  {
    digitalWrite(LED_RED, HIGH);
    digitalWrite(LED_GREEN, LOW);
    digitalWrite(LED_BLUE, LOW);
    LED_RED_B.on();
    LED_GREEN_B.off();
    LED_BLUE_B.off();
  }
}

void getdata()
{
  //  while (1) {
  // Connect or reconnect to WiFi
  if (!checkWiFi()) {
    return;
  }

  readValues();
//  Serial.println("\nTry Disconnected Wifi\n");
//  delay(10000);

  // Write to ThingSpeak. There are up to 8 fields in a channel, allowing you to store up to 8 different
  // pieces of information in a channel.
  for (int i = 0; i < len; i += 8)
  {
    if (i > 0) {
      delay(20000);  // Wait Thingspeak to ready for sending values
    }
    //    int h = ThingSpeak.setField(1, VaGet[i]);     // Temp HIGH
    //    Serial.println("h Status: " + String(h));
    //    h = ThingSpeak.setField(1, VaGet[i]);     // Temp HIGH
    //    Serial.println("h Status: " + String(h));
    ThingSpeak.setField(1, VaGet[i]);     // Temp HIGH
    ThingSpeak.setField(2, VaGet[i + 1]); // Temp LOW
    ThingSpeak.setField(3, VaGet[i + 2]); // Pressure HIGH
    ThingSpeak.setField(4, VaGet[i + 3]); // Pressure LOW
    ThingSpeak.setField(5, VaGet[i + 4]); // MPU6050_1_HEIGHT
    ThingSpeak.setField(6, VaGet[i + 5]); // MPU6050_2_HEIGHT
    ThingSpeak.setField(7, VaGet[i + 6]); // BME280_HEIGHT
//    ThingSpeak.setField(8, VaGet[i + 7]); // MPU6050_HEIGHT

    terminal.print("HEIGHT_BME : ");
    terminal.print(String(VaGet[i + 6]));
    terminal.print("  HEIGHT_MPU : ");
    terminal.println(String(VaGet[i + 7]));

    ThingSpeak.setCreatedAt(VaGet[i + 7]);

    // write to the ThingSpeak channel
    int x = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
    if (x == 200)
    {
      //    digitalWrite(14, HIGH);
      //    digitalWrite(26, LOW);
      Serial.println("Channel update successful.");
      Serial.println();

      //      for (int i = 0; i < EEPROM_SIZE; i++)
      //      {
      //        EEPROM.put(i, 0);
      //      }
      //      Serial.println("Clear ROM");
      for (int j = i / 8 * 48; j < ((i / 8) + 1) * 48; j++)
      {
        EEPROM.put(j, 0);
      }

      Serial.println("Clear ROM");
      //            break;
    }
    else
    {
      Serial.println("Problem updating channel. HTTP error code " + String(x));
      Serial.println();
      break;
    }
  }
  EEPROM.commit();
  //    Blynk.virtualWrite(Widget_HEIGHT_BME, VaGet[6]);
  //    Blynk.virtualWrite(Widget_HEIGHT_MPU, VaGet[7]);
  //    sendEvent();
  //  }
}

void checkSettings1()
{
  Serial.println();
  Serial.println(mpu1.testConnection() ? " * MPU6050_1_x68 connection successful" : " * MPU6050_1_x68 connection failed");
  Serial.print(" * Motion Interrupt:     ");
  Serial.println(mpu1.getIntMotionEnabled() ? "Enabled" : "Disabled");
  Serial.print(" * Zero Motion Interrupt:     ");
  Serial.println(mpu1.getIntZeroMotionEnabled() ? "Enabled" : "Disabled");
  Serial.print(" * Free Fall Interrupt:       ");
  Serial.println(mpu1.getIntFreefallEnabled() ? "Enabled" : "Disabled");
  Serial.print(" * Free Fal Threshold:          ");
  Serial.println(mpu1.getFreefallDetectionThreshold());
  Serial.print(" * Free FallDuration:           ");
  Serial.println(mpu1.getFreefallDetectionDuration());
  Serial.println();
}

void checkSettings2()
{
  Serial.println();
  Serial.println(mpu2.testConnection() ? " * MPU6050_2_x69 connection successful" : " * MPU6050_2_x69 connection failed");
  Serial.print(" * Motion Interrupt:     ");
  Serial.println(mpu2.getIntMotionEnabled() ? "Enabled" : "Disabled");
  Serial.print(" * Zero Motion Interrupt:     ");
  Serial.println(mpu2.getIntZeroMotionEnabled() ? "Enabled" : "Disabled");
  Serial.print(" * Free Fall Interrupt:       ");
  Serial.println(mpu2.getIntFreefallEnabled() ? "Enabled" : "Disabled");
  Serial.print(" * Free Fal Threshold:          ");
  Serial.println(mpu2.getFreefallDetectionThreshold());
  Serial.print(" * Free FallDuration:           ");
  Serial.println(mpu2.getFreefallDetectionDuration());
  Serial.println();
}

bool checkWiFi()
{
  //  while (1) {
  //    if (WiFi.status() != WL_CONNECTED) {
  //     Serial.println("WiFi Disconnected.");
  //      WiFi.begin((char*)ssid, (char*)pass);
  //      Serial.print("disconnect state: ");
  //      Serial.println(state);
  //    }
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.print("Attempting to connect to SSID: ");
    //Serial.println(SECRET_SSID);

    long ttl = millis();
    while (WiFi.status() != WL_CONNECTED)
    {
      if (millis() - ttl >= 5000)
      {
        Serial.println("Can't Connected");
        return false;
      }
      WiFi.begin(ssid, pass); // Connect to WPA/WPA2 network. Change this line if using open or WEP network
      Serial.print(".");
      delay(1000);
    }
  }
  Serial.println("\nConnected.");
  return true;
  //    vTaskDelay(5000 / portTICK_PERIOD_MS);
  //    delay(000);
}

//void checkstate(void *p) {
//  while(1){
//    if (state == 0){
//      state = 1;
//    }
//    vTaskDelay(5000 / portTICK_PERIOD_MS);
//  }
//}

//void CheckConnection(void *p){
//  while(1) {
//    if(!Blynk.connected()){
//      Serial.println("Not connected to Blynk server.");
//      Serial.println("Connecting to Blynk server...");
//      Blynk.connect();  // try to connect to server
//    }
//    Serial.println("Blynk connected...");
//    vTaskDelay(5000 / portTICK_PERIOD_MS);
//  }
//}

// *** CHECK BLYNK CONNECTION ***
void CheckConnection()
{
  if (!Blynk.connected())
  {
    Serial.println("Not connected to Blynk server.");
    Serial.println("Connecting to Blynk server...");
    Blynk.connect(); // try to connect to server
    state = 9;
    ledstatus();
  }
}

// *** SETUP ***
void setup()
{
  Wire.begin();
  EEPROM.begin(EEPROM_SIZE);
  Serial.begin(115200); //Initialize serial

  // SET BME280


  //  bmeTwoWire.begin(21, 22);
  //  while (!bme.begin())
  bmeTwoWire.begin(16, 17);
  while (!bme.begin(&bmeTwoWire))
  {
    Serial.println("Could not find a valid bme280 sensor, check wiring!");
  }

  // SET MPU6050 initialize device
  Serial.println("Initializing I2C devices...");
  mpu1.initialize();
  mpu2.initialize();
  // verify connection
  Serial.println("Testing device connections...");
  delay(10);
  mpu1.setIntFreefallEnabled(1);
  mpu1.setFreefallDetectionThreshold(150);
  mpu1.setFreefallDetectionDuration(1);
  mpu2.setIntFreefallEnabled(1);
  mpu2.setFreefallDetectionThreshold(150);
  mpu2.setFreefallDetectionDuration(1);
  checkSettings1();
  checkSettings2();
  delay(100);

  Serial.println(" + Start Measuremant");
  mpu1.getIntFreefallStatus();
  mpu2.getIntFreefallStatus();

  // SET WiFi
  //  xTaskCreate(&checkWiFi, "checkWiFi", 3000, NULL, 10, NULL);
  if (!checkWiFi()) {
    while (!checkWiFi()) {
    }
  }

  Blynk.begin(auth, ssid, pass);
  Blynk.connect();
  WiFi.mode(WIFI_STA);
  //  xTaskCreate(&CheckConnection, "CheckConnection", 3000, NULL, 10, NULL);
  ThingSpeak.begin(client); // Initialize ThingSpeak

  // SET LED
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);
  pinMode(battery_adc, INPUT);
  //  state = 0;
  ledstatus();
  state = 1;
  readBattery();

  //clean table at start
  Blynk.virtualWrite(V9, "clr");

  //run sendEvent method every second
  //  xTaskCreate(&checkstate, "checkstate", 3000, NULL, 11, NULL);
  //  timerAttachInterrupt(checkWiFi, &checkWiFi, true);
  //  timer.setInterval(1000L, CheckConnection);
  //  timer.setInterval(1000L, sendEvent);

  //init and get the time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  String dt_now = getLocalTime();
  Serial.println("Time is: " + dt_now + "\n");

  //  timeClient.setTimeOffset(0);
  //  timeClient.begin();
}

// *** LOOP ***
void loop()
{
  //  CheckConnection();
  //  Blynk.run();
  //  timer.run();
//  Serial.print(" * start state: ");
  //  Serial.println(uxTaskGetNumberOfTasks);
  // *** BME280 high height measurement ***
  if (state == 9)
  {
    Serial.print(" * state: ");
    Serial.println(state);
    ledstatus();
    CheckConnection();
    sendEvent();
    state = 1;
  }
  if (state == 1)
  {
    CheckConnection();
    Serial.print(" * state: ");
    Serial.println(state);
    delay(10000);
    Serial.println("*** Start measurement ***");
    read_bme();
    VaPut[0] = sumTemp / AVG_SIZE;
    VaPut[2] = sumPress / AVG_SIZE;
    upAlti = sumAlti / AVG_SIZE;
    Serial.print("Average Temperature = ");
    Serial.print(VaPut[0]);
    Serial.println(" *C");
    Serial.print("Pressure = ");
    Serial.print(VaPut[2]);
    Serial.println(" Pa");
    Serial.print("Approx. Altitude = ");
    Serial.print(upAlti);
    Serial.println(" m");
    //    Serial.print("Humidity = ");
    //    Serial.print(bme.readHumidity());
    //    Serial.println(" %");
    Serial.println();
    Serial.println("Save high ground bme succcess!");
    Serial.println();
    ledstatus();
    state = 2;
    Serial.print("Go to state: ");
    Serial.println(state);
  }

  // *** MPU6050 height measurement ***
  if (state == 2)
  {
    Serial.print("state: ");
    Serial.println(state);
    read_mpu();
//    count1_st = 0;
//    count1_sp = 0;
//    count2_st = 0;
//    count2_sp = 0;
    memset(timer1, 0, sizeof(timer1));
    memset(timer2, 0, sizeof(timer2));
//    height1 = 0;
//    height2 = 0;
    if (!((height1 < 50 && height1 > 0.1) && (height2 > 0.1 && height2 < 50)))
    {
      state = 9;
      Serial.print("Go to state: ");
    Serial.println(state);
    }
    else
    {
      VaPut[4] = height1;
      VaPut[5] = height2;
      //      ledstatus();
      state = 3;
      Serial.print("Go to state: ");
    Serial.println(state);
    }
  }

  // *** BME280 low height measurement ***
  if (state == 3)
  {
    Serial.print(" * state: ");
    Serial.println(state);
    delay(5000);

    read_bme();
    //    float bme2 = VaPut[5];
    //    saveValues(addr);
    VaPut[1] = sumTemp / AVG_SIZE;
    VaPut[3] = sumPress / AVG_SIZE;
    lowAlti = sumAlti / AVG_SIZE;
    Serial.print("Average Temperature = ");
    Serial.print(VaPut[1]);
    Serial.println(" *C");
    Serial.print("Pressure = ");
    Serial.print(VaPut[3]);
    Serial.println(" Pa");
    Serial.print("Approx. Altitude = ");
    Serial.print(lowAlti);
    Serial.println(" m");
    //    Serial.print("Humidity = ");
    //    Serial.print(bme.readHumidity());
    //    Serial.println(" %");
    Serial.println();
    Serial.println("Save low ground bme succcess!");
    Serial.println();
    Serial.print("Height diff: ");
    Serial.print(upAlti - lowAlti);
    Serial.println(" m");
    Serial.println();
    VaPut[6] = upAlti - lowAlti;
    ledstatus();
    saveValues();
    EEPROM.commit();
    state = 4;
    Serial.print("Go to state: ");
    Serial.println(state);
  }

  // *** GETDATE TO BLYNK AND ThingSpeak***
  if (state == 4)
  {
    //    CheckConnection();
    Serial.print(" * state: ");
    Serial.println(state);
    getdata();
    readBattery();
    delay(10000); // Wait 20 seconds to update the channel again
    state = 1;
    Serial.print("Go to state: ");
    Serial.println(state);
  }
}
