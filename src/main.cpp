#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <vector>
#include <WiFiClientSecure.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <time.h>

// Time configuration
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 7 * 3600;
const int   daylightOffset_sec = 0;

// WiFi & MQTT Configuration
const char* ssid = "TP-Link_B510";
const char* password = "50447430";
const char* mqtt_server = "e7b9bebcbf214dcfa2de218f51a8ee97.s1.eu.hivemq.cloud";
const char* mqtt_user = "B21DCCN004";
const char* mqtt_pass = "Theanh2032003";
const int mqtt_port = 8883;
const char* mqtt_topic_write = "rfid/write";
const char* mqtt_topic_user = "rfid/user";
const char* mqtt_topic_book = "rfid/book";

WiFiClientSecure espClient;
PubSubClient mqttClient(espClient);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 7 * 3600, 60000);

// RFID Configuration
#define RST_PIN 5
#define SS_PIN  4
MFRC522 mfrc522(SS_PIN, RST_PIN);
MFRC522::MIFARE_Key key;

// LCD Configuration (I2C 16x2)
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Read/Write State
enum State { READ, WRITE };
State currentState = READ;
String writeData = "";
unsigned long writeDataTimestamp = 0;  // Thời điểm nhận message từ MQTT

// Function Prototypes
void setupWifi();
void setupMQTT();
void reconnectMqtt();
void callback(char* topic, byte* message, unsigned int length);
bool isCardPresent();
bool writeID(const char* input);
String readID();
void displayLCD(const std::vector<String>& dataList, int duration = 2000);
void sendIdMqtt(String topic, String id);
time_t getCurrentTimestamp();

void setup() {
    Serial.begin(115200);
    setupWifi();
    setupMQTT();
    SPI.begin();
    mfrc522.PCD_Init();

    // Setup LCD
    Wire.begin(16, 17);
    lcd.begin(16, 2);
    lcd.backlight();
    lcd.print("RFID Ready...");
    Serial.println("Place RFID card to read or write...");

    // Initialize default key (0xFF)
    memset(key.keyByte, 0xFF, sizeof(key.keyByte));

    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
}

void loop() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("⚠️ Lost WiFi connection! Reconnecting...");
        displayLCD({"WiFi lost!", "Reconnecting..."});
        setupWifi();
    }

    if (!mqttClient.connected()) {
        reconnectMqtt();
    }
    mqttClient.loop();

    // Kiểm tra thời gian tồn tại của writeData (30s)
    if (!writeData.isEmpty() && millis() - writeDataTimestamp > 30000) {
        Serial.println("⚠️ writeData expired!");
        displayLCD({"Write expired!"});
        writeData = ""; // Reset write data
        currentState = READ;
    }

    if (isCardPresent()) {
        if (currentState == WRITE && !writeData.isEmpty()) {
            if (writeID(writeData.c_str())) {
                Serial.println("✅ Write successful!");
                displayLCD({"Write success!", writeData});
            } else {
                Serial.println("❌ Write failed!");
                displayLCD({"Write failed!"});
            }
            writeData.clear(); // Reset write data
        } else {
            String id = readID();
            if (id.isEmpty()) {
                Serial.println("❌ Failed to read card!");
                displayLCD({"Read failed!"});
                return;
            }

            char firstChar = id.charAt(0);
            String topic = firstChar == 'U' ? mqtt_topic_user : mqtt_topic_book;
            sendIdMqtt(topic, id);
            displayLCD({"Read success!", id});
        }
    }
}

void setupWifi() {
    Serial.println();
    Serial.print("Connecting to ");
    displayLCD({"Connecting WiFi"});
    Serial.println(ssid);

    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("\nWiFi connected");
    displayLCD({"WiFi connected"});
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
}

void setupMQTT() {
    espClient.setInsecure();
    mqttClient.setServer(mqtt_server, mqtt_port);
    mqttClient.setCallback(callback);
}

void reconnectMqtt() {
    while (!mqttClient.connected()) {
        Serial.print("Attempting MQTT connection...");
        displayLCD({"Connecting MQTT"});
        String clientID = "ESPClient-";
        clientID += String(random(0xffff), HEX);

        if (mqttClient.connect(clientID.c_str(), mqtt_user, mqtt_pass)) {
            Serial.println("Connected");
            displayLCD({"MQTT connected"});
            if (mqttClient.subscribe(mqtt_topic_write)) {
                Serial.println("Subscribed to topic_write");
                displayLCD({"Subscribed!", mqtt_topic_write});
            }
            if (mqttClient.subscribe(mqtt_topic_user)) {
                Serial.println("Subscribed to topic_user");
                displayLCD({"Subscribed!", mqtt_topic_user});
            }
            if (mqttClient.subscribe(mqtt_topic_book)) {
                Serial.println("Subscribed to topic_book");
                displayLCD({"Subscribed!", mqtt_topic_book});
            }
        } else {
            Serial.print("Failed, rc=");
            Serial.println(mqttClient.state());
            displayLCD({"MQTT failed!", "Retrying..."});
            delay(5000);
        }
    }
}

void callback(char* topic, byte* message, unsigned int length) {
    Serial.print("Received MQTT: ");
    Serial.println(topic);

    String messageStr;
    for (unsigned int i = 0; i < length; i++) {
        messageStr += (char)message[i];
    }

    if (topic == mqtt_topic_write ) {
        Serial.println("Content: " + messageStr);
        writeData = messageStr;  // Lưu dữ liệu để ghi vào thẻ
        writeDataTimestamp = millis();  // Ghi lại thời gian nhận message
        currentState = WRITE;
        displayLCD({"MQTT received!", messageStr});        
    }

}

bool isCardPresent() {
    return mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial();
}

bool writeID(const char* input) {
    byte block = 1;
    byte dataBlock[16] = {0};

    int len = strlen(input);
    if (len > 16) len = 16;
    memcpy(dataBlock, input, len);

    if (mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, block, &key, &(mfrc522.uid)) != MFRC522::STATUS_OK) {
        Serial.println("Auth failed!");
        return false;
    }

    if (mfrc522.MIFARE_Write(block, dataBlock, 16) != MFRC522::STATUS_OK) {
        Serial.println("Write failed!");
        return false;
    }

    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();
    return true;
}

String readID() {
    byte block = 1;
    byte buffer[18];
    byte size = sizeof(buffer);

    if (mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, block, &key, &(mfrc522.uid)) != MFRC522::STATUS_OK) {
        Serial.println("Auth failed!");
        return "";
    }

    if (mfrc522.MIFARE_Read(block, buffer, &size) != MFRC522::STATUS_OK) {
        Serial.println("Read failed!");
        return "";
    }

    String cardID;
    for (int i = 0; i < 4; i++) {
        cardID += String(buffer[i], HEX);
    }
    cardID.toUpperCase();

    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();
    return cardID;
}

void sendIdMqtt(String topic, String id) {
    if (!mqttClient.connected()) return;

    StaticJsonDocument<200> doc;
    doc["data"] = id;
    doc["timestamp"] = getCurrentTimestamp();
    char jsonBuffer[256];
    serializeJson(doc, jsonBuffer);

    mqttClient.publish(topic.c_str(), jsonBuffer);
}

time_t getCurrentTimestamp() {
    time_t now;
    time(&now);
    return now;
}

void displayLCD(const std::vector<String>& dataList, int duration) {
    lcd.clear();
    for (size_t i = 0; i < dataList.size() && i < 2; i++) {
        lcd.setCursor(0, i);
        lcd.print(dataList[i]);
    }
    delay(duration);
    lcd.clear();
}
