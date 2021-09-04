/**
The MIT License (MIT)

Copyright (c) 2021 Roman Rachkov

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
/**
   connect the sensor as follows :
          Pin 2 of dust sensor PM1.0      -> gpio pin 12 esp8266
    Pin 3 of dust sensor          -> +5V 
    Pin 4 of dust sensor PM2.5    -> gpio pin 13 esp8266
    Pin 5 of dust sensor          -> Ground
  Datasheet: http://www.samyoungsnc.com/products/3-1%20Specification%20DSM501.pdf
*/
#include <FS.h> 
#include <ArduinoJson.h>
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include "SparkFunHTU21D.h"
#include"DSM501.h"
#ifdef ESP32
  #include <SPIFFS.h>
#endif

#define DSM501_PM10 12
#define DSM501_PM25 13

DSM501 dsm501(DSM501_PM10, DSM501_PM25);
HTU21D myHumidity;

const int mqtt_port = 1883;
//const char *mqtt_broker = "mqtt.beebotte.com";
//const char *mqtt_pass = "";

char mqtt_broker[40] = "agrosens.xyz";
char mqtt_topic[40] = "sensor/pm10";
char mqtt_topic1[40] = "sensor/pm2.5";
char mqtt_topic2[40] = "sensor/temp";
char mqtt_topic3[40] = "sensor/humd";
char mqtt_user[40] = "";
char mqtt_pass[40] = "";


WiFiClient espClient;
PubSubClient client(espClient);

bool shouldSaveConfig = false;
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void setupSpiffs(){
  //clean FS, for testing
  // SPIFFS.format();

  //read configuration from FS json
  Serial.println("mounting FS...");

  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          Serial.println("\nparsed json");

         strcpy(mqtt_broker, json["mqtt_broker"]);
        //  char mqtt_port=Character.forDigit(port);
       //   strcpy(port, json["port"]);
          strcpy(mqtt_topic, json["mqtt_topic"]);
          strcpy(mqtt_topic1, json["mqtt_topic1"]);
           strcpy(mqtt_topic2, json["mqtt_topic2"]);
          strcpy(mqtt_topic3, json["mqtt_topic3"]);
          strcpy(mqtt_user, json["mqtt_user"]);
         strcpy(mqtt_pass, json["mqtt_pass"]);

          // if(json["ip"]) {
          //   Serial.println("setting custom ip from config");
          //   strcpy(static_ip, json["ip"]);
          //   strcpy(static_gw, json["gateway"]);
          //   strcpy(static_sn, json["subnet"]);
          //   Serial.println(static_ip);
          // } else {
          //   Serial.println("no custom ip in config");
          // }

        } else {
          Serial.println("failed to load json config");
        }
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }
  //end read
}

