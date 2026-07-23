#include <Arduino.h>
#include <WiFi.h>
#include <FirebaseESP32.h>
#include <time.h>

// Core project headers
#include "Config.h"
#include "Sensors.h"
#include "RFID_Logic.h"
#include "OTA_Handler.h"

FirebaseData fbData;
FirebaseAuth auth;
FirebaseConfig config;

unsigned long lastSensorUpdate = 0;
bool isMaintenanceMode = false; 

// Sends system logs to the Firebase dashboard.
void sendLogToDashboard(String message) {
    FirebaseJson json;
    json.set("action", "SYSTEM");
    json.set("name", message);
    json.set("role", "AetherFlash OTA");
    
    // Push the log payload using FirebaseESP32.
    if (Firebase.pushJSON(fbData, "/MachineLogs", json)) {
        Serial.println("✅ Log sent to Dashboard");
    } else {
        Serial.println("❌ Failed to send log: " + fbData.errorReason());
    }
}

// Include AWS IoT support after the dashboard logger is defined.
#include "AWS_IoT_Manager.h"

String getCurrentTime() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) return "Time Error";
    char timeStringBuff[50];
    strftime(timeStringBuff, sizeof(timeStringBuff), "%Y-%m-%d %H:%M:%S", &timeinfo);
    return String(timeStringBuff);
}

void setup() {
    Serial.begin(115200);

    setupHardware();
    setupRFID();
    pinMode(2, OUTPUT);
    Serial.print("Connecting to WiFi");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(300);
        Serial.print(".");
    }
    Serial.println("\nWiFi Connected! IP: " + WiFi.localIP().toString());

    setupOTA();
    configTime(19800, 0, "pool.ntp.org");

    config.database_url = FIREBASE_HOST;
    config.signer.tokens.legacy_token = FIREBASE_AUTH;
    Firebase.begin(&config, &auth);
    Firebase.reconnectWiFi(true);

    Firebase.setString(fbData, "/MachineStatus/" MACHINE_ID "/State", "IDLE");
    
    // Connect to AWS IoT Core.
    connectAWS();
    Serial.println("System Ready!");
}

void loop() {
    handleOTA(); 
    handleAWS();
    digitalWrite(2, HIGH);  

    // Relay control
    if (Firebase.getString(fbData, "/MachineControl/" MACHINE_ID "/Relay1")) controlRelay(1, fbData.stringData() == "ON");
    if (Firebase.getString(fbData, "/MachineControl/" MACHINE_ID "/Relay2")) controlRelay(2, fbData.stringData() == "ON");
    if (Firebase.getString(fbData, "/MachineControl/" MACHINE_ID "/Relay3")) controlRelay(3, fbData.stringData() == "ON");
    if (Firebase.getString(fbData, "/MachineControl/" MACHINE_ID "/Relay4")) controlRelay(4, fbData.stringData() == "ON");

    // Periodic sensor updates
    if (millis() - lastSensorUpdate > 10000) {
        float temp = getMachineTemp();
        bool workerHere = isWorkerPresent();
        Firebase.setFloat(fbData, "/MachineStatus/" MACHINE_ID "/Temperature", temp);
        Firebase.setBool(fbData, "/MachineStatus/" MACHINE_ID "/WorkerPresent", workerHere);
        lastSensorUpdate = millis();
    }

    // RFID attendance handling (IN / OUT logic)
    String scannedID = getCardID();
    if (scannedID != "") {
        Serial.println("\nCard Scanned: " + scannedID);

        // Check whether the card exists in the root of the database.
        if (Firebase.getString(fbData, "/" + scannedID + "/name")) {
            String name = fbData.stringData(); 
            Firebase.getString(fbData, "/" + scannedID + "/role");
            String role = fbData.stringData(); 
            
            String scanTime = getCurrentTime();

            // Check whether the worker is currently marked as IN.
            String currentStatus = "OUT"; 
            if (Firebase.getString(fbData, "/ActiveSessions/" + scannedID + "/status")) {
                currentStatus = fbData.stringData();
            }

            FirebaseJson log;
            log.set("uid", scannedID);
            log.set("name", name);
            log.set("role", role);
            log.set("timestamp", scanTime);

            // Technician flow
            if (role == "Technician") {
                if (!isMaintenanceMode) {
                    isMaintenanceMode = true;
                    log.set("action", "Maintenance Started");
                    Firebase.setString(fbData, "/MachineStatus/" MACHINE_ID "/State", "MAINTENANCE");
                    Serial.println("Technician " + name + " STARTED Maintenance.");
                } else {
                    isMaintenanceMode = false;
                    log.set("action", "Maintenance Ended");
                    Firebase.setString(fbData, "/MachineStatus/" MACHINE_ID "/State", "IDLE");
                    Serial.println("Technician " + name + " ENDED Maintenance.");
                }
            } 
            // Worker flow: toggle between IN and OUT.
            else {
                if (currentStatus == "OUT" || currentStatus == "") {
                    log.set("action", "IN");
                    Firebase.setString(fbData, "/ActiveSessions/" + scannedID + "/status", "IN");
                    Firebase.setString(fbData, "/ActiveSessions/" + scannedID + "/inTime", scanTime);
                    Serial.println("Worker " + name + " Marked: IN at " + scanTime);
                } else {
                    log.set("action", "OUT");
                    Firebase.setString(fbData, "/ActiveSessions/" + scannedID + "/status", "OUT");
                    Firebase.setString(fbData, "/ActiveSessions/" + scannedID + "/outTime", scanTime);
                    Serial.println("Worker " + name + " Marked: OUT at " + scanTime);
                }
            }
            
            Firebase.pushJSON(fbData, "/MachineLogs", log);
        } 
        else {
            Serial.println("New Card Detected! Sending to Web UI...");
            Firebase.setString(fbData, "/MachineStatus/" MACHINE_ID "/LastUnknownCard", scannedID);
        }
        
        delay(2000); 
    }
}