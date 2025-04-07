#ifndef WIFI_MODULE_H
#define WIFI_MODULE_H

#include <WiFi.h>
#include <LiquidCrystal_I2C.h>

extern const char* ssid;
extern const char* password;

void setupWifi(LiquidCrystal_I2C& lcd) {
    Serial.println();
    Serial.print("Connecting to ");
    lcd.clear();
    lcd.print("Connecting WiFi");

    WiFi.begin(ssid);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("\nWiFi connected");
    lcd.clear();
    lcd.print("WiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
}
#endif
