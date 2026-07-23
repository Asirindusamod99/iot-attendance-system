#ifndef AWS_IOT_MANAGER_H
#define AWS_IOT_MANAGER_H

#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include "AWS_Secrets.h"
#include "OTA_Manager.h" 

WiFiClientSecure net;
PubSubClient mqttClient(net);

extern void sendLogToDashboard(String message); 

void mqttCallback(char* topic, byte* payload, unsigned int length) {
    // Handle incoming MQTT messages from AWS IoT Core.
    Serial.print("🔔 AWS IoT Alert! Message arrived on topic: ");
    Serial.println(topic);
    
    sendLogToDashboard("AWS IoT Alert! Message arrived"); 

    String message = "";
    for (int i = 0; i < length; i++) {
        message += (char)payload[i];
    }
    Serial.println("Message Content: " + message);

    if(String(topic) == AWS_IOT_TOPIC) {
        // The OTA topic payload contains the firmware URL.
        Serial.println("🚀 Starting OTA Update from AWS S3...");
        sendLogToDashboard("Starting OTA Update from AWS S3..."); 
        
        String s3_url = message; 
        performOTAUpdate(s3_url); 
    }
}

void connectAWS() {
    // Configure the TLS client and MQTT connection.
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
        // Subscribe to the OTA command topic after a successful connection.
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
    // Keep the MQTT client processing incoming messages.
    mqttClient.loop();
}

#endif