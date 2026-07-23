#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Update.h>

// main.cpp එකේ තියෙන Firebase ලොග් යවන ෆන්ක්ෂන් එක මෙතැනට අරගන්නවා
extern void sendLogToDashboard(String message);

void setupOTA() {
    // Local OTA අවශ්‍ය නම් මෙහි තැබිය හැක
}

// S3 ලින්ක් එකෙන් ඩවුන්ලෝඩ් කරමින්, සියලුම Logs Firebase එකට යවන ප්‍රධාන ෆන්ක්ෂන් එක
void performOTAUpdate(String url) {
    Serial.println("🚀 Connecting to S3 to download firmware...");
    sendLogToDashboard("Connecting to S3 to download firmware...");
    
    WiFiClientSecure client;
    client.setInsecure(); // SSL Certificate Error මඟහරවා ගැනීමට

    HTTPClient http;
    http.begin(client, url);

    int httpCode = http.GET();
    if (httpCode > 0) {
        if (httpCode == HTTP_CODE_OK) {
            int contentLength = http.getSize();
            Serial.printf("📦 Firmware size: %d bytes\n", contentLength);
            sendLogToDashboard("Firmware size: " + String(contentLength) + " bytes");

            // OTA ප්‍රමාණය පරීක්ෂා කර Update එක ආරම්භ කිරීම
            bool canBegin = Update.begin(contentLength);
            if (canBegin) {
                Serial.println("🔄 Beginning OTA update. Please wait...");
                sendLogToDashboard("Beginning OTA update. Please wait...");
                
                // ඩවුන්ලෝඩ් වෙමින් පවතියි
                size_t written = Update.writeStream(http.getStream());

                if (written == contentLength) {
                    Serial.printf("Written : %d successfully\n", written);
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
                    // **✨ Rollback Mechanism:** අවුලක් වුණොත් පරණ කේතයම තබා ගනියි.
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

void handleOTA() {
    // අවශ්‍ය නම් මෙහි වෙනත් Background tasks දැමිය හැක
}

#endif