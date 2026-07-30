#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Update.h>

extern void sendLogToDashboard(String message);

// NOTE: setupOTA() and handleOTA() were REMOVED from this file.
// OTA_Handler.h already defines the real setupOTA()/handleOTA() (ElegantOTA
// web-based local OTA). Having them defined here too caused a
// "redefinition" compile error, since both headers get included in main.cpp.
// This file now only handles the S3/AWS firmware download + flash logic.

// S3 ලින්ක් එකෙන් ඩවුන්ලෝඩ් කරමින්, Rollback පහසුකම සහ Firebase ලොග් සහිත ප්‍රධාන ෆන්ක්ෂන් එක
void performOTAUpdate(String url) {

    Serial.println("======================================");
    Serial.println("🚀 OTA Update Started");
    Serial.println("======================================");

    sendLogToDashboard("Connecting to S3 to download firmware...");

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;

    if (!http.begin(client, url)) {
        Serial.println("❌ HTTP begin failed");
        return;
    }

    int httpCode = http.GET();

    if (httpCode != HTTP_CODE_OK) {
        Serial.printf("❌ HTTP Error : %d\n", httpCode);
        Serial.println(http.errorToString(httpCode));
        http.end();
        return;
    }

    int contentLength = http.getSize();

    if (contentLength <= 0) {
        Serial.println("⚠️ Unknown content length");
        contentLength = UPDATE_SIZE_UNKNOWN;
    }

    Serial.printf("Firmware Size      : %d\n", contentLength);
    Serial.printf("Sketch Size        : %u\n", ESP.getSketchSize());
    Serial.printf("Free Sketch Space  : %u\n", ESP.getFreeSketchSpace());

    sendLogToDashboard("Firmware size: " + String(contentLength));

    if (!Update.begin(contentLength)) {

        Serial.println("======================================");
        Serial.println("❌ Update.begin FAILED");
        Serial.printf("Error Code : %d\n", Update.getError());
        Serial.printf("Error      : %s\n", Update.errorString());
        Serial.println("======================================");

        http.end();
        return;
    }

    Serial.println("✅ Update.begin OK");

    WiFiClient *stream = http.getStreamPtr();

    size_t written = Update.writeStream(*stream);

    Serial.println("--------------------------------------");
    Serial.printf("Bytes Written : %u\n", written);
    Serial.printf("Update Error  : %d\n", Update.getError());
    Serial.printf("Error String  : %s\n", Update.errorString());
    Serial.println("--------------------------------------");

    if (written != (size_t)contentLength &&
        contentLength != UPDATE_SIZE_UNKNOWN) {

        Serial.println("⚠️ Firmware not fully written");
    }

    if (!Update.end()) {

        Serial.println("======================================");
        Serial.println("❌ Update.end FAILED");
        Serial.printf("Error Code : %d\n", Update.getError());
        Serial.printf("Error      : %s\n", Update.errorString());
        Serial.println("🛡️ Rolling Back...");
        Serial.println("======================================");

        sendLogToDashboard("OTA Failed");

        http.end();
        return;
    }

    if (!Update.isFinished()) {

        Serial.println("❌ OTA Not Finished");
        http.end();
        return;
    }

    Serial.println("======================================");
    Serial.println("🎉 OTA SUCCESS");
    Serial.println("Restarting...");
    Serial.println("======================================");

    sendLogToDashboard("OTA Success");

    http.end();

    delay(3000);

    ESP.restart();
}

#endif