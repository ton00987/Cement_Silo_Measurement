/*
  Demo   : Silo measurement
  VERSION: 0.2.5 (1/10/2562)

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
#include <BlynkSimpleEsp32.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <MPU6050.h>
#include <EEPROM.h>

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
Adafruit_BME280 bme;
TwoWire bmeTwoWire = TwoWire(0);

// *** MPU6050 ***
MPU6050 mpu(0x69);
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

// *** Wifi ***
WiFiClient client;
char ssid[] = "";  // your network SSID (name)
char pass[] = ""; // your network password
int WifiSignal;
int CurrentWiFiSignal;

// *** Blynk ***
char auth[] = "";
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
int state = 0;
int rom_status;
int a;
int saveValues(int z = 0);
float VaPut[8] = {};
float VaGet[8] = {};

// *** ThingSpeak ***
unsigned long myChannelNumber = 000000;         //update
const char *myWriteAPIKey = ""; //update
int keyIndex = 0;                               // your network key Index number (needed only for WEP)

// *** Battery ***
long readBatTimer = -60000;     // Timer For Read Battery
long batVal = 0;                // Get Battery Value
#define battery_adc      35       // Battery Read Pin

int rowIndex = 0;

// *** FUNCTION WifiPercentSignal ***
int WifiPercentSignal() {
  CurrentWiFiSignal = WiFi.RSSI();
  if (CurrentWiFiSignal > -40) CurrentWiFiSignal = -40;
  if (CurrentWiFiSignal < -90) CurrentWiFiSignal = -90;
  WifiSignal = map(CurrentWiFiSignal, -40, -90, 0, 100);
  return WifiSignal;
}

void sendEvent() {
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

void readBattery() {
  if (millis() - readBatTimer >= 60000) {       // Read Battery Every 1 Minute
    double batVoltage = 0;                      // Get Battery Voltage
    for (int i = 0; i < 20; i++) {              // Read Battery Voltage 20 Rounds
      batVoltage += analogRead(battery_adc);
      delay(20);
    }
    batVoltage = batVoltage / 20;               // Get Average Battery Voltage
    batVal = map(batVoltage, 1662, 2340, 0, 100);   // 1662.80 1724.30 Map to percentage 2340.75 2284.65
    Serial.println();
    Serial.print(" * batVoltage :");
    Serial.print(batVoltage); Serial.print("\t");
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
    readBatTimer = millis();                    // Reset Battery Timer
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

// *** EEPROM ***
bool romReady() {
  EEPROM.get(0, rom_status);
  if (rom_status == 0)  {
    return true;
  }
  else  {
    return false;
  }
}
int saveValues(int z) {
  int size_data = z;
  for (int i = 0; i < (sizeof(VaPut) / sizeof(VaPut[0])); i++)  {
    EEPROM.put(size_data, VaPut[i]);
    size_data += sizeof(VaPut[i]);
  }
  return size_data;
}
void readValues() {
  int size_data = 0;
  for (int i = 0; i < (sizeof(VaGet) / sizeof(VaGet[0])); i++)  {
    EEPROM.get(size_data, VaGet[i]);
    size_data += sizeof(VaGet[i]);
  }
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
void read_mpu()
{
  Serial.println(mpu.getIntFreefallStatus());
  delay(100);
  //  while (!(mpu.getIntFreefallStatus() == 1)) {}
  while (1)
  {
    readmpu();
    if (mpu.getIntFreefallStatus())
    {
      Serial.println("Inttttttt");
      break;
    }
    //    Serial.println(sum);
  }
  Serial.println("Fallingggggggggggg");
  start_time = millis();
  while (sum < 20)
  {
    readmpu();
  }
  stop_time = millis();
  capture_time = stop_time - start_time;
  height = (pow(capture_time * 0.001, 2) * gravity) / 2;
  sum = 0;
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

float readmpu()
{
  mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);
  f_ax = ax;
  f_ay = ay;
  f_az = az;
  gax = (f_ax / 16384) * gravity;
  gay = (f_ay / 16384) * gravity;
  gaz = (f_az / 16384) * gravity;
  sum = sqrt(pow(gax, 2) + pow(gay, 2) + pow(gaz, 2));
  return sum;
}

void ledstatus()
{
  checkWiFi();
  if (state == 9) {
    LED_ORANGE_B.on();
    LED_RED_B.off();
    LED_GREEN_B.off();
    LED_BLUE_B.off();
  }
  if (state == 0) {
    digitalWrite(LED_RED, LOW);
    digitalWrite(LED_GREEN, LOW);
    digitalWrite(LED_BLUE, LOW);
    LED_RED_B.on();
    LED_GREEN_B.on();
    LED_BLUE_B.on();
  }
  if (state == 1) {
    digitalWrite(LED_RED, HIGH);
    digitalWrite(LED_GREEN, LOW);
    digitalWrite(LED_BLUE, HIGH);
    LED_ORANGE_B.off();
    LED_RED_B.off();
    LED_GREEN_B.on();
    LED_BLUE_B.off();
  }
  if (state == 2) {
    digitalWrite(LED_RED, HIGH);
    digitalWrite(LED_GREEN, HIGH);
    digitalWrite(LED_BLUE, LOW);
    LED_ORANGE_B.off();
    LED_RED_B.off();
    LED_GREEN_B.off();
    LED_BLUE_B.on();
  }
  if (state == 3) {
    digitalWrite(LED_RED, LOW);
    digitalWrite(LED_GREEN, HIGH);
    digitalWrite(LED_BLUE, HIGH);
    LED_RED_B.on();
    LED_GREEN_B.off();
    LED_BLUE_B.off();
  }
}

void getdata()
{
  readValues();
  while (1) {
    // Connect or reconnect to WiFi
    if (WiFi.status() != WL_CONNECTED)
    {
      Serial.print("Attempting to connect to SSID: ");
      //Serial.println(SECRET_SSID);
      while (WiFi.status() != WL_CONNECTED)
      {
        WiFi.begin(ssid, pass); // Connect to WPA/WPA2 network. Change this line if using open or WEP network
        Serial.print(".");
        delay(1000);
      }
      Serial.println("\nConnected.");
    }

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

    //    Blynk.virtualWrite(Widget_HEIGHT_BME, VaGet[6]);
    //    Blynk.virtualWrite(Widget_HEIGHT_MPU, VaGet[7]);
    //    sendEvent();

    terminal.print("HEIGHT_BME : "); terminal.print(String(VaGet[6]));
    terminal.print("  HEIGHT_MPU : "); terminal.println(String(VaGet[7]));

    // write to the ThingSpeak channel
    int x = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
    if (x == 200)
    {
      //    digitalWrite(14, HIGH);
      //    digitalWrite(26, LOW);
      Serial.println("Channel update successful.");
      Serial.println();
      //      Serial.println("EEPROM.length() " + String(EEPROM.length()));
      for (int i = 0; i < EEPROM_SIZE; i++)
      {
        EEPROM.put(i, 0);
      }
      Serial.println("Clear ROM");
      break;
    }
    else
    {
      Serial.println("Problem updating channel. HTTP error code " + String(x));
      Serial.println();
    }
  }
}

void checkSettings()
{
  Serial.println();
  Serial.print(" * Motion Interrupt:     ");
  Serial.println(mpu.getIntMotionEnabled() ? "Enabled" : "Disabled");
  Serial.print(" * Zero Motion Interrupt:     ");
  Serial.println(mpu.getIntZeroMotionEnabled() ? "Enabled" : "Disabled");
  Serial.print(" * Free Fall Interrupt:       ");
  Serial.println(mpu.getIntFreefallEnabled() ? "Enabled" : "Disabled");
  Serial.print(" * Free Fal Threshold:          ");
  Serial.println(mpu.getFreefallDetectionThreshold());
  Serial.print(" * Free FallDuration:           ");
  Serial.println(mpu.getFreefallDetectionDuration());
  Serial.println();
}

void checkWiFi() {
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
    while (WiFi.status() != WL_CONNECTED)
    {
      WiFi.begin(ssid, pass); // Connect to WPA/WPA2 network. Change this line if using open or WEP network
      Serial.print(".");
      delay(1000);
    }
    Serial.println("\nConnected.");
  }
  Serial.print("loopwifi state: ");
  Serial.println(state);
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
void CheckConnection() {
  if (!Blynk.connected()) {
    Serial.println("Not connected to Blynk server.");
    Serial.println("Connecting to Blynk server...");
    Blynk.connect();  // try to connect to server
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
  bmeTwoWire.begin(16, 17);
  while (!bme.begin(&bmeTwoWire)) {
    Serial.println("Could not find a valid bme280 sensor, check wiring!");
  }

  // SET MPU6050 initialize device
  Serial.println("Initializing I2C devices...");
  mpu.initialize();
  // verify connection
  Serial.println("Testing device connections...");
  Serial.println(mpu.testConnection() ? "MPU6050 connection successful" : "MPU6050 connection failed");
  delay(10);
  mpu.setIntFreefallEnabled(1);
  mpu.setFreefallDetectionThreshold(150);
  mpu.setFreefallDetectionDuration(1);
  checkSettings();

  // SET WiFi
  //  xTaskCreate(&checkWiFi, "checkWiFi", 3000, NULL, 10, NULL);

  checkWiFi();
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
  timer.setInterval(1000L, sendEvent);
}

// *** LOOP ***
void loop()
{
  //  CheckConnection();
  Blynk.run();
  timer.run();
  Serial.print(" * start state: ");
  //  Serial.println(uxTaskGetNumberOfTasks);
  // *** BME280 high height measurement ***
  if (state == 9)
  {
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
    //    Serial.print("Humidity = ");
    //    Serial.print(bme.readHumidity());
    //    Serial.println(" %");
    Serial.println();
    Serial.println("Save high ground bme succcess!");
    Serial.println();
    ledstatus();
    state = 2;
  }

  // *** MPU6050 height measurement ***
  if (state == 2)
  {
    Serial.print("state: ");
    Serial.println(state);
    read_mpu();
    if (!((height < 50) && (height > 10))) {
      state = 9;
    }
    else {
      VaPut[7] = height;
      //      ledstatus();
      state = 3;
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
    VaPut[3] = sumTemp / AVG_SIZE;
    VaPut[4] = sumPress / AVG_SIZE;
    VaPut[5] = sumAlti / AVG_SIZE;
    Serial.print("Average Temperature = ");
    Serial.print(VaPut[3]);
    Serial.println(" *C");
    Serial.print("Pressure = ");
    //  Serial.print(bmeVaPut[1] / 100.0F);
    Serial.print(VaPut[4]);
    Serial.println(" Pa");
    Serial.print("Approx. Altitude = ");
    Serial.print(VaPut[5]);
    Serial.println(" m");
    //    Serial.print("Humidity = ");
    //    Serial.print(bme.readHumidity());
    //    Serial.println(" %");
    Serial.println();
    Serial.println("Save low ground bme succcess!");
    Serial.println();
    Serial.print("Height diff: ");
    Serial.print(VaPut[2] - VaPut[5]);
    Serial.println(" m");
    Serial.println();
    VaPut[6] = VaPut[2] - VaPut[5];
    ledstatus();
    saveValues();
    state = 4;
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
  }
}
