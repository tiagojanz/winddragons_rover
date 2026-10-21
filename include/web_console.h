#pragma once

#include <Arduino.h>
#include <cstdarg>

#define WEB_CONSOLE_MAX_LINES 100
#define WEB_CONSOLE_LINE_LEN  160

struct WebConsoleLine {
    uint32_t id;
    uint32_t ms;
    char text[WEB_CONSOLE_LINE_LEN];
};

class WebConsoleClass {
public:
    WebConsoleClass() : _head(0), _count(0), _counter(0) {}

    void log(const char* text) {
        if (!text || text[0] == '\0') return;

        // Limpar quebras de linha no fim
        char clean[WEB_CONSOLE_LINE_LEN];
        strncpy(clean, text, sizeof(clean) - 1);
        clean[sizeof(clean) - 1] = '\0';
        size_t len = strlen(clean);
        while (len > 0 && (clean[len - 1] == '\n' || clean[len - 1] == '\r')) {
            clean[--len] = '\0';
        }
        if (len == 0) return;

        uint32_t idx = (_head + _count) % WEB_CONSOLE_MAX_LINES;
        if (_count == WEB_CONSOLE_MAX_LINES) {
            _head = (_head + 1) % WEB_CONSOLE_MAX_LINES;
        } else {
            _count++;
        }

        _lines[idx].id = ++_counter;
        _lines[idx].ms = millis();
        strncpy(_lines[idx].text, clean, WEB_CONSOLE_LINE_LEN - 1);
        _lines[idx].text[WEB_CONSOLE_LINE_LEN - 1] = '\0';
    }

    void logf(const char* format, ...) {
        char buf[WEB_CONSOLE_LINE_LEN];
        va_list args;
        va_start(args, format);
        vsnprintf(buf, sizeof(buf), format, args);
        va_end(args);
        log(buf);
    }

    String getJsonSince(uint32_t lastId) const {
        String json = "{\"last_id\":";
        json += String(_counter);
        json += ",\"lines\":[";

        bool first = true;
        // Se lastId == 0 ou se o cliente ficou muito para trás, envia apenas as últimas 40 linhas
        uint32_t startIdx = 0;
        if ((lastId == 0 || (_counter > lastId && (_counter - lastId) > 40)) && _count > 40) {
            startIdx = _count - 40;
        }

        for (uint32_t i = startIdx; i < _count; i++) {
            uint32_t actualIdx = (_head + i) % WEB_CONSOLE_MAX_LINES;
            const WebConsoleLine &l = _lines[actualIdx];
            if (l.id > lastId) {
                if (!first) json += ",";
                first = false;
                json += "{\"id\":";
                json += String(l.id);
                json += ",\"ms\":";
                json += String(l.ms);
                json += ",\"msg\":";
                escapeJsonString(json, l.text);
                json += "}";
            }
        }
        json += "]}";
        return json;
    }

    uint32_t getLastId() const { return _counter; }

    void clear() {
        _head = 0;
        _count = 0;
    }

private:
    WebConsoleLine _lines[WEB_CONSOLE_MAX_LINES];
    uint32_t _head;
    uint32_t _count;
    uint32_t _counter;

    static void escapeJsonString(String &dest, const char* s) {
        dest += "\"";
        while (*s) {
            if (*s == '"') dest += "\\\"";
            else if (*s == '\\') dest += "\\\\";
            else if (*s == '\n') dest += "\\n";
            else if (*s == '\r') dest += "\\r";
            else if (*s == '\t') dest += "\\t";
            else if (static_cast<unsigned char>(*s) < 32) {
                // Ignore other non-printable control chars
            } else {
                dest += *s;
            }
            s++;
        }
        dest += "\"";
    }
};

extern WebConsoleClass webConsole;
