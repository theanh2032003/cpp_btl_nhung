#ifndef RFID_H
#define RFID_H

#include <MFRC522.h>

#define RST_PIN 5
#define SS_PIN  4

MFRC522 mfrc522(SS_PIN, RST_PIN);
MFRC522::MIFARE_Key key;

void initRFID() {
    SPI.begin();
    mfrc522.PCD_Init();
    memset(key.keyByte, 0xFF, sizeof(key.keyByte));
}

bool isCardPresent() {
    return mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial();
}

bool writeID(const char* input) {
    byte block = 4;
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
    byte block = 4;
    byte buffer[18];
    byte size = sizeof(buffer);
    // mfrc522.PICC_DumpToSerial( &(mfrc522.uid));
    if (mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, block, &key, &(mfrc522.uid)) != MFRC522::STATUS_OK) {
        Serial.println("Auth failed!");
        return "";
    }

    if (mfrc522.MIFARE_Read(block, buffer, &size) != MFRC522::STATUS_OK) {
        Serial.println("Read failed!");
        return "";
    }

    String cardID;
    for (int i = 0; i < 16; i++) {
        if (buffer[i] == 0) break; // kết thúc chuỗi nếu gặp ký tự null
        cardID += (char)buffer[i];
    }
    cardID.toUpperCase();

    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();
    return cardID;
}

String readUID() {
    String uid = "";
    for (byte i = 0; i < mfrc522.uid.size; i++) {
        if (mfrc522.uid.uidByte[i] < 0x10) {
            uid += "0";
        }
        uid += String(mfrc522.uid.uidByte[i], HEX);
    }
    uid.toUpperCase();
    return uid;
}

#endif
