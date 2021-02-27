#pragma once

#include <SPI.h>
#include <WiFi101.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <Wire.h>

#define PRINT(v) ( Serial && Serial.print(v) )
#define PRINTLN(v) ( Serial && Serial.println(v) )

class Timer_t {
    WiFiUDP _ntpUDP;
    NTPClient _client;   // NTP time server
public:
    Timer_t(): _ntpUDP(), _client(_ntpUDP) {}
    bool begin(char time_zone_offset) {
        _client.setUpdateInterval(24 * 3600 * 1000);  // update once a day
        _client.setTimeOffset(time_zone_offset * 3600);             // Time zone = EST
        _client.begin();
        return (_client.update());
    }
    bool update() { return _client.update(); }
    unsigned long time() { return _client.getEpochTime(); }
    int getHours() { return _client.getHours(); }
    int getMinutes() { return _client.getMinutes(); }

};

// Commands to control relay bank
#define RELAY_ADDR 0x08

bool writeRelay(uint8_t cmd) {
    static bool initialized = false;
    if (!initialized) {
        PRINTLN("Init start");
        Wire.begin();
        Wire.beginTransmission(RELAY_ADDR); 
        Wire.endTransmission(RELAY_ADDR); 
        PRINTLN("Init finished");
        initialized = true;
    }
    Wire.beginTransmission(RELAY_ADDR); 
    Wire.write(cmd);
    return(Wire.endTransmission());
}
uint8_t readRelay(uint8_t cmd) {
    writeRelay(cmd);
    Wire.requestFrom(RELAY_ADDR, 1);  
    return(Wire.read());
}

// Input: zone_idx = 0 - 3, which is index into zone array
//        tgt = tgt state of on or off
bool setRelayState(uint8_t zone_idx, bool tgt) {
    if (zone_idx > 3)
        return false;
    PRINT("reading relay: ");
    PRINTLN(zone_idx);
    bool state = readRelay(zone_idx + 5);  // get state cmd
    if (state == tgt)
        return true;
    PRINT("toggle relay\n");
    return(writeRelay(zone_idx + 1));     // toggle cmd
}