void setup() {
    WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP

    // put your setup code here, to run once:
    Serial.begin(115200);
    
    // WiFi.mode(WiFi_STA); // it is a good practice to make sure your code sets wifi mode how you want it.
     setupSpiffs();
    //WiFiManager, Local intialization. Once its business is done, there is no need to keep it around
    WiFiManager wm;

    wm.setSaveConfigCallback(saveConfigCallback);

    //reset settings - wipe credentials for testing
    //wm.resetSettings();
    WiFiManagerParameter custom_mqtt_broker("broker", "mqtt broker", mqtt_broker, 40);
   wm.addParameter(&custom_mqtt_broker);
  //  WiFiManagerParameter custom_port("port", "port", port, 6);
  //  wm.addParameter(&custom_port);
    WiFiManagerParameter custom_mqtt_topic("topic", "mqtt topic", mqtt_topic, 40);
    wm.addParameter(&custom_mqtt_topic);
    WiFiManagerParameter custom_mqtt_topic1("topic1", "mqtt topic1", mqtt_topic1, 40);
    wm.addParameter(&custom_mqtt_topic1);
 WiFiManagerParameter custom_mqtt_topic2("topic2", "mqtt topic2", mqtt_topic2, 40);
    wm.addParameter(&custom_mqtt_topic1);
 WiFiManagerParameter custom_mqtt_topic3("topic3", "mqtt topic3", mqtt_topic3, 40);
    wm.addParameter(&custom_mqtt_topic3);
   WiFiManagerParameter custom_mqtt_user("user", "mqtt user", mqtt_user, 40);
    wm.addParameter(&custom_mqtt_user);
WiFiManagerParameter custom_mqtt_pass("password", "mqtt pass", mqtt_pass, 40);
    wm.addParameter(&custom_mqtt_pass);

    // Automatically connect using saved credentials,
    // if connection fails, it starts an access point with the specified name ( "AutoConnectAP"),
    // if empty will auto generate SSID, if password is blank it will be anonymous AP (wm.autoConnect())
    // then goes into a blocking loop awaiting configuration and will return success result

    bool res;
    // res = wm.autoConnect(); // auto generated AP name from chipid
    // res = wm.autoConnect("AutoConnectAP"); // anonymous ap
    res = wm.autoConnect("AutoConnectAP","password"); // password protected ap

    if(!res) {
        Serial.println("Failed to connect");
        // ESP.restart();
    } 
    else {
        //if you get here you have connected to the WiFi    
        Serial.println("connected...yeey :)");
    }
 while (WiFi.status() != WL_CONNECTED) {
     delay(500);
     Serial.println("Connecting to WiFi..");
 }
 Serial.println("Connected to the WiFi network");
strcpy(mqtt_broker, custom_mqtt_broker.getValue());

 // strcpy(port, custom_port.getValue());
  strcpy(mqtt_topic, custom_mqtt_topic.getValue());
   strcpy(mqtt_topic1, custom_mqtt_topic1.getValue());
   strcpy(mqtt_topic2, custom_mqtt_topic2.getValue());
   strcpy(mqtt_topic3, custom_mqtt_topic3.getValue());
   strcpy(mqtt_user, custom_mqtt_user.getValue());
    strcpy(mqtt_pass, custom_mqtt_pass.getValue());

if (shouldSaveConfig) {
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["mqtt_broker"] = mqtt_broker;
   //json["port"]   = port;
    json["mqtt_topic"]   = mqtt_topic;
json["mqtt_topic1"]   = mqtt_topic1;
json["mqtt_topic2"]   = mqtt_topic2;
json["mqtt_topic3"]   = mqtt_topic3;
    json["mqtt_user"]   = mqtt_user;
   json["mqtt_pass"]   = mqtt_pass;

    // json["ip"]          = WiFi.localIP().toString();
    // json["gateway"]     = WiFi.gatewayIP().toString();
    // json["subnet"]      = WiFi.subnetMask().toString();

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

    json.prettyPrintTo(Serial);
    json.printTo(configFile);
    configFile.close();
    //end save
    shouldSaveConfig = false;
  }

//const uint16_t mqtt_port_x = 1883;
 client.setServer(mqtt_broker, mqtt_port);
 //client.setCallback(callback);
 while (!client.connected()) {
     String client_id = "esp8266-client-";
     client_id += String(WiFi.macAddress());
     Serial.printf("The client %s connects to the public mqtt broker\n", client_id.c_str());
     if (client.connect(client_id.c_str(),mqtt_user,mqtt_pass)) {
         Serial.println("Public emqx mqtt broker connected");
     } else {
         Serial.print("failed with state ");
         Serial.print(client.state());
         delay(2000);
     }
 }

  myHumidity.begin();
  // Initialize DSM501
  dsm501.begin(MIN_WIN_SPAN);

  // wait 60s for DSM501 to warm up
  for (int i = 1; i <= 60; i++)
  {
    delay(1000); // 1s
    Serial.print(i);
    Serial.println(" s (wait 60s for DSM501 to warm up)");
  }
}


void loop()
{
  // call dsm501 to handle updates.
  dsm501.update();
  float humd = myHumidity.readHumidity();
  float temp = myHumidity.readTemperature();
  
  // get PM density of particles over 1.0 μm
  Serial.print("PM10: ");
  Serial.print(dsm501.getParticalWeight(0));
  Serial.println(" ug/m3");
  
  // get PM density of particles over 2.5 μm
  Serial.print("PM25: ");
  Serial.print(dsm501.getParticalWeight(1));
  Serial.println(" ug/m3");
  
  Serial.print("AQI: ");
  Serial.println(dsm501.getAQI());
  
  // get PM2.5 density of particles between 1.0~2.5 μm
  Serial.print("PM2.5: ");
  Serial.print(dsm501.getPM25());
  Serial.println(" ug/m3");

  Serial.print(" Temperature:");
  Serial.print(temp, 1);
  Serial.print("C");
  Serial.print(" Humidity:");
  Serial.print(humd, 1);
  Serial.print("%");

  Serial.println();
    client.publish(mqtt_topic, String(dsm501.getParticalWeight(0)).c_str(), true); 
     client.publish(mqtt_topic1, String(dsm501.getPM25()).c_str(), true);
       client.publish(mqtt_topic2, String(temp).c_str(), true); 
     client.publish(mqtt_topic3, String(humd).c_str(), true);
  delay(1000);
}
