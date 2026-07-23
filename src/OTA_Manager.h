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
    Serial.println("🚀 Connecting to S3 to download firmware...");
    sendLogToDashboard("Connecting to S3 to download firmware...");

    WiFiClientSecure client;
    client.setInsecure(); // ⚠️ SSL Certificate Error මඟහරවා ගැනීමට — see note below

    HTTPClient http;
    http.begin(client, url);

    int httpCode = http.GET();
    if (httpCode > 0) {
        if (httpCode == HTTP_CODE_OK) {
            int contentLength = http.getSize();

            // Fallback: S3 may respond without Content-Length (e.g. chunked
            // encoding). Without this, Update.begin(-1) can fail silently.
            if (contentLength <= 0) {
                Serial.println("⚠️ Content-Length missing/invalid, using UPDATE_SIZE_UNKNOWN");
                contentLength = UPDATE_SIZE_UNKNOWN;
            }

            Serial.printf("📦 Firmware size: %d bytes\n", contentLength);
            sendLogToDashboard("Firmware size: " + String(contentLength) + " bytes");

            bool canBegin = Update.begin(contentLength);
            if (canBegin) {
                Serial.println("🔄 Beginning OTA update. Please wait...");
                sendLogToDashboard("Beginning OTA update. Please wait...");

                size_t written = Update.writeStream(http.getStream());

                if (contentLength != UPDATE_SIZE_UNKNOWN && written == (size_t)contentLength) {
                    Serial.printf("Written : %d successfully\n", written);
                } else if (contentLength == UPDATE_SIZE_UNKNOWN && written > 0) {
                    Serial.printf("Written : %d bytes (size was unknown)\n", written);
                } else {
                    Serial.printf("⚠️ Written only : %d/%d. Retrying...\n", written, contentLength);
                    sendLogToDashboard("Warning: Partial firmware write (" + String(written) + "/" + String(contentLength) + ")");
                }

                if (Update.end()) {
                    Serial.println("✅ OTA completed!");
                    if (Update.isFinished()) {
                        Serial.println("🎉 Update successfully completed. Rebooting in 3 seconds...");
                        sendLogToDashboard("OTA Update Success! Rebooting...");
                        delay(3000);
                        ESP.restart(); // සාර්ථක නම් පමණක් Reboot වේ
                    } else {
                        Serial.println("❌ Error: OTA not finished properly.");
                        sendLogToDashboard("Error: OTA not finished properly.");
                    }
                } else {
                    // ✨ Rollback Mechanism: අවුලක් වුණොත් පරණ කේතයම තබා ගනියි
                    String errNum = String(Update.getError());
                    Serial.printf("❌ Error Occurred. Error #: %s\n", errNum.c_str());
                    Serial.println("🛡️ Rolling back to previous stable firmware...");
                    sendLogToDashboard("OTA Failed (Err: " + errNum + "). Rolling back to stable firmware.");
                }
            } else {
                Serial.println("❌ Not enough space to begin OTA");
                sendLogToDashboard("OTA Error: Not enough space to begin update.");
            }
        } else {
            String errStr = http.errorToString(httpCode);
            Serial.printf("❌ HTTP GET failed, error: %s\n", errStr.c_str());
            sendLogToDashboard("OTA HTTP GET failed: " + errStr);
        }
    } else {
        String errStr = http.errorToString(httpCode);
        Serial.printf("❌ HTTP connection failed, error: %s\n", errStr.c_str());
        sendLogToDashboard("OTA HTTP connection failed: " + errStr);
    }

    http.end();
}

#endif