#pragma once

#include <Arduino.h>
#include <TinyGPSPlus.h>
#include "config_common.h"

class GPSTracker {
public:
    GPSTracker() : gpsSerial(1) {}

    void begin() {
        gpsSerial.begin(GPS_BAUDRATE, SERIAL_8N1, PIN_GPS_RX, PIN_GPS_TX);
    }

    void update() {
        while (gpsSerial.available() > 0) {
            char c = gpsSerial.read();
            gps.encode(c);
        }
    }

    bool hasFix() {
        return gps.location.isValid() && gps.location.age() < 2000;
    }

    double getLatitude() {
        return gps.location.isValid() ? gps.location.lat() : 0.0;
    }

    double getLongitude() {
        return gps.location.isValid() ? gps.location.lng() : 0.0;
    }

    int32_t getLatDeg7() {
        return static_cast<int32_t>(getLatitude() * 10000000.0);
    }

    int32_t getLngDeg7() {
        return static_cast<int32_t>(getLongitude() * 10000000.0);
    }

    float getSpeedKnots() {
        return gps.speed.isValid() ? gps.speed.knots() : 0.0f;
    }

    float getHeading() {
        return gps.course.isValid() ? gps.course.deg() : 0.0f;
    }

    uint8_t getSatellites() {
        return gps.satellites.isValid() ? gps.satellites.value() : 0;
    }

    uint8_t getFixQuality() {
        return hasFix() ? 1 : 0;
    }

private:
    HardwareSerial gpsSerial;
    TinyGPSPlus gps;
};
