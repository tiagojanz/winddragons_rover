#pragma once

#include <stdint.h>
#include <string.h>
#include <strings.h>

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
    ACTION_DROP_ANCHOR     = 3, // Lower physical anchor (watches Hall sensor & auto-locks on stop)
    ACTION_RETRIEVE_ANCHOR = 4, // Retrieve physical anchor
    ACTION_STOP_ANCHOR     = 5, // Emergency stop anchor winch
    ACTION_NAVIGATE        = 6, // Navigate to target coordinates (GOTO)
    ACTION_RTL             = 7, // Return to Base / Launch
    ACTION_ALARM           = 8, // Strobe LED & acoustic alarm
    ACTION_PING            = 9, // Request immediate telemetry
    ACTION_MANUAL          = 10,// Manual joystick control
    ACTION_RELAX_RACK      = 13,// Relax rack servo
    ACTION_LOWER_WINCH     = 14,// Lower winch continuously (1200us)
    ACTION_ZERO_ANCHOR     = 15,// Zero anchor Hall counter
    ACTION_ENGAGE_RACK     = 16,// Lock/engage anchor rack
    ACTION_RELEASE_ANCHOR  = 17,// Release anchor/rack freely without sensor auto-lock ("Libertar")
    ACTION_SET_BUOY_ROLE   = 18,// Set regatta buoy LED role/rounding (BuoyLightRole in target_depth_cm)
    ACTION_SET_HOME        = 19,// Set Rover Home / RTL coordinates (current GPS or target_lat/lng)
    ACTION_SET_LED_ANIMS   = 20 // Set per-mode Nautical Animation bitmasks (packed in target_lat/lng or single mode in target_depth_cm/duration_s)
};

// Bitmask de Animação e Sinalização Náutica (Fita WS2812B - GPIO 19)
// Permite selecionar uma única animação, OFF (0x00) ou combinar múltiplas animações por Modo de Operação (0..6)
enum NauticalAnimBit : uint8_t {
    NAV_ANIM_OFF     = 0x00, // Desligado (OFF)
    NAV_ANIM_STROBE  = 0x01, // Bit 0: Strobe Náutico (Duplo Flash branco)
    NAV_ANIM_NAV     = 0x02, // Bit 1: Luzes de Navegação (Bombordo Vermelho / Estibordo Verde)
    NAV_ANIM_BEACON  = 0x04, // Bit 2: Farol Âmbar (Beacon Rotativo 2Hz)
    NAV_ANIM_RAINBOW = 0x08, // Bit 3: Arco-Íris (Rainbow Ciclo HSV)
    NAV_ANIM_CHASE   = 0x10, // Bit 4: Scanner Pixel-a-Pixel (Chase)
    NAV_ANIM_POLICE  = 0x20, // Bit 5: Emergência (Vermelho / Azul)
    NAV_ANIM_BUOY    = 0x40  // Bit 6: Sinalização de Boia de Regata (BB/EB/Largada/Chegada/Porta)
};

// Configuração padrão de animação náutica por cada MotorStatus (0=IDLE .. 6=FAILSAFE)
static constexpr uint8_t DEFAULT_ROVER_LED_MODE_ANIMS[7] = {
    NAV_ANIM_BUOY,   // 0: MOTOR_IDLE (Boia Regata se atribuída, senão OFF)
    NAV_ANIM_NAV,    // 1: MOTOR_MANUAL (Luzes de Navegação BB/EB)
    NAV_ANIM_BUOY,   // 2: MOTOR_HOLDING_STATION (Boia Regata)
    NAV_ANIM_BUOY,   // 3: MOTOR_ANCHORED (Boia Regata)
    NAV_ANIM_NAV,    // 4: MOTOR_MOVING_TO_TARGET (Luzes de Navegação BB/EB)
    NAV_ANIM_BEACON, // 5: MOTOR_RTL (Farol Âmbar)
    NAV_ANIM_BEACON  // 6: MOTOR_FAILSAFE (Farol Âmbar 2Hz)
};

