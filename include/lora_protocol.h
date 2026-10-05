#pragma once

#include <stdint.h>

// ==========================================
// LORA PROTOCOL DEFINITIONS FOR MULTI-ROVER
// ==========================================

#define LORA_MAGIC_BYTE     0xD9
#define BROADCAST_ROVER_ID  0xFF

// Message Types
enum LoRaMsgType : uint8_t {
    MSG_TELEMETRY        = 0x10, // Rover -> Base
    MSG_MANUAL_CONTROL   = 0x20, // Base -> Rover (continuous joystick stream)
    MSG_ACTION_COMMAND   = 0x30, // Base -> Rover (from WindDragons cloud portal)
    MSG_POLL_REQUEST     = 0x40, // Base -> Rover (request telemetry)
    MSG_COMMAND_ACK      = 0x50  // Rover -> Base (command acknowledged)
};

// Motor status enum mapping with WindDragons Portal
// Motor status enum mapping with WindDragons Portal
enum MotorStatus : uint8_t {
    MOTOR_IDLE             = 0, // "idle" (disarmed / neutral)
    MOTOR_MANUAL           = 1, // "manual"
    MOTOR_HOLDING_STATION  = 2, // "holding_station"
    MOTOR_ANCHORED         = 3, // "anchored"
    MOTOR_MOVING_TO_TARGET = 4, // "moving_to_target" (navigate)
    MOTOR_RTL              = 5, // "rtl" (return to launch)
    MOTOR_FAILSAFE         = 6  // "failsafe"
};

// Anchor status enum mapping with WindDragons Portal
enum AnchorStatus : uint8_t {
    ANCHOR_RETRACTED = 0, // "retracted"
    ANCHOR_DEPLOYING = 1, // "deploying"
    ANCHOR_DEPLOYED  = 2, // "deployed"
    ANCHOR_RETRACTING= 3  // "retracting"
};

// Cloud action enum matching WindDragons Commands
enum CloudAction : uint8_t {
    ACTION_NONE            = 0,
    ACTION_HOLD_STATION    = 1, // Station keeping (target or current GPS position)
    ACTION_STOP            = 2, // Stop motors / disarm (STANDBY)
    ACTION_DROP_ANCHOR     = 3, // Lower physical anchor
    ACTION_RETRIEVE_ANCHOR = 4, // Retrieve physical anchor
    ACTION_STOP_ANCHOR     = 5, // Emergency stop anchor winch
    ACTION_NAVIGATE        = 6, // Navigate to target coordinates (GOTO)
    ACTION_RTL             = 7, // Return to Base / Launch
    ACTION_ALARM           = 8, // Strobe LED & acoustic alarm
    ACTION_PING            = 9, // Request immediate telemetry
    ACTION_MANUAL          = 10 // Manual joystick control
};

// ------------------------------------------
// 1. Telemetry Packet (Rover -> Base)
// ------------------------------------------
struct __attribute__((packed)) PacketTelemetry {
    uint8_t  magic;          // 0xD9
    uint8_t  msg_type;       // MSG_TELEMETRY
    uint8_t  rover_id;       // 1 = ROVER-01, 2 = ROVER-02, ...
    uint16_t seq_num;        // Sequence counter
    
    // GPS Data
    int32_t  lat_deg7;       // Latitude * 10,000,000 (e.g. 38.750508 -> 387505080)
    int32_t  lng_deg7;       // Longitude * 10,000,000 (e.g. -8.973995 -> -89739950)
    uint16_t speed_cm_s;     // Speed in cm/s (or knots * 100)
    uint16_t heading_deg10;  // Heading in degrees * 10 (0 to 3599)
    uint8_t  satellites;     // Satellites in view
    uint8_t  fix_quality;    // 0 = No Fix, 1 = GPS Fix, 2 = DGPS, 4 = RTK Fix

