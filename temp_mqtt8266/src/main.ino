#include <ESP8266WiFi.h>
#include <Wire.h>
#include <PubSubClient.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <time.h>
#include <stdlib.h>

// Time Config
int timezone = 2;
int dst = 0;


// Wifi Config
#define wifi_ssid "iot"
#define wifi_password "iot123456"
unsigned char macAddress[6];
char c_macAddress[18];

// MQTT Config
#define mqtt_server "192.168.2.2"
#define mqtt_user "YOUR_MQTT_USERNAME"
#define mqtt_password "YOUR_MQTT_PASSWORD"

// MQTT Topics
char* topic_humidity;
char* topic_dewpoint;
char* topic_temperature_c;
char* topic_temperature_f;
char* topic_barometer_hpa;
char* topic_barometer_inhg;
char* topic_correction_baro;
char* topic_correction_temp;
char* topic_correction_hum;
char* topicprefix = (char*)"sensor/";

// flowcontrol
long lastMsg = 0;
long lastForceMsg = 0;
bool forceMsg = false;

// Initial sensor values
float temp = 0.0;
float hum = 0.0;
float dewPoint = 0.0;
float baro = 0.0;

//only post new values if the differ by this much from previously posted value
float difftemp = 0.1;
float diffhum = 1;
float diffbaro = 0.5;

// Initial Correction Values
float correction_baro=0.0;
float correction_hum=0.0;
float correction_temp=0.0;

//Wiring config
#define red_led 0
#define blue_led 2
const uint8_t bmeaddress = 0x76;

WiFiClient espClient;
PubSubClient client(espClient);
Adafruit_BME280 bme; // I2C

void setup() {
  // Setup the two LED ports to use for signaling status
  pinMode(red_led, OUTPUT);
  pinMode(blue_led, OUTPUT);
  digitalWrite(red_led, HIGH);  
  digitalWrite(blue_led, HIGH);  
  Serial.begin(115200);
  digitalWrite(red_led, LOW);  
  digitalWrite(blue_led, LOW);  
  delay(1000);
  //for 8266
  Wire.begin(4, 5);
  // for ESP32
  //Wire.begin(21, 22);
  blink_blue();

  //wait for sensor to get online
  delay(1000);

  // Start sensor
  while (!bme.begin(bmeaddress)) {
    Serial.println("Could not find a valid BME280 sensor, check wiring!");
    blink_red();
    delay(1000);
  }

  setup_wifi();
  client.setServer(mqtt_server, 1883);

  //generate topic names based on MAC address
  Serial.println("will post on following topics:");
  topic_humidity = concat((char*)c_macAddress, "/humidity/percentRelative");
  topic_dewpoint = concat((char*)c_macAddress, "/humidity/dewPoint");
  topic_temperature_c = concat((char*)c_macAddress, "/temperature/degreeCelsius");
  topic_barometer_hpa = concat((char*)c_macAddress, "/barometer/hectoPascal");
  Serial.println("will look for correction values on following topics:");
  topic_correction_baro = concat((char*)c_macAddress, "/barometer/correction");
  topic_correction_temp = concat((char*)c_macAddress, "/temperature/correction");
  topic_correction_hum = concat((char*)c_macAddress, "/humidity/correction");

  digitalWrite(blue_led, HIGH);
  digitalWrite(red_led, LOW);    

  configTime(timezone * 3600, dst, "pool.ntp.org", "time.nist.gov");
  Serial.println("\nWaiting for time");
  while (!time(nullptr)) {
    Serial.print(".");
    delay(1000);
  }
  Serial.println();
  Serial.print("time received: ");
  printLocalTime();

  // avoid sending values before there has been a chance to receive correction values
  long now = millis();
  lastMsg = now - 4500;

}

// helps putting together the topic names
char* concat(const char *mac, const char *topic)
{
    char *result = (char*)malloc(strlen(mac) + strlen(topic) + 9); // +1 for the null-terminator
    // in real code you would check for errors in malloc here
    strcpy(result, topicprefix);
    strcat(result, mac);
    strcat(result, topic);
    Serial.println(result);
    return result;
}

// prints local time
void printLocalTime()
{
  time_t now = time(nullptr);
  Serial.println(ctime(&now));
}

void blink_red() {
  digitalWrite(red_led, LOW);
  delay(20);
  digitalWrite(red_led, HIGH);  
}

void blink_blue() {
  digitalWrite(blue_led, LOW);
  delay(20);
  digitalWrite(blue_led, HIGH);  
  delay(20);
  digitalWrite(blue_led, LOW);

}

