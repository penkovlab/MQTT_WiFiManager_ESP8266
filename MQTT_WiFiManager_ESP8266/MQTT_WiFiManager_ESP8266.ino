#include <FS.h>                   //this needs to be first, or it all crashes and burns...
#include <WiFiManager.h>
#include <PubSubClient.h>
#include <DallasTemperature.h>
#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson

#define ONE_WIRE_BUS D2
#define DELAY_MILLIS_DS18B20 750

char mqtt_server[40] = "192.168.1.1";
char mqtt_port[6] = "1883";

bool shouldSaveConfig = false;

String Hostname; 

void saveConfigCallback () {
  shouldSaveConfig = true;
}

WiFiManager wm;
WiFiClient espClient;
PubSubClient client(espClient);
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);


#define DS18B20_MAX_DEVICES 10
uint8_t numberOfDevices = 0;
uint32_t lastTempRequest = 0;

typedef struct
{
   DeviceAddress  deviceAddress;
   float          temperature;
} OWI_device;
OWI_device ds18b20Sensors[DS18B20_MAX_DEVICES];

void setup() 
{

  Serial.begin(115200);
  pinMode(0, INPUT_PULLUP);                     // Кнопка FLASH
  pinMode(LED_BUILTIN, OUTPUT);                 // Светодиод
  digitalWrite(LED_BUILTIN, HIGH);


  if (SPIFFS.begin()) {

    if (SPIFFS.exists("/config.json")) {
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
          strcpy(mqtt_server, json["mqtt_server"]);
          strcpy(mqtt_port, json["mqtt_port"]);
        } else {
          Serial.println("failed to load json config");
        }
        configFile.close();
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }

  WiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqtt_server, 40);
  WiFiManagerParameter custom_mqtt_port("port", "mqtt port", mqtt_port, 6);


  wm.setSaveConfigCallback(saveConfigCallback);

  wm.addParameter(&custom_mqtt_server);
  wm.addParameter(&custom_mqtt_port);

  wm.setConfigPortalTimeout(180);

  Hostname = "ESP" + WiFi.macAddress();
  Hostname.replace(":","");
  
  sensors.begin();
  sensors.setWaitForConversion(false);
  sensors.requestTemperatures();
  lastTempRequest = millis();

  WiFi.mode(WIFI_STA); 
  std::vector<const char *> menu = {"wifi"};    // Убираем из меню все кноме настроки WIFI
  wm.setMenu(menu);
  wm.autoConnect(Hostname.c_str());

  strcpy(mqtt_server, custom_mqtt_server.getValue());
  strcpy(mqtt_port, custom_mqtt_port.getValue());

  if (shouldSaveConfig) {
    Serial.println("saving config");

    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();

    json["mqtt_server"] = mqtt_server;
    json["mqtt_port"] = mqtt_port;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

    json.printTo(Serial);
    json.printTo(configFile);

    configFile.close();
  }

  client.setServer(mqtt_server, atoi(mqtt_port));    
}

void loop() 
{
  checkButton();
  
  if (WiFi.status() == WL_CONNECTED){
    led_on();

    if (millis() - lastTempRequest >= DELAY_MILLIS_DS18B20)
    {
      numberOfDevices = sensors.getDeviceCount();
      for (uint8_t i = 0; i < numberOfDevices; i++)
      {
        sensors.getAddress(ds18b20Sensors[i].deviceAddress, i);
        ds18b20Sensors[i].temperature = sensors.getTempCByIndex(i); 
  
        String topic;
        topic += "temp/";
  
         for (uint8_t j = 0; j < 8; j++)
         {
          if (ds18b20Sensors[i].deviceAddress[j] < 16) topic += "0"; 
          topic += String(ds18b20Sensors[i].deviceAddress[j], HEX);
         } 
        
        if (!client.connected()) client.connect("ESP8266");
        client.publish(topic.c_str(), String(ds18b20Sensors[i].temperature).c_str());
        Serial.print(topic);
        Serial.print(" - ");
        Serial.println(ds18b20Sensors[i].temperature);
      }
      sensors.requestTemperatures(); 
      lastTempRequest = millis(); 
    }

  }else{
    led_off();
  }  


}

void checkButton(){
  if(digitalRead(0) == LOW){
    wm.resetSettings();
    ESP.eraseConfig();
    ESP.restart();              
  } 
}

void led_on(void){
  digitalWrite(LED_BUILTIN, LOW);
}
void led_off(void){
  digitalWrite(LED_BUILTIN, HIGH);
}
