#ifndef SENSORS_H
#define SENSORS_H

#include <DHT.h>
#include "Config.h"

#define DHTTYPE DHT11
DHT dht(DHT_PIN, DHTTYPE);

void setupHardware() {
    // Configure all relays as outputs and start them in the OFF state.
    pinMode(RELAY_1, OUTPUT); digitalWrite(RELAY_1, LOW);
    pinMode(RELAY_2, OUTPUT); digitalWrite(RELAY_2, LOW);
    pinMode(RELAY_3, OUTPUT); digitalWrite(RELAY_3, LOW);
    pinMode(RELAY_4, OUTPUT); digitalWrite(RELAY_4, LOW);

    pinMode(PIR_PIN, INPUT);
    dht.begin();
}

float getMachineTemp() {
    float temp = dht.readTemperature();
    if (isnan(temp)) return 0.0;
    return temp;
}

bool isWorkerPresent() {
    return digitalRead(PIR_PIN) == HIGH;
}

// Turn the selected relay on or off based on its number.
void controlRelay(int relayNum, bool state) {
    int pin = RELAY_1;
    if(relayNum == 2) pin = RELAY_2;
    if(relayNum == 3) pin = RELAY_3;
    if(relayNum == 4) pin = RELAY_4;

    digitalWrite(pin, state ? HIGH : LOW);
}

#endif