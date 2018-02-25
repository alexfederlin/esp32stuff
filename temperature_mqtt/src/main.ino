#include <Arduino.h>
#include "WiFi.h"
#include <WiFiMulti.h>

//#include <ESP8266WiFi.h>
#include <Wire.h>
#include <PubSubClient.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <time.h>

int timezone = 3;
int dst = 0;

WiFiMulti WiFiMulti;

#define wifi_ssid "iot"
#define wifi_password "iot123456"



#define mqtt_server "192.168.2.2"
#define mqtt_user "YOUR_MQTT_USERNAME"
#define mqtt_password "YOUR_MQTT_PASSWORD"

// In case you have more than one sensor, make each one a different number here
#define sensor_number "2"
#define humidity_topic "sensor/" sensor_number "/humidity/percentRelative"
#define dewpoint_topic "sensor/" sensor_number "/humidity/dewPoint"
#define temperature_c_topic "sensor/" sensor_number "/temperature/degreeCelsius"
#define temperature_f_topic "sensor/" sensor_number "/temperature/degreeFahrenheit"
#define barometer_hpa_topic "sensor/" sensor_number "/barometer/hectoPascal"
#define barometer_inhg_topic "sensor/" sensor_number "/barometer/inchHg"
// Lookup for your altitude and fill in here, units hPa
// Positive for altitude above sea level
//#define baro_corr_hpa 34.5879 // = 289m above sea level
#define baro_corr_hpa 0

#define red_led 0
#define blue_led 2

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
  // Set SDA and SDL ports
  //Wire.begin(SDA, SCL);
  //for 8266
  //Wire.begin(4, 5);
  // for ESP32
  Wire.begin(21, 22);
  blink_red;
  delay(1000);
  // Using I2C on the HUZZAH board SCK=#5, SDI=#4 by default

  // Start sensor
  while (!bme.begin()) {
    Serial.println("Could not find a valid BME280 sensor, check wiring!");
    blink_red();
    delay(1000);
  }


  setup_wifi();
  client.setServer(mqtt_server, 1883);
  digitalWrite(blue_led, HIGH);
  digitalWrite(red_led, LOW);    

  configTime(3 * 3600, 0, "pool.ntp.org", "time.nist.gov");
  Serial.println("\nWaiting for time");
  while (!time(nullptr)) {
    Serial.print(".");
    delay(1000);
  }
  Serial.println("");

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

  // WiFi.begin(wifi_ssid, wifi_password);

  // while (WiFi.status() != WL_CONNECTED) {
  //   blink_blue();
  //   delay(480);
  //   Serial.print(".");
  // }

  WiFiMulti.addAP(wifi_ssid, wifi_password);
  while(WiFiMulti.run() != WL_CONNECTED) {
      Serial.print(".");
      delay(500);
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    // If you do not want to use a username and password, change next line to
    // if (client.connect("ESP8266Client")) {
//    if (client.connect("ESP8266Client", mqtt_user, mqtt_password)) {
    if (client.connect("ESP32Client")) {
      Serial.println("connected");
      digitalWrite(red_led, LOW);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      blink_red();
      delay(200);
      blink_red();
      delay(4760);
    }
  }
}

bool checkBound(float newValue, float prevValue, float maxDiff) {
  return newValue < prevValue - maxDiff || newValue > prevValue + maxDiff;
}

long lastMsg = 0;
long lastForceMsg = 0;
bool forceMsg = false;
float temp = 0.0;
float hum = 0.0;
float dewPoint = 0.0;
float baro = 0.0;
float difftemp = 0.1;
float diffhum = 1;
float diffbaro = 0.5;

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

// std::string createJson(float value, String unit){
//   time_t now = time(nullptr); //uint32_t
//   // String time = ctime(&now);
//   Serial.println(ctime(&now));
//   char valchar[6];
//   dtostrf(value, 4, 2, valchar);
//   std::string json(std::string("{\"time\": ")+ std::string(ctime(&now)) +", \"value\": "+ std::string(valchar) +", \"unit\": \""+ std::string(unit.c_str()) +"\"}");
//   return json;
// }

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

    float newTemp = bme.readTemperature();
    float newHum = bme.readHumidity();
    float newBaro = bme.readPressure() / 100.0F;
    // std::string temp_c_str;
    // std::string temp_f_str;
    if (checkBound(newTemp, temp, difftemp) || forceMsg) {
      temp = newTemp;
      float temp_c=temp; // Celsius
      float temp_f=temp*1.8F+32.0F; // Fahrenheit
      // std::string temp_c_str(createJson(temp_c, "degC"));
      // std::string temp_f_str(createJson(temp_f, "degF"));
      Serial.print("New temperature:");
      Serial.print(String(temp_c) + " degC   ");
      Serial.println(String(temp_f) + " degF");
      // Serial.println(temp_c_str);
      client.publish(temperature_c_topic, String(temp_c).c_str(), true);
      // client.publish(temperature_c_topic, temp_c_str.c_str(), true);
      // client.publish(temperature_f_topic, String(temp_f).c_str(), true);
      blink_blue();
    }

    if (checkBound(newHum, hum, diffhum) || forceMsg) {
      hum = newHum;
      Serial.print("New humidity:");
      Serial.println(String(hum) + " %");
      client.publish(humidity_topic, String(hum).c_str(), true);
      blink_blue();
      dewPoint = dewPointFast(temp, hum);
      Serial.print("New dewPoint:");
      Serial.println(String(dewPoint) + " °C");
      client.publish(dewpoint_topic, String(dewPoint).c_str(), true);
    }

    if (checkBound(newBaro, baro, diffbaro) || forceMsg) {
      baro = newBaro;
      float baro_hpa=baro+baro_corr_hpa; // hPa corrected to sea level
      float baro_inhg=(baro+baro_corr_hpa)/33.8639F; // inHg corrected to sea level
      Serial.print("New barometer:");
      Serial.print(String(baro_hpa) + " hPa   ");
      Serial.println(String(baro_inhg) + " inHg");
      client.publish(barometer_hpa_topic, String(baro_hpa).c_str(), true);
      client.publish(barometer_inhg_topic, String(baro_inhg).c_str(), true);
      blink_blue();
    }

    forceMsg = false;
  }
}