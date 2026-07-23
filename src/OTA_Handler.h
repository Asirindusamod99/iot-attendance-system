#ifndef OTA_HANDLER_H
#define OTA_HANDLER_H

#include <WiFi.h>
#include <WebServer.h>
#include <ElegantOTA.h>

WebServer server(80);

void setupOTA() {
    server.on("/", []() {
        // Root page shown before opening the OTA update route.
        server.send(200, "text/plain", "Smart Machine is Online! Go to /update to flash new firmware.");
    });
    
    // Start the ElegantOTA web update server.
    ElegantOTA.begin(&server);    
    server.begin();
    
    // Print the local OTA update URL to the serial monitor.
    Serial.print(">>> OTA Server Ready! Go to: http://");
    Serial.print(WiFi.localIP());
    Serial.println("/update <<<");
}

void handleOTA() {
    // Keep the web server and OTA handler responsive.
    server.handleClient();
    ElegantOTA.loop();
}

#endif