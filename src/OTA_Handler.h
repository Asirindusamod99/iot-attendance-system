#ifndef OTA_HANDLER_H
#define OTA_HANDLER_H

#include <WiFi.h>
#include <WebServer.h>
#include <ElegantOTA.h>

WebServer server(80);

void setupOTA() {
    server.on("/", []() {
        server.send(200, "text/plain", "Smart Machine is Online! Go to /update to flash new firmware.");
    });
    
    ElegantOTA.begin(&server);    
    server.begin();
    
    Serial.print(">>> OTA Server Ready! Go to: http://");
    Serial.print(WiFi.localIP());
    Serial.println("/update <<<");
}

void handleOTA() {
    server.handleClient();
    ElegantOTA.loop();
}

#endif