// Empacota os 7 bytes de máscaras de animação (modos 0..6) nos campos int32_t target_lat_deg7 e target_lng_deg7
inline void pack_led_mode_anims(const uint8_t masks[7], int32_t &outLat7, int32_t &outLng7) {
    uint32_t u0 = (static_cast<uint32_t>(masks[0] & 0x7F)) |
                  (static_cast<uint32_t>(masks[1] & 0x7F) << 8) |
                  (static_cast<uint32_t>(masks[2] & 0x7F) << 16) |
                  (static_cast<uint32_t>(masks[3] & 0x7F) << 24);
    uint32_t u1 = (static_cast<uint32_t>(masks[4] & 0x7F)) |
                  (static_cast<uint32_t>(masks[5] & 0x7F) << 8) |
                  (static_cast<uint32_t>(masks[6] & 0x7F) << 16) |
                  (0xA5UL << 24);
    outLat7 = static_cast<int32_t>(u0);
    outLng7 = static_cast<int32_t>(u1);
}

// Desempacota os 7 bytes de máscaras de animação (modos 0..6) a partir de target_lat_deg7 e target_lng_deg7
inline bool unpack_led_mode_anims(int32_t lat7, int32_t lng7, uint8_t outMasks[7]) {
    uint32_t u0 = static_cast<uint32_t>(lat7);
    uint32_t u1 = static_cast<uint32_t>(lng7);
    if (((u1 >> 24) & 0xFF) != 0xA5) {
        return false;
    }
    outMasks[0] = static_cast<uint8_t>(u0 & 0x7F);
    outMasks[1] = static_cast<uint8_t>((u0 >> 8) & 0x7F);
    outMasks[2] = static_cast<uint8_t>((u0 >> 16) & 0x7F);
    outMasks[3] = static_cast<uint8_t>((u0 >> 24) & 0x7F);
    outMasks[4] = static_cast<uint8_t>(u1 & 0x7F);
    outMasks[5] = static_cast<uint8_t>((u1 >> 8) & 0x7F);
    outMasks[6] = static_cast<uint8_t>((u1 >> 16) & 0x7F);
    return true;
}

// Regatta Buoy LED Role / Rounding signaling on Rover WS2812B LEDs
enum BuoyLightRole : uint8_t {
    BUOY_ROLE_NONE      = 0, // Sem função obrigatória ("none") -> LEDs apagados em standby
    BUOY_ROLE_PORT      = 1, // Bombordo ("port" / BB) -> Vermelho náutico
    BUOY_ROLE_STARBOARD = 2, // Estibordo ("starboard" / EB) -> Verde náutico
    BUOY_ROLE_START     = 3, // Largada ("start") -> Amarelo / Âmbar
    BUOY_ROLE_FINISH    = 4, // Meta / Chegada ("finish") -> Azul / Ciano
    BUOY_ROLE_GATE      = 5  // Porta intermédia ("gate") -> Bombordo/Estibordo (Vermelho + Verde)
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

    // BNO055 9-DOF IMU / Compass Telemetry
    int16_t  roll_deg10;     // Roll angle in degrees * 10 (e.g. -12.5° -> -125)
    int16_t  pitch_deg10;    // Pitch angle in degrees * 10 (e.g. +5.3° -> 53)
    uint8_t  compass_flags;  // Bit 7: active (1=BNO055, 0=GPS_COG), Bit 6: home_set (1=Home RTL set), Bits 5..4: cal_sys (0..3), Bits 3..2: cal_mag (0..3), Bits 1..0: cal_imu (0..3)

    uint16_t checksum;
};

// Size of legacy PacketTelemetry before BNO055 fields were added (for backward-compatible RX)
static constexpr size_t LORA_LEGACY_TELEMETRY_SIZE = sizeof(PacketTelemetry) - 5;

inline uint8_t pack_compass_flags(bool active, uint8_t cal_sys, uint8_t cal_mag, uint8_t cal_gyro = 0, uint8_t cal_accel = 0, bool home_set = false) {
    uint8_t imu = (cal_gyro < cal_accel) ? cal_gyro : cal_accel;
    return (active ? 0x80 : 0x00) |
           (home_set ? 0x40 : 0x00) |
           ((cal_sys & 0x03) << 4) |
           ((cal_mag & 0x03) << 2) |
           (imu & 0x03);
}

