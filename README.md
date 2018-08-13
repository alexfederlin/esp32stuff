# Temperatursensor mit ESP8266

## Hardware

- [Wemos D1 miniPro](https://arduino-projekte.info/wemos-d1-mini-pro/)
- [BME-280 Temperatur-/Feuchtigkeits-/Luftdrucksensor](https://learn.adafruit.com/adafruit-bme280-humidity-barometric-pressure-temperature-sensor-breakout/downloads)

## Software

- [platformio](https://platformio.org/)
  - [Code für ESP8266](https://github.com/alexfederlin/esp32stuff/tree/master/temp_mqtt8266)
- [MQTT Broker](https://mosquitto.org/) [(auch auf Docker)](https://hub.docker.com/_/eclipse-mosquitto/)
- Docker
  - [NodeRed](https://hub.docker.com/r/nodered/node-red-docker/)
  - [InfluxDb](https://hub.docker.com/_/influxdb/)
  - [Grafana](https://hub.docker.com/r/grafana/grafana/)


## Allgemeine Beschreibung
### MQTT Broker

Bildet die Platform der Kommunikation zwischen Microcontroller und NodeRed
Bietet eine Publish Subscribe Mechanismus zum Austausch von Nachrichten in einem beliebig gestaltbaren hierarchischen Sytem von Topics.
Hilfreich zur Entwicklung ist ein stand-alone MQTT Client, z.B. [MQTTBox](http://workswithweb.com/mqttbox.html)

### Microcontroller 
- verbindet sich mit dem WLAN
- verbindet sich mit dem MQTT Broker
- Aboniert Topics auf dem MQTT Broker um Korrekturwerte für Sensordaten entgegen zu nehmen
- liest den Temperatur-/Feuchtigkeits-/Luftdrucksensor aus
- Published die ausgelesenen Werte auf individuelle MQTT Topics (enthält MAC Addresse des Microcontrollers im Namen)

### Platformio

Dient zum Aufspielen der Software auf den Microcontroller

### NodeRed
- Verbindet sich auf den MQTT Broker
- subscribed sich auf alle topics auf denen Sensordaten veröffentlicht werden
- liest die Daten aus und schreibt sie in die InfluxDB, versehen mit einem Tag für die Location, welcher Anhand des Topicnamen bestimmt wird

### InfluxDB
- Timeseries Datenbank
- Speicherung der von NodeRed aufbereiteten und angelieferten Messwerte 

### Grafana
- verbindet sich mit der InfluxDB und stellt die Daten dar

## Platformio
### Setup

platformio ist ein python Paket.
Compilieren für ein Projekt wird komplett über platformio.ini im Projektverzeichnis gesteuert.
Hier werden auch alle Libraries angegeben, die eingebunden werden.

### Usage

Device finden

    pio device list

Build directory säubern, Sketch compilieren und hochladen, danach anfangen auf dem seriellen Device zu lauschen

    platformio run --target clean && platformio run --target upload && platformio device monitor -b 115200
    
## Code für Microcontroller
### Notwendige Anpassungen
- WiFi SSID und Passwort
- IP Addresse für MQTT Broker

## InfluxDB (Docker)
### Install
    sudo docker pull influxdb
### Start
    sudo docker run -p 8083:8083 -p 8086:8086 -v <path to influxDB data on host>:/var/lib/influxdb influxdb

### Usage
Create a DB: 

    curl -G http://localhost:8086/query --data-urlencode "q=CREATE DATABASE environment"

Access the Database CLI:

    sudo docker exec -it <name of docker container> influx

Influx queries: 

    > select * from environment
    > SELECT "value" FROM "autogen"."temp" WHERE ("measurement" = 'dewPoint') AND time >= 1516834800000 and time <= 1516921199999


### Setup:
- all measurements go into the DB "environment". 
- measurements can be: temp, dewpt, pressure
- information about where the measurements were taken are in tag-key: location
- information about sensor are in tag-key: sensor
- information about unit are in tag-key: unit
- the actually measured value is in field-key: value
- this is pieced together in node red by reading from the MQTT Server

## NodeRed
### Install
    docker pull nodered/node-red-docker
### Run
    sudo docker run -it -p 1880:1880 -v <path to data directory on host>:/data nodered/node-red-docker

### Access

    http://localhost:1880
### Code

[NodeRed Flows](https://github.com/alexfederlin/esp32stuff/tree/master/nodered)

## Grafana: 
### Install
    docker pull grafana/grafana
### Run
    sudo docker run -d -p 3210:3000 --link <name of influx container>:influxdb --name grafana
### Access

http://localhost:3210 (u/p: admin/admin)

### Setup

Data Source konfigurieren (http://localhost:8086)
Dashboard erstellen und Rows konfigurieren


![](https://d2mxuefqeaa7sj.cloudfront.net/s_8209757245AA9E8AFCEB69AF91F5DC5177A88B303B7853E8FB49343D4E6016F6_1530220623800_image.png)

![](https://d2mxuefqeaa7sj.cloudfront.net/s_8209757245AA9E8AFCEB69AF91F5DC5177A88B303B7853E8FB49343D4E6016F6_1530220673435_image.png)

![](https://d2mxuefqeaa7sj.cloudfront.net/s_8209757245AA9E8AFCEB69AF91F5DC5177A88B303B7853E8FB49343D4E6016F6_1530220743493_image.png)

