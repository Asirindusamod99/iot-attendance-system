#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include <HTTPClient.h>
#include <Update.h>
#include <WiFiClientSecure.h>

extern void sendLogToDashboard(String message);

void performOTAUpdate(String firmwareUrl) {
    Serial.println("🚀 Starting OTA Download from: " + firmwareUrl);
    sendLogToDashboard("Downloading Firmware from S3...");

    WiFiClientSecure secureClient;
    secureClient.setInsecure(); // S3 HTTPS bypass

    HTTPClient http;
    http.begin(secureClient, firmwareUrl);

    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
        int contentLength = http.getSize();
        bool canBegin = Update.begin(contentLength);

        if (canBegin) {
            Serial.println("Begin Flash... Please wait...");
            WiFiClient *client = http.getStreamPtr();
            size_t written = Update.writeStream(*client);

            if (written == contentLength) {
                Serial.println("✅ Download & Flash Complete!");
            } else {
                Serial.println("❌ Flash Failed!");
            }

            if (Update.end()) {
                if (Update.isFinished()) {
                    Serial.println("✅ Update successfully completed! Rebooting...");
                    sendLogToDashboard("✅ OTA Update Successful! Rebooting Now...");
                    delay(2000);
                    ESP.restart(); 
                } else {
                    Serial.println("❌ Update not finished!");
                    sendLogToDashboard("❌ OTA Update Failed (Not Finished)!");
                }
            } else {
                Serial.println("❌ Error Occurred");
                sendLogToDashboard("❌ OTA Update Error!");
            }
        } else {
            Serial.println("❌ Not enough space to begin OTA");
            sendLogToDashboard("❌ OTA Failed: Not enough storage!");
        }
    } else {
        Serial.println("❌ Cannot download firmware.");
        sendLogToDashboard("❌ S3 Download Failed.");
    }
    http.end();
}

#endif