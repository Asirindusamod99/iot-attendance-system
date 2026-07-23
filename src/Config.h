#ifndef CONFIG_H
#define CONFIG_H

// Wi-Fi credentials.
#define WIFI_SSID "SLT-4G_164B2A"
#define WIFI_PASSWORD "B55F9EED"

#define FIREBASE_HOST "iot-attendance-system-d5008-default-rtdb.asia-southeast1.firebasedatabase.app"
#define FIREBASE_AUTH "w0VFGOYz7OqMIvEys63lrJj1fFLDIIBn2jQ3CnvF"

#define MACHINE_ID "Machine_01"

// RFID and sensor pins.
#define RFID_SS_PIN 5
#define RFID_RST_PIN 27
#define PIR_PIN 34        
#define DHT_PIN 15        

// Relay output pins.
#define RELAY_1 4
#define RELAY_2 16
#define RELAY_3 17
#define RELAY_4 13

#endif