void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to Wifi SSID ");
  Serial.print(wifi_ssid);
  Serial.print(" with password: ");
  Serial.print(wifi_password);

  WiFi.begin(wifi_ssid, wifi_password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    blink_blue();
  }
  digitalWrite(blue_led, HIGH);
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  // find out the MAC address of the device
  WiFi.macAddress(macAddress);
  Serial.print("MAC Address: ");

  //transform the array of hex into something we can print
  for (int i = 0; i < sizeof(macAddress); ++i){
      sprintf(c_macAddress,"%s%02x",c_macAddress,macAddress[i]);
    }
  Serial.println (c_macAddress);
}

void reconnect() {
  // Loop until we're reconnected
  Serial.print("Connectedto MQTT: ");
  Serial.println(client.connected());
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    blink_blue();
    if (client.connect((const char*)macAddress, mqtt_user, mqtt_password)) {
      Serial.println("connected");
      digitalWrite(red_led, LOW);
      Serial.println("subscribing to topics: ");
      Serial.println(topic_correction_baro);
      client.subscribe(topic_correction_baro);
      Serial.println(topic_correction_hum);
      client.subscribe(topic_correction_hum);
      Serial.println(topic_correction_temp);
      client.subscribe(topic_correction_temp);

      client.setCallback(callback);
      client.loop();

    } 
    else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      blink_blue();
      delay(200);
      blink_blue();
      delay(4760);
    }
  }
  Serial.print("Connected to MQTT: ");
  Serial.println(client.connected());
  digitalWrite(blue_led, HIGH);
}

// callback that is called if a message is received on any of the subscribed topics
void callback(char* topic, byte* payload, unsigned int length) {
  
  int i = 0;


  Serial.println("Message arrived on topic: " + String(topic));
  Serial.println("Length: " + String(length,DEC));
  char message_buff[length+1];

  if (strcmp(topic, topic_correction_baro) == 0){
    Serial.print("Receiving correction value for barometric pressure: ");
    // create character buffer with ending null terminator (string)
    for(i=0; i<length; i++) {
      message_buff[i] = payload[i];
    }
    message_buff[i] = '\0';
      
    String msgString = String(message_buff);
    Serial.println(msgString);
    correction_baro = atof(message_buff);
  }
  if (strcmp(topic, topic_correction_hum) == 0){  
    Serial.println ("dew");
  }
  if (strcmp(topic, topic_correction_temp) == 0){  
    Serial.println ("temp");
  }

  forceMsg = true;
}

// check if new measurement needs to be posted or not
bool checkBound(float newValue, float prevValue, float maxDiff) {
  return newValue < prevValue - maxDiff || newValue > prevValue + maxDiff;
}


// delta max = 0.6544 wrt dewPoint()
// 6.9 x faster than dewPoint()
// reference: http://en.wikipedia.org/wiki/Dew_point
double dewPointFast(double celsius, double humidity)
{
 double a = 17.271;
 double b = 237.7;
 double temp = (a * celsius) / (b + celsius) + log(humidity*0.01);
 double Td = (b * temp) / (a - temp);
 return Td;
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  long now = millis();
  if (now - lastMsg > 5000) {
    lastMsg = now;

    // MQTT broker could go away and come back at any time
    // so doing a forced publish to make sure something shows up
    // within the first 5 minutes after a reset
    if (now - lastForceMsg > 300000) {
      lastForceMsg = now;
      forceMsg = true;
      Serial.println("Forcing publish every 5 minutes...");
    }

    float newTemp = bme.readTemperature() + correction_temp;
    float newHum = bme.readHumidity() + correction_hum;
    float newBaro = bme.readPressure() / 100.0F + correction_baro;

    if (checkBound(newTemp, temp, difftemp) || forceMsg) {
      temp = newTemp;
      Serial.print("New temperature:");
      Serial.println (String(temp) + " degC");
      client.publish(topic_temperature_c, String(temp).c_str(), true);
      blink_blue();
      digitalWrite(blue_led, HIGH);
    }

    if (checkBound(newHum, hum, diffhum) || forceMsg) {
      hum = newHum;
      Serial.print("New humidity:");
      Serial.println(String(hum) + " %");
      client.publish(topic_humidity, String(hum).c_str(), true);

      dewPoint = dewPointFast(temp, hum);
      Serial.print("New dewPoint:");
      Serial.println(String(dewPoint) + " °C");
      client.publish(topic_dewpoint, String(dewPoint).c_str(), true);
      blink_blue();
      digitalWrite(blue_led, HIGH);

    }

    if (checkBound(newBaro, baro, diffbaro) || forceMsg) {
      baro = newBaro;
      Serial.print("New barometer:");
      Serial.println(String(baro) + " hPa");

      client.publish(topic_barometer_hpa, String(baro).c_str(), true);
      blink_blue();
      digitalWrite(blue_led, HIGH);
    }

    forceMsg = false;
  }
}