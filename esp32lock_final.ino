#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

#define LED 2
const int AIN1 = 5;
const int AIN2 = 4;
const int PWMA = 15;

// WiFi配置
const char* id = "24371068";
const char* password = "sfz060330";
const char* ssid = "BUAA-Mobile";

// 阿里云配置
#define PRODUCT_KEY "a1C6qtpIPNw"
#define DEVICE_NAME "esp32lock"
#define DEVICE_SECRET "545b0a262040c08b0828789f0ec0c4d0"
#define REGION_ID "cn-shanghai"

// MQTT配置
#define MQTT_SERVER PRODUCT_KEY ".iot-as-mqtt." REGION_ID ".aliyuncs.com"
#define MQTT_PORT 1883
#define MQTT_USRNAME DEVICE_NAME "&" PRODUCT_KEY
#define CLIENT_ID "a1C6qtpIPNw.esp32lock|securemode=2,signmethod=hmacsha256,timestamp=1739765535894|"
#define MQTT_PASSWD "97023086d04dd9a1177a930e57b7a4b2c06bf47a1391a0c56b993bba6c5c9ad0"

// MQTT主题
#define ALINK_TOPIC_PROP_SET "/sys/" PRODUCT_KEY "/" DEVICE_NAME "/thing/service/property/set"

// 定位平台配置
const char* api_url = "https://api.newayz.com/location/hub/v1/track_points?access_key=pQ6wbfTg0bDMCyBDnQ8L91ikAocfmVB3";
String post_id = "1675840238995-****************";
String asset_id = "8bedcf60-0005-11f0-a3c6-03139935df17";
String manufacturer = "esp32-s2";

WiFiClient espClient;
PubSubClient client(espClient);

// 状态标志
DynamicJsonDocument wifi_doc(4096);
bool getLocation = false;
bool locationObtained = false;  // 新增定位成功标志

void wifiInit() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, WPA2_AUTH_PEAP, "", id, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.println("WiFi not Connect");
    }
    Serial.println("WiFi Connected");
}

void wifi_scan() {
  int n = WiFi.scanNetworks();
  Serial.println("\n=== 开始WiFi扫描 ===");
  Serial.printf("发现 %d 个网络\n", n);

  wifi_doc.clear();
  wifi_doc["id"] = post_id;
  wifi_doc["asset"]["id"] = asset_id;
  wifi_doc["asset"]["manufacturer"] = manufacturer;
  wifi_doc["timestamp"] = millis();
  
  JsonObject location = wifi_doc.createNestedObject("location");
  location["timestamp"] = millis();
  
  JsonArray wifis = location.createNestedArray("wifis");
  
  for (int i=0; i<n; i++) {
    JsonObject wifi = wifis.createNestedObject();
    wifi["macAddress"] = WiFi.BSSIDstr(i);
    wifi["signalStrength"] = WiFi.RSSI(i);
    
    Serial.printf("[AP%d] MAC: %s, RSSI: %d\n", 
                 i+1, WiFi.BSSIDstr(i).c_str(), WiFi.RSSI(i));
  }
  Serial.println("=== 扫描完成 ===");
}

void get_location() {
  HTTPClient http;
  http.begin(api_url);
  http.addHeader("Content-Type", "application/json");

  String payload;
  serializeJson(wifi_doc, payload);
  
  Serial.println("\n发送定位请求：");
  Serial.println(payload);

  int httpCode = http.POST(payload);
  if (httpCode == HTTP_CODE_OK) {
    String response = http.getString();
    Serial.println("收到响应：");
    Serial.println(response);
    
    DynamicJsonDocument res(1024);
    DeserializationError error = deserializeJson(res, response);
    if (!error) {
      if (res.containsKey("location")) {
        String address = res["location"]["address"]["name"].as<String>();
        if (!address.isEmpty()) {
          Serial.print("定位成功：");
          Serial.println(address);
          locationObtained = true;  // 设置成功标志
        }
      }
    }
  }
  http.end();
}

void mqttCheckConnect() {
    if (!client.connected()) {
        Serial.println("Connecting to MQTT Server ...");
        if (client.connect(CLIENT_ID, MQTT_USRNAME, MQTT_PASSWD)) {
            Serial.println("MQTT Connected!");
            client.subscribe(ALINK_TOPIC_PROP_SET);
        } else {
            Serial.print("MQTT Connect err:");
            Serial.println(client.state());
            delay(5000);
        }
    }
}

void callback(char* topic, byte* payload, unsigned int length) {
    Serial.print("Message arrived [");
    Serial.print(topic);
    Serial.print("] ");
    for (int i = 0; i < length; i++) {
        Serial.print((char)payload[i]);
    }
    Serial.println();

    DynamicJsonDocument doc(1024);
    deserializeJson(doc, String((char*)payload));

    if (doc["params"].containsKey("deng")) {
        Serial.println("GOT DENG CMD");
        digitalWrite(LED, doc["params"]["deng"]); 
    }
    if (doc["params"].containsKey("nav")) {
        Serial.println("GOT NAVIGATE CMD");
        getLocation = (doc["params"]["nav"] == 1);
        locationObtained = !getLocation;  // 重置定位标志
        Serial.printf("定位状态: %s\n", getLocation ? "开启" : "关闭");
    }
    if (doc["params"].containsKey("suo")) {
        Serial.println("GOT LOCK CMD");
        if (doc["params"]["suo"] == 1) {
            digitalWrite(AIN1, HIGH);
            digitalWrite(AIN2, LOW);
            analogWrite(PWMA, 255);
            delay(2225);
            digitalWrite(AIN1, LOW);
            digitalWrite(AIN2, LOW);
            
        } else {
          digitalWrite(AIN1, LOW);
          digitalWrite(AIN2, LOW);
        }
    }
}

void setup() {
    pinMode(LED, OUTPUT);
    pinMode(AIN1, OUTPUT);
    pinMode(AIN2, OUTPUT);
    pinMode(PWMA, OUTPUT);
    Serial.begin(115200);
    wifiInit();
    client.setServer(MQTT_SERVER, MQTT_PORT);
    client.setCallback(callback);
}

void loop() {
    mqttCheckConnect();
    client.loop();
    
    if (getLocation && !locationObtained) {  // 修改判断条件
        wifi_scan();
        get_location();
        delay(5000);
    } else {
        delay(2000);
    }
}
//----------------------------------------------------------------