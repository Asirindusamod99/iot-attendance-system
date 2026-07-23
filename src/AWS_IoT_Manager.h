#ifndef AWS_IOT_MANAGER_H
#define AWS_IOT_MANAGER_H

#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <HTTPClient.h>
#include <Update.h>
#include "AWS_Secrets.h"

WiFiClientSecure net;
PubSubClient mqttClient(net);

extern void sendLogToDashboard(String message);
extern void performOTAUpdate(String url); // OTA_Manager එකෙන් එන ෆන්ක්ෂන් එක සඳහා external reference

// AWS IoT Core එකෙන් මැසේජ් එකක් ආවම ක්‍රියාත්මක වන Callback එක
void mqttCallback(char* topic, byte* payload, unsigned int length) {
    Serial.print("🔔 AWS IoT Alert! Message arrived on topic: ");
    Serial.println(topic);

    sendLogToDashboard("AWS IoT Alert! Message arrived");

    String message = "";
    for (unsigned int i = 0; i < length; i++) {
        message += (char)payload[i];
    }

    // ✨ URL එකේ අගින් මුලින් තියෙන අනවශ්‍ය spaces සහ newline characters ඉවත් කිරීම
    message.trim();
    Serial.println("Cleaned S3 URL Content: [" + message + "]");
    Serial.printf("URL length: %d\n", message.length());

    if(String(topic) == AWS_IOT_TOPIC) {
        Serial.println("🚀 Starting OTA Update from AWS S3...");
        sendLogToDashboard("Starting OTA Update from AWS S3...");

        performOTAUpdate(message);
    }
}

void connectAWS() {
    Serial.println("Connecting to AWS IoT Core...");
    sendLogToDashboard("Connecting to AWS IoT Core...");

    net.setCACert(AWS_CERT_CA);
    net.setCertificate(AWS_CERT_CRT);
    net.setPrivateKey(AWS_CERT_PRIVATE);

    // Default PubSubClient MQTT buffer is 256 bytes. Presigned S3 URLs are
    // usually 300-500+ characters, so they get silently truncated without
    // this. Must be set BEFORE connect().
    mqttClient.setBufferSize(1024);

    mqttClient.setServer(AWS_IOT_ENDPOINT, 8883);
    mqttClient.setCallback(mqttCallback);

    // Bounded retry loop instead of an infinite while(). If AWS IoT is
    // unreachable, setup() will still finish and the rest of the device
    // (relays, RFID, local OTA server) comes online; handleAWS()/loop()
    // retries in the background instead of blocking forever.
    const int MAX_RETRIES = 10;
    int attempts = 0;
    while (!mqttClient.connect("AetherFlash_ESP32") && attempts < MAX_RETRIES) {
        Serial.print(".");
        attempts++;
        delay(1000);
    }

    if (mqttClient.connected()) {
        Serial.println("\n✅ AWS IoT Connected Successfully!");
        sendLogToDashboard("AWS IoT Connected Successfully!");

        mqttClient.subscribe(AWS_IOT_TOPIC);
        Serial.println("📡 Subscribed to OTA Topic: " AWS_IOT_TOPIC);
        sendLogToDashboard("Subscribed to OTA Topic");
    } else {
        Serial.println("\n❌ AWS IoT Connection Failed! Will keep retrying in background.");
        sendLogToDashboard("AWS IoT Connection Failed! Will retry in background.");
    }
}

void handleAWS() {
    // If we're not connected (e.g. initial connect() above timed out, or the
    // link dropped later), keep retrying here instead of leaving AWS dead
    // for the rest of the device's life.
    if (!mqttClient.connected()) {
        static unsigned long lastRetry = 0;
        if (millis() - lastRetry > 5000) {
            lastRetry = millis();
            Serial.println("🔄 Reconnecting to AWS IoT...");
            if (mqttClient.connect("AetherFlash_ESP32")) {
                mqttClient.subscribe(AWS_IOT_TOPIC);
                Serial.println("✅ AWS IoT Reconnected!");
                sendLogToDashboard("AWS IoT Reconnected");
            }
        }
        return;
    }
    mqttClient.loop();
}

#endif