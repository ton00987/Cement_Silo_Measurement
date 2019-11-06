/*
* Demo   : Silo measurement 
* VERSION: 0.1 (2/8/2019)
*
* MCU    : ESP32(ESP-WROOM-32)
*
* Output : Wifi
*        : BLYNK
*        : ThingSpeak
*
* Demo   : BLYNK -> "SILO MEASUREMENT"
*        : Blynk library V0.6.1
*        : BME280 
*        : MPU6050
*        :  
* Widget : RESET_B_pin              V1
*        : Widget_HEIGHT_BME        V5
*        : Widget_HEIGHT_MPU        V6
*        : Widget_Terminal          V7
*        : Widget_LED_RED_Status    V14
*        : Widget_LED_GREEN_Status  V27
*        : Widget_LED_BLUE_Status   V26
*/

#include <Wire.h>
#include <ThingSpeak.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <MPU6050.h>
#include <EEPROM.h>

// *** LED_PIN ***
#define LED_RED 14
#define LED_GREEN 27
#define LED_BLUE 26

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

// *** Blynk ***
char auth[] = "";
#define BLYNK_PRINT Serial
#define RESET_B_pin V1
#define Widget_HEIGHT_BME V5
#define Widget_HEIGHT_MPU V6
#define Widget_Terminal V7
#define Widget_LED_RED_Status V14
#define Widget_LED_GREEN_Status V27
#define Widget_LED_BLUE_Status V26
WidgetLED LED_RED_B(Widget_LED_RED_Status);
WidgetLED LED_GREEN_B(Widget_LED_GREEN_Status);
WidgetLED LED_BLUE_B(Widget_LED_BLUE_Status);
WidgetTerminal terminal(Widget_Terminal);
BlynkTimer Byk_timer;

// *** EEPROM ***
#define EEPROM_SIZE 512
int state;
int rom_status;
int a;
int saveValues(int z = 0);
float VaPut[8] = {};
float VaGet[8] = {};

// *** ThingSpeak ***
unsigned long myChannelNumber = 000000;         //update
const char *myWriteAPIKey = ""; //update
int keyIndex = 0;                               // your network key Index number (needed only for WEP)

// *** RESTART ***
BLYNK_WRITE(RESET_B_pin)
{
  if (param.asInt() == 1)
  {
    Serial.print("RESET");
    ESP.restart();
  }
}

// *** EEPROM ***
bool romReady(){
  EEPROM.get(0, rom_status);
  if (rom_status == 0)  {
    return true;
  }
  else  {
    return false;
  }
}
int saveValues(int z){
  int size_data = z;
  for (int i = 0; i < (sizeof(VaPut) / sizeof(VaPut[0])); i++)  {
    EEPROM.put(size_data, VaPut[i]);
    size_data += sizeof(VaPut[i]);
  }
  return size_data;
}
void readValues(){
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

void getdata()
{
  // Connect or reconnect to WiFi
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.print("Attempting to connect to SSID: ");
    //Serial.println(SECRET_SSID);
    while (WiFi.status() != WL_CONNECTED)
    {
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
  Blynk.virtualWrite(Widget_HEIGHT_BME, VaGet[6]);
  Blynk.virtualWrite(Widget_HEIGHT_MPU, VaGet[7]);
  terminal.println("HEIGHT_BME" + String(VaGet[6]) + "\tHEIGHT_MPU" + String(VaGet[7]));

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
    digitalWrite(LED_RED, HIGH);
    digitalWrite(LED_GREEN, LOW);
    digitalWrite(LED_BLUE, LOW);
    LED_RED_B.on();
    LED_GREEN_B.off();
    LED_BLUE_B.off();
  }
  else
  {
    Serial.println("Problem updating channel. HTTP error code " + String(x));
    Serial.println();
  }
  delay(5000); // Wait 20 seconds to update the channel again
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

void checkWiFi(void *p) {
  while(1) {
    if(WiFi.status() != WL_CONNECTED) {
//      digitalWrite(LEDWiFiPin, LOW);
      Serial.println("WiFi Disconnected.");
      WiFi.begin((char*)ssid, (char*)pass);
    } 
//    else {
//      digitalWrite(LEDWiFiPin, HIGH);
//    }
  vTaskDelay(5000 / portTICK_PERIOD_MS);
 }
}

// *** CHECK BLYNK CONNECTION ***
void CheckConnection(){
  if(!Blynk.connected()){
    Serial.println("Not connected to Blynk server.");
    Serial.println("Connecting to Blynk server...");
    Blynk.connect();  // try to connect to server
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
  while (!bme.begin(&bmeTwoWire)){
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
  
  state = 1;

  // SET LED
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);
  digitalWrite(LED_RED, HIGH);
  digitalWrite(LED_GREEN, HIGH);
  digitalWrite(LED_BLUE, HIGH);
  LED_RED_B.on();
  LED_GREEN_B.on();
  LED_BLUE_B.on();
  
  // SET WiFi
  xTaskCreate(&checkWiFi, "checkWiFi", 3000, NULL, 10, NULL);
  Blynk.begin(auth, ssid, pass);
  Blynk.connect();
  WiFi.mode(WIFI_STA);
  ThingSpeak.begin(client); // Initialize ThingSpeak
  Byk_timer.setInterval(5000, CheckConnection);
}

// *** LOOP ***
void loop()
{
  Blynk.run();

  // *** BME280 high height measurement ***
  if (state == 1)
  {
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
    digitalWrite(LED_RED, LOW);
    digitalWrite(LED_GREEN, HIGH);
    digitalWrite(LED_BLUE, LOW);
    LED_RED_B.off();
    LED_GREEN_B.on();
    LED_BLUE_B.off();
    state = 2;
  }

  // *** MPU6050 height measurement ***
  if (state == 2)
  {
    read_mpu();
    VaPut[7] = height;
    state = 3;
    digitalWrite(LED_RED, LOW);
    digitalWrite(LED_GREEN, LOW);
    digitalWrite(LED_BLUE, HIGH);
    LED_RED_B.off();
    LED_GREEN_B.off();
    LED_BLUE_B.on();

  }

  // *** BME280 low height measurement ***
  if (state == 3)
  {
    delay(10000);

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

    saveValues();
    state = 4;
  }

  // *** GETDATE TO BLYNK AND ThingSpeak***
  if (state == 4)
  {
    getdata();
    state = 1;
  }
  delay(100);
}
