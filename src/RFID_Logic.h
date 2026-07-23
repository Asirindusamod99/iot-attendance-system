#ifndef RFID_LOGIC_H
#define RFID_LOGIC_H

#include <SPI.h>
#include <MFRC522.h>
#include "Config.h"

MFRC522 rfid(RFID_SS_PIN, RFID_RST_PIN);

void setupRFID() {
    SPI.begin();
    rfid.PCD_Init();
}

String getCardID() {
    if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
        return "";
    }
    String cardID = "";
    for (byte i = 0; i < rfid.uid.size; i++) {
        cardID += String(rfid.uid.uidByte[i] < 0x10 ? "0" : "");
        cardID += String(rfid.uid.uidByte[i], HEX);
    }
    cardID.toUpperCase();
    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
    return cardID;
}

#endif