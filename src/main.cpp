#include <Arduino.h>
#include <WiFi.h>
#include <FirebaseESP32.h>
#include <time.h>

// Core project headers
#include "Config.h"
#include "Sensors.h"
#include "RFID_Logic.h"
#include "OTA_Handler.h"
#include "OTA_Manager.h" // 👈 අනිවාර්යයෙන්ම AWS_IoT_Manager.h එකට උඩින් මෙය තියෙන්න ඕනේ!

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

    if (Firebase.pushJSON(fbData, "/MachineLogs", json)) {
        Serial.println("✅ Log sent to Dashboard");
    } else {
        Serial.println("❌ Failed to send log: " + fbData.errorReason());
    }
}

// Include AWS IoT support after OTA_Manager and logger are defined.
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

    // Sync local isMaintenanceMode with whatever Firebase last had, so a
    // reboot mid-maintenance (e.g. after an OTA update) doesn't desync the
    // device's local flag from the dashboard's reported state.
    if (Firebase.getString(fbData, "/MachineStatus/" MACHINE_ID "/State")) {
        String lastState = fbData.stringData();
        isMaintenanceMode = (lastState == "MAINTENANCE");
        Serial.println("🔄 Synced maintenance state from Firebase: " + lastState);
    } else {
        // No previous state on record - report IDLE as before.
        Firebase.setString(fbData, "/MachineStatus/" MACHINE_ID "/State", "IDLE");
    }

    Firebase.setString(fbData, "/MachineStatus/" MACHINE_ID "/FirmwareVersion", FIRMWARE_VERSION);
    Serial.println("📌 Current Firmware Version Reported to Firebase: " + String(FIRMWARE_VERSION));

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

        if (Firebase.getString(fbData, "/" + scannedID + "/name")) {
            String name = fbData.stringData();
            Firebase.getString(fbData, "/" + scannedID + "/role");
            String role = fbData.stringData();

            String scanTime = getCurrentTime();

            String currentStatus = "OUT";
            if (Firebase.getString(fbData, "/ActiveSessions/" + scannedID + "/status")) {
                currentStatus = fbData.stringData();
            }

            FirebaseJson log;
            log.set("uid", scannedID);
            log.set("name", name);
            log.set("role", role);
            log.set("timestamp", scanTime);

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