    // Power & Actuator States
    uint16_t battery_mv;     // Battery voltage in mV (e.g. 12500 for 12.5V)
    uint8_t  battery_pct;    // 0 - 100%
    uint8_t  motor_status;   // MotorStatus enum
    uint8_t  anchor_status;  // AnchorStatus enum
    uint16_t anchor_depth_cm;// Anchor depth in cm (e.g. 450 for 4.5m)
    
    // Command confirmation
    uint16_t last_cmd_id;    // Last executed cloud command ID
    uint16_t checksum;
};

// ------------------------------------------
// 2. Manual Control Packet (Base -> Rover)
// ------------------------------------------
struct __attribute__((packed)) PacketManualControl {
    uint8_t magic;           // 0xD9
    uint8_t msg_type;        // MSG_MANUAL_CONTROL
    uint8_t target_rover_id; // Target Rover (1, 2...) or 0xFF
    int8_t  throttle;        // -100 to +100 (%)
    int8_t  rudder;          // -100 (full left) to +100 (full right)
    int8_t  anchor_jog;      // -1 = lower winch, 0 = stop, 1 = raise winch
    uint8_t mode_request;    // 0 = Manual, 1 = Hold Station
    uint16_t checksum;
};

// ------------------------------------------
// 3. Action Command Packet (Base -> Rover from Portal)
// ------------------------------------------
struct __attribute__((packed)) PacketActionCommand {
    uint8_t  magic;          // 0xD9
    uint8_t  msg_type;       // MSG_ACTION_COMMAND
    uint8_t  target_rover_id;// Target Rover ID
    uint16_t command_id;     // Database ID (e.g. 14)
    uint8_t  action;         // CloudAction enum
    uint16_t target_depth_cm;// For DROP_ANCHOR (e.g. 450 cm)
    uint16_t duration_s;     // For ALARM (e.g. 10 s)
    int32_t  target_lat_deg7;// For NAVIGATE / HOLD_STATION
    int32_t  target_lng_deg7;// For NAVIGATE / HOLD_STATION
    uint16_t checksum;
};

// Checksum helper
inline uint16_t calculate_checksum(const uint8_t *data, size_t len) {
    uint16_t sum = 0;
    for (size_t i = 0; i < len; ++i) {
        sum = (sum << 1) ^ data[i];
    }
    return sum;
}

// Convert helper strings for Winddragons Portal
inline const char* get_motor_status_str(uint8_t status) {
    switch (status) {
        case MOTOR_MANUAL:           return "manual";
        case MOTOR_HOLDING_STATION:  return "holding_station";
        case MOTOR_ANCHORED:         return "anchored";
        case MOTOR_MOVING_TO_TARGET: return "moving_to_target";
        case MOTOR_RTL:              return "rtl";
        case MOTOR_FAILSAFE:         return "failsafe";
        case MOTOR_IDLE:
        default:                     return "idle";
    }
}

inline const char* get_action_str(uint8_t action) {
    switch (action) {
        case ACTION_HOLD_STATION:    return "HOLD_STATION";
        case ACTION_STOP:            return "STOP";
        case ACTION_DROP_ANCHOR:     return "DROP_ANCHOR";
        case ACTION_RETRIEVE_ANCHOR: return "RETRIEVE_ANCHOR";
        case ACTION_STOP_ANCHOR:     return "STOP_ANCHOR";
        case ACTION_NAVIGATE:        return "NAVIGATE";
        case ACTION_RTL:             return "RTL";
        case ACTION_ALARM:           return "ALARM";
        case ACTION_PING:            return "PING";
        case ACTION_MANUAL:          return "MANUAL";
        default:                     return "NONE";
    }
}

inline const char* get_anchor_status_str(uint8_t status) {
    switch (status) {
        case ANCHOR_DEPLOYING:  return "deploying";
        case ANCHOR_DEPLOYED:   return "deployed";
        case ANCHOR_RETRACTING: return "retracting";
        case ANCHOR_RETRACTED:
        default:                return "retracted";
    }
}
