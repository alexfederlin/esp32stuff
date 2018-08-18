#include <ESP8266WiFi.h>
#include <Wire.h>
#include <PubSubClient.h>

//Wiring config
#define red_led 5        // D1
#define blue_led 4       // D2

// Pins 0 and 2 have internal pullups
// Reed switches must close against GND
#define backdoor_pin 0   // D3
#define garagedoor_pin 2 // D4

// Time Config
int timezone = 2;
int dst = 0;


// Wifi Config
#define wifi_ssid "Buschfunk"
#define wifi_password "FritzBoxIstTotalSuper"
unsigned char macAddress[6];
char c_macAddress[18];

// MQTT Config
#define mqtt_server "192.168.2.2"
#define mqtt_user "YOUR_MQTT_USERNAME"
#define mqtt_password "YOUR_MQTT_PASSWORD"

// MQTT Topics
char* topic_backdoor;
char* topic_garagedoor;
char* topicprefix = (char*)"sensor/";
char* topic_rssi;

char rssi_buf[3];


// flowcontrol
long lastMsg = 0;
long lastForceMsg = 0;
bool forceMsg = false;

// Initial sensor values
int backdoor = 0;
int garagedoor = 0;
int prev_backdoor = 0;
int prev_garagedoor = 0;

const char* backdoor_char;
const char* garagedoor_char;

WiFiClient espClient;
PubSubClient client(espClient);

void setup() {
  // Setup the two LED ports to use for signaling status
  pinMode(red_led, OUTPUT);
  pinMode(blue_led, OUTPUT);
  digitalWrite(red_led, HIGH);  
  digitalWrite(blue_led, HIGH);  
  Serial.begin(115200);
  digitalWrite(red_led, LOW);  
  digitalWrite(blue_led, LOW);  
 
  // Start sensor
 
  pinMode(backdoor_pin, INPUT);
  pinMode(garagedoor_pin, INPUT);
  // TODO!!

  setup_wifi();
  client.setServer(mqtt_server, 1883);

  //generate topic names based on MAC address
  Serial.println("will post on following topics:");
  topic_backdoor = concat((char*)c_macAddress, "/backdoor");
  topic_garagedoor = concat((char*)c_macAddress, "/garagedoor");
  topic_rssi = concat((char*)c_macAddress, "/rssi");

  digitalWrite(blue_led, HIGH);
  digitalWrite(red_led, LOW);    



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

void loop() {
  if (!client.connected()) {
    reconnect();
    forceMsg = true;
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
   }
   //Post message if anything to post of forced
   //poll the pin every second or so to see if status changed

   backdoor = digitalRead(backdoor_pin);
   garagedoor = digitalRead(garagedoor_pin);

   backdoor_char = (backdoor == 0) ? "closed" : "open";
   garagedoor_char = (garagedoor == 0) ? "closed" : "open";
   

   if ((prev_backdoor != backdoor) || forceMsg) {
    //sendmessage to MQTT
    client.publish(topic_backdoor, backdoor_char, true);
    prev_backdoor = backdoor;
    Serial.print("backdoor: ");
    Serial.println(backdoor_char);
   }

   if ((prev_garagedoor != garagedoor) || forceMsg) {
    //sendmessage to MQTT
    client.publish(topic_garagedoor, garagedoor_char, true);    
    prev_garagedoor = garagedoor;
    Serial.print("garagedoor: ");
    Serial.println(garagedoor_char);
   }

   if (forceMsg) {
    itoa(WiFi.RSSI(), rssi_buf, 10);
    Serial.print("rssi: ");
    Serial.println(rssi_buf);
    client.publish(topic_rssi, rssi_buf, true);
   }

   forceMsg = false;
}