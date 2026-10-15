#pragma once

#include <Arduino.h>
#include <TinyGPSPlus.h>
#include "config_common.h"

class GPSTracker {
public:
    GPSTracker(uint8_t uartNum = 0) : gpsSerial(uartNum), isHwEnabled(false) {}

    void begin(bool enableHw = false) {
        isHwEnabled = enableHw;
        if (isHwEnabled) {
            gpsSerial.begin(GPS_BAUDRATE, SERIAL_8N1, PIN_GPS_RX, PIN_GPS_TX);
        }
    }

    void update() {
        if (!isHwEnabled) return;
        while (gpsSerial.available() > 0) {
            char c = gpsSerial.read();
            gps.encode(c);
        }
    }

    void setValues(double lat, double lng, float spd, float head, uint8_t sats, bool fix) {
        manualLat = lat;
        manualLng = lng;
        manualSpeed = spd;
        manualHeading = head;
        manualSats = sats;
        manualFix = fix;
        hasManualData = true;
    }

    bool hasFix() {
        if (hasManualData) return manualFix;
        return gps.location.isValid() && gps.location.age() < 2000;
    }

    double getLatitude() {
        if (hasManualData) return manualLat;
        return gps.location.isValid() ? gps.location.lat() : 0.0;
    }

    double getLongitude() {
        if (hasManualData) return manualLng;
        return gps.location.isValid() ? gps.location.lng() : 0.0;
    }

    int32_t getLatDeg7() {
        return static_cast<int32_t>(getLatitude() * 10000000.0);
    }

    int32_t getLngDeg7() {
        return static_cast<int32_t>(getLongitude() * 10000000.0);
    }

    float getSpeedKnots() {
        if (hasManualData) return manualSpeed;
        return gps.speed.isValid() ? gps.speed.knots() : 0.0f;
    }

    float getHeading() {
        if (hasManualData) return manualHeading;
        return gps.course.isValid() ? gps.course.deg() : 0.0f;
    }

    uint8_t getSatellites() {
        if (hasManualData) return manualSats;
        return gps.satellites.isValid() ? gps.satellites.value() : 0;
    }

    uint8_t getFixQuality() {
        return hasFix() ? 1 : 0;
    }

    // Distance in meters between current GPS location and target coordinates
    double distanceTo(double targetLat, double targetLng) {
        if (!hasFix()) return 999999.0;
        return TinyGPSPlus::distanceBetween(getLatitude(), getLongitude(), targetLat, targetLng);
    }

    // Bearing/Course in degrees (0..359.9) from current GPS location to target coordinates
    double courseTo(double targetLat, double targetLng) {
        if (!hasFix()) return 0.0;
        return TinyGPSPlus::courseTo(getLatitude(), getLongitude(), targetLat, targetLng);
    }

private:
    HardwareSerial gpsSerial;
    TinyGPSPlus gps;
    bool isHwEnabled = false;
    bool hasManualData = false;
    double manualLat = 0.0;
    double manualLng = 0.0;
    float manualSpeed = 0.0f;
    float manualHeading = 0.0f;
    uint8_t manualSats = 0;
    bool manualFix = false;
};
