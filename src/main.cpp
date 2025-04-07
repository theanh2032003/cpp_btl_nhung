#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <time.h>

#include "wifi_module.h"
#include "mqtt.h"
#include "lcd.h"
#include "rfid.h"

// Config
// const char* ssid = "TP-Link_B510";
// const char* password = "50447430";

// const char* ssid = "iPhone";
// const char* password = "00000000";

const char* ssid = "Tuan Them";
// const char* password = "";

const char* mqtt_server = "e7b9bebcbf214dcfa2de218f51a8ee97.s1.eu.hivemq.cloud";
const char* mqtt_user = "B21DCCN004";
const char* mqtt_pass = "Theanh2032003";
const int mqtt_port = 8883;
const char* mqtt_topic_write = "rfid/write";
const char* mqtt_topic_write_success = "rfid/write_success";
const char* mqtt_topic_user = "rfid/user";
const char* mqtt_topic_book = "rfid/book";

LiquidCrystal_I2C lcd(0x27, 16, 2);

// State
// enum State { READ, WRITE };
State currentState = READ;
String writeData = "";
unsigned long writeDataTimestamp = 0;

void setup() {
    Serial.begin(115200);

    Wire.begin(16, 17);
    lcd.begin(16, 2);
    lcd.backlight();
    lcd.print("RFID Ready...");

    setupWifi(lcd);
    setupMQTT();
    initRFID();

    configTime(7 * 3600, 0, "pool.ntp.org");
    Serial.println("Place RFID card to read or write...");
}

void loop() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("⚠️ Lost WiFi! Reconnecting...");
        displayLCD(lcd, {"WiFi lost!", "Reconnecting..."});
        setupWifi(lcd);
    }

    if (!mqttClient.connected()) {
        reconnectMqtt(lcd);
    }
    mqttClient.loop();

    if (!writeData.isEmpty() && millis() - writeDataTimestamp > 30000) {
        Serial.println("⚠️ writeData expired!");
        displayLCD(lcd, {"Write expired!"});
        writeData = "";
        currentState = READ;
    }

    if (isCardPresent()) {
        if (currentState == WRITE && !writeData.isEmpty()) {
            if (writeID(writeData.c_str())) {
                Serial.println("✅ id: " + writeData);
                String uid = readUID();
                // String id = readID();
                sendIdMqtt(mqtt_topic_write_success, writeData, uid);
                Serial.println("✅ Write successful!");
                displayLCD(lcd, {"Write success!", writeData});
            } else {
                Serial.println("❌ Write failed!");
                displayLCD(lcd, {"Write failed!"});
            }
            writeData = "";
        } else {
            String id = readID();
            String uid = readUID();
            if (id.isEmpty() || uid.isEmpty()) {
                Serial.println("❌ Failed to read card or UID!");
                displayLCD(lcd, {"Read failed!"});
                return;
            }
            String topic = id.charAt(0) == 'U' ? mqtt_topic_user : mqtt_topic_book;
            sendIdMqtt(topic, id, uid);
            
            displayLCD(lcd, {"Read success!", id});
        }
    }
}
