#ifndef AWS_IOT_MANAGER_H
#define AWS_IOT_MANAGER_H

#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <HTTPClient.h>
#include <Update.h>
#include "AWS_Secrets.h"

WiFiClientSecure net;
PubSubClient mqttClient(net);

// main.cpp එකේ තියෙන Firebase ලොග් යවන ෆන්ක්ෂන් එක මෙතැනට අරගන්නවා
extern void sendLogToDashboard(String message);

// ✨ S3 ලින්ක් එකෙන් ඩවුන්ලෝඩ් කරමින්, Rollback පහසුකම සහ Firebase ලොග් සහිත OTA ෆන්ක්ෂන් එක
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

            bool canBegin = Update.begin(contentLength);
            if (canBegin) {
                Serial.println("🔄 Beginning OTA update. Please wait...");
                sendLogToDashboard("Beginning OTA update. Please wait...");
                
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

// AWS IoT Core එකෙන් මැසේජ් එකක් ආවම ක්‍රියාත්මක වන Callback එක
void mqttCallback(char* topic, byte* payload, unsigned int length) {
    Serial.print("🔔 AWS IoT Alert! Message arrived on topic: ");
    Serial.println(topic);
    
    sendLogToDashboard("AWS IoT Alert! Message arrived"); 

    String message = "";
    for (int i = 0; i < length; i++) {
        message += (char)payload[i];
    }
    Serial.println("Message Content: " + message);

    // Topic එක අපේ OTA Topic එක නම් S3 ලින්ක් එක අරන් Update එක පටන් ගන්නවා
    if(String(topic) == AWS_IOT_TOPIC) {
        Serial.println("🚀 Starting OTA Update from AWS S3...");
        sendLogToDashboard("Starting OTA Update from AWS S3..."); 
        
        String s3_url = message; 
        performOTAUpdate(s3_url); 
    }
}

void connectAWS() {
    Serial.println("Connecting to AWS IoT Core...");
    sendLogToDashboard("Connecting to AWS IoT Core..."); 

    net.setCACert(AWS_CERT_CA);
    net.setCertificate(AWS_CERT_CRT);
    net.setPrivateKey(AWS_CERT_PRIVATE);

    mqttClient.setServer(AWS_IOT_ENDPOINT, 8883);
    mqttClient.setCallback(mqttCallback);

    while (!mqttClient.connect("AetherFlash_ESP32")) {
        Serial.print(".");
        delay(1000);
    }

    if (mqttClient.connected()) {
        Serial.println("\n✅ AWS IoT Connected Successfully!");
        sendLogToDashboard("AWS IoT Connected Successfully!"); 
        
        mqttClient.subscribe(AWS_IOT_TOPIC);
        Serial.println("📡 Subscribed to OTA Topic: " AWS_IOT_TOPIC);
        sendLogToDashboard("Subscribed to OTA Topic"); 
    } else {
        Serial.println("\n❌ AWS IoT Connection Failed!");
        sendLogToDashboard("AWS IoT Connection Failed!"); 
    }
}

void handleAWS() {
    mqttClient.loop();
}

#endif