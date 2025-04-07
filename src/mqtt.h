#ifndef MQTT_H
#define MQTT_H

#include <PubSubClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <LiquidCrystal_I2C.h>
#include <time.h>

// ===================== Cấu hình MQTT =====================
extern const char* mqtt_server;
extern const char* mqtt_user;
extern const char* mqtt_pass;
extern const int mqtt_port;
extern const char* mqtt_topic_write;
extern const char* mqtt_topic_user;
extern const char* mqtt_topic_book;

// ===================== Biến toàn cục =====================
extern String writeData;
extern unsigned long writeDataTimestamp;

// Enum và biến trạng thái
enum State { READ, WRITE };
extern State currentState;

// ===================== Hàm timestamp =====================
String getCurrentTimestamp() {
    time_t now = time(nullptr);
    struct tm* timeinfo = localtime(&now);
    char buffer[25];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);
    return String(buffer);
}

// ===================== MQTT Client =====================
WiFiClientSecure espClient;
PubSubClient mqttClient(espClient);

// ===================== Callback =====================
void callback(char* topic, byte* message, unsigned int length) {
    Serial.print("Received MQTT: ");
    Serial.println(topic);

    String messageStr;
    for (unsigned int i = 0; i < length; i++) {
        messageStr += (char)message[i];
    }

    if (String(topic) == mqtt_topic_write) {
        Serial.println("Content: " + messageStr);
        writeData = messageStr;
        writeDataTimestamp = millis();
        currentState = WRITE;
    }
}

// ===================== Setup MQTT =====================
void setupMQTT() {
    espClient.setInsecure(); // Dùng WiFiClientSecure nhưng bỏ xác thực
    mqttClient.setServer(mqtt_server, mqtt_port);
    mqttClient.setCallback(callback);
}

// ===================== Reconnect MQTT =====================
void reconnectMqtt(LiquidCrystal_I2C& lcd) {
    while (!mqttClient.connected()) {
        Serial.print("Attempting MQTT connection...");
        lcd.clear();
        lcd.print("Connecting MQTT");

        String clientID = "ESPClient-" + String(random(0xffff), HEX);

        if (mqttClient.connect(clientID.c_str(), mqtt_user, mqtt_pass)) {
            Serial.println("Connected");
            lcd.clear();
            lcd.print("MQTT connected");

            mqttClient.subscribe(mqtt_topic_write);
            mqttClient.subscribe(mqtt_topic_user);
            mqttClient.subscribe(mqtt_topic_book);
        } else {
            Serial.print("Failed, rc=");
            Serial.println(mqttClient.state());
            lcd.clear();
            lcd.print("MQTT failed!");
            delay(5000);
        }
    }
}

// ===================== Gửi dữ liệu UID =====================
void sendIdMqtt(const String& topic, const String& id, const String& uid) {
    if (!mqttClient.connected()) return;

    StaticJsonDocument<256> doc;
    doc["id"] = id;
    doc["uid"] = uid;
    doc["timestamp"] = getCurrentTimestamp();

    char jsonBuffer[300];
    serializeJson(doc, jsonBuffer);

    mqttClient.publish(topic.c_str(), jsonBuffer);
}

#endif
