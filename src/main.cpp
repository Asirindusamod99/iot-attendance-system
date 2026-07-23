#include <Arduino.h>
#include <WiFi.h>
#include <FirebaseESP32.h>
#include <time.h>

// 🟢 ඔයාගේ අනිත් Header ෆයිල් ටික
#include "Config.h"
#include "Sensors.h"
#include "RFID_Logic.h"
#include "OTA_Handler.h"

FirebaseData fbData;
FirebaseAuth auth;
FirebaseConfig config;

unsigned long lastSensorUpdate = 0;
bool isMaintenanceMode = false; 

// 🔴 අර Missing වෙලා තිබ්බ Function එක! (මේක අනිවාර්යයෙන්ම මෙතන තියෙන්න ඕනේ)
void sendLogToDashboard(String message) {
    FirebaseJson json;
    json.set("action", "SYSTEM");
    json.set("name", message);
    json.set("role", "AetherFlash OTA");
    
    // ඔයා පාවිච්චි කරන FirebaseESP32 ලයිබ්‍රරි එකට අනුව pushJSON කරන විදිහ
    if (Firebase.pushJSON(fbData, "/MachineLogs", json)) {
        Serial.println("✅ Log sent to Dashboard");
    } else {
        Serial.println("❌ Failed to send log: " + fbData.errorReason());
    }
}

// 🟢 Function එක ලිව්වට පස්සේ තමයි AWS IoT Manager එක Include කරන්නේ
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
    
    // AWS වලට Connect වීම
    connectAWS();
    Serial.println("System Ready!");
}

void loop() {
    handleOTA(); 
    handleAWS();

    // Relays Control
    if (Firebase.getString(fbData, "/MachineControl/" MACHINE_ID "/Relay1")) controlRelay(1, fbData.stringData() == "ON");
    if (Firebase.getString(fbData, "/MachineControl/" MACHINE_ID "/Relay2")) controlRelay(2, fbData.stringData() == "ON");
    if (Firebase.getString(fbData, "/MachineControl/" MACHINE_ID "/Relay3")) controlRelay(3, fbData.stringData() == "ON");
    if (Firebase.getString(fbData, "/MachineControl/" MACHINE_ID "/Relay4")) controlRelay(4, fbData.stringData() == "ON");

    // Sensors
    if (millis() - lastSensorUpdate > 10000) {
        float temp = getMachineTemp();
        bool workerHere = isWorkerPresent();
        Firebase.setFloat(fbData, "/MachineStatus/" MACHINE_ID "/Temperature", temp);
        Firebase.setBool(fbData, "/MachineStatus/" MACHINE_ID "/WorkerPresent", workerHere);
        lastSensorUpdate = millis();
    }

    // 🔴 RFID Attendance (IN / OUT Logic)
    String scannedID = getCardID();
    if (scannedID != "") {
        Serial.println("\nCard Scanned: " + scannedID);

        // 1. කාඩ් එක Database එකේ (Root එකේ) තියෙනවද බලනවා
        if (Firebase.getString(fbData, "/" + scannedID + "/name")) {
            String name = fbData.stringData(); 
            Firebase.getString(fbData, "/" + scannedID + "/role");
            String role = fbData.stringData(); 
            
            String scanTime = getCurrentTime();

            // 🔴 සේවකයා දැනට ඇතුලෙද (IN) ඉන්නේ කියලා Firebase එකෙන් පරීක්ෂා කිරීම
            String currentStatus = "OUT"; 
            if (Firebase.getString(fbData, "/ActiveSessions/" + scannedID + "/status")) {
                currentStatus = fbData.stringData();
            }

            FirebaseJson log;
            log.set("uid", scannedID);
            log.set("name", name);
            log.set("role", role);
            log.set("timestamp", scanTime);

            // 🔴 Technician කෙනෙක් නම්
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
            // 🔴 Worker කෙනෙක් නම් (IN / OUT මාරු කිරීම)
            else {
                if (currentStatus == "OUT" || currentStatus == "") {
                    // පළවෙනි පාර - පැමිණීම (IN)
                    log.set("action", "IN");
                    Firebase.setString(fbData, "/ActiveSessions/" + scannedID + "/status", "IN");
                    Firebase.setString(fbData, "/ActiveSessions/" + scannedID + "/inTime", scanTime);
                    Serial.println("Worker " + name + " Marked: IN at " + scanTime);
                } else {
                    // දෙවෙනි පාර - පිටවීම (OUT)
                    log.set("action", "OUT");
                    Firebase.setString(fbData, "/ActiveSessions/" + scannedID + "/status", "OUT");
                    Firebase.setString(fbData, "/ActiveSessions/" + scannedID + "/outTime", scanTime);
                    Serial.println("Worker " + name + " Marked: OUT at " + scanTime);
                }
            }
            
            // Web UI Logs වලට දත්ත යැවීම
            Firebase.pushJSON(fbData, "/MachineLogs", log);
            
        } 
        // 2. Register කරපු නැති අලුත් කාඩ් එකක් නම්
        else {
            Serial.println("New Card Detected! Sending to Web UI...");
            Firebase.setString(fbData, "/MachineStatus/" MACHINE_ID "/LastUnknownCard", scannedID);
        }
        
        delay(2000); // දෙපාරක් එක දිගට කියවීම වැළැක්වීමට
    }
}