inline bool unpack_compass_active(uint8_t flags) {
    return (flags & 0x80) != 0;
}

inline bool unpack_home_set(uint8_t flags) {
    return (flags & 0x40) != 0;
}

inline uint8_t unpack_compass_cal_sys(uint8_t flags) {
    return (flags >> 4) & 0x03;
}

inline uint8_t unpack_compass_cal_mag(uint8_t flags) {
    return (flags >> 2) & 0x03;
}

inline uint8_t unpack_compass_cal_imu(uint8_t flags) {
    return flags & 0x03;
}

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

// ------------------------------------------
// 4. Poll Request Packet (Base -> Rover)
// ------------------------------------------
struct __attribute__((packed)) PacketPollRequest {
    uint8_t  magic;          // 0xD9
    uint8_t  msg_type;       // MSG_POLL_REQUEST (0x40)
    uint8_t  target_rover_id;// Target Rover ID (or 0xFF for broadcast)
    uint16_t poll_seq;       // Incremental poll sequence counter
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
        case ACTION_RELAX_RACK:      return "RELAX_RACK";
        case ACTION_LOWER_WINCH:     return "LOWER_WINCH";
        case ACTION_ZERO_ANCHOR:     return "ZERO_ANCHOR";
        case ACTION_ENGAGE_RACK:     return "LOCK_ANCHOR";
        case ACTION_RELEASE_ANCHOR:  return "RELEASE_ANCHOR";
        case ACTION_SET_BUOY_ROLE:   return "SET_BUOY_ROLE";
        case ACTION_SET_HOME:        return "SET_RTL";
        case ACTION_SET_LED_ANIMS:   return "SET_LED_ANIMS";
        default:                     return "NONE";
    }
}

inline const char* get_buoy_light_role_str(uint8_t bRole) {
    switch (bRole) {
        case BUOY_ROLE_PORT:      return "PORT_BB";
        case BUOY_ROLE_STARBOARD: return "STARBOARD_EB";
        case BUOY_ROLE_START:     return "START";
        case BUOY_ROLE_FINISH:    return "FINISH";
        case BUOY_ROLE_GATE:      return "GATE";
        case BUOY_ROLE_NONE:
        default:                  return "NONE";
    }
}

inline uint8_t resolve_buoy_light_role(const char* role, const char* rounding) {
    const char* r = role ? role : "";
    const char* rnd = rounding ? rounding : "";

    // 1. Largada ("start" / "largada")
    if (strcasecmp(r, "start") == 0 || strcasestr(r, "largada") != nullptr) {
        return BUOY_ROLE_START;
    }
    // 2. Chegada / Meta ("finish" / "meta" / "chegada")
    if (strcasecmp(r, "finish") == 0 || strcasestr(r, "meta") != nullptr ||
        strcasestr(r, "chegada") != nullptr) {
        return BUOY_ROLE_FINISH;
    }
    // 3. Bombordo ("port" / "bombordo" / "bb")
    if (strcasecmp(rnd, "port") == 0 || strcasestr(rnd, "bombordo") != nullptr ||
        strcasecmp(rnd, "bb") == 0 || strcasestr(r, "bombordo") != nullptr) {
        return BUOY_ROLE_PORT;
    }
    // 4. Estibordo ("starboard" / "estibordo" / "eb")
    if (strcasecmp(rnd, "starboard") == 0 ||
        strcasestr(rnd, "estibordo") != nullptr ||
        strcasecmp(rnd, "eb") == 0 || strcasestr(r, "estibordo") != nullptr) {
        return BUOY_ROLE_STARBOARD;
    }
    // 5. Porta ("gate" / "porta")
    if (strcasecmp(r, "gate") == 0 || strcasestr(r, "porta") != nullptr ||
        strcasecmp(rnd, "gate") == 0) {
        return BUOY_ROLE_GATE;
    }
    return BUOY_ROLE_NONE;
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
