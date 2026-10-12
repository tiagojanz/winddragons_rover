#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <esp_mac.h>
#include <qrcode.h>
#include <canvas/Arduino_Canvas.h>

inline String getDeviceMacAddress() {
    String mac = WiFi.macAddress();
    if (mac.isEmpty() || mac == "00:00:00:00:00:00") {
        uint8_t baseMac[6];
        esp_read_mac(baseMac, ESP_MAC_WIFI_STA);
        char macStr[18];
        snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
                 baseMac[0], baseMac[1], baseMac[2], baseMac[3], baseMac[4], baseMac[5]);
        mac = String(macStr);
    }
    return mac;
}

inline void drawQrCode(Arduino_Canvas *canvas, const char *text, int boxX, int boxY, int boxW, int boxH) {
    if (!canvas || !text || strlen(text) == 0) return;

    struct QrCanvasContext {
        Arduino_Canvas *cv;
        int x, y, w, h;
    };
    static QrCanvasContext ctx;
    ctx.cv = canvas;
    ctx.x = boxX;
    ctx.y = boxY;
    ctx.w = boxW;
    ctx.h = boxH;

    esp_qrcode_config_t cfg = {
        .display_func = [](esp_qrcode_handle_t qrcode) {
            int size = esp_qrcode_get_size(qrcode);
            if (size <= 0 || !ctx.cv) return;

            int minBox = (ctx.w < ctx.h) ? ctx.w : ctx.h;
            int margin = (minBox / (size + 4) >= 3) ? 2 : 1;
            int total = size + 2 * margin;
            int scale = minBox / total;
            if (scale < 1) scale = 1;

            int actualPx = total * scale;
            int startX = ctx.x + (ctx.w - actualPx) / 2;
            int startY = ctx.y + (ctx.h - actualPx) / 2;

            // Draw white background
            ctx.cv->fillRect(startX, startY, actualPx, actualPx, 0xFFFF /* UI_WHITE */);

            // Draw modules
            for (int r = 0; r < size; r++) {
                for (int c = 0; c < size; c++) {
                    if (esp_qrcode_get_module(qrcode, c, r)) {
                        ctx.cv->fillRect(startX + (c + margin) * scale,
                                         startY + (r + margin) * scale,
                                         scale, scale, 0x0000 /* UI_BLACK */);
                    }
                }
            }
        },
        .max_qrcode_version = 4,
        .qrcode_ecc_level = ESP_QRCODE_ECC_LOW,
    };
    esp_qrcode_generate(&cfg, text);
}

inline String generateQrCodeSvg(const char *text, int pixelSize = 160) {
    if (!text || strlen(text) == 0) return "";

    struct SvgContext {
        String svg;
        int targetSize;
    };
    static SvgContext ctx;
    ctx.svg = "";
    ctx.targetSize = pixelSize;

    esp_qrcode_config_t cfg = {
        .display_func = [](esp_qrcode_handle_t qrcode) {
            int size = esp_qrcode_get_size(qrcode);
            if (size <= 0) return;

            int margin = 2;
            int total = size + 2 * margin;

            ctx.svg.reserve(2048);
            ctx.svg = "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 " + String(total) + " " + String(total) + 
                      "' width='" + String(ctx.targetSize) + "' height='" + String(ctx.targetSize) + 
                      "' style='background:#ffffff;border-radius:12px;padding:8px;box-shadow:0 4px 6px -1px rgba(0,0,0,0.3);display:inline-block;'><path fill='#0f172a' shape-rendering='crispEdges' d='";

            for (int r = 0; r < size; r++) {
                int c = 0;
                while (c < size) {
                    if (esp_qrcode_get_module(qrcode, c, r)) {
                        int startC = c;
                        while (c < size && esp_qrcode_get_module(qrcode, c, r)) {
                            c++;
                        }
                        int runLen = c - startC;
                        ctx.svg += "M" + String(startC + margin) + "," + String(r + margin) + "h" + String(runLen) + "v1h-" + String(runLen) + "z ";
                    } else {
                        c++;
                    }
                }
            }
            ctx.svg += "'/></svg>";
        },
        .max_qrcode_version = 4,
        .qrcode_ecc_level = ESP_QRCODE_ECC_LOW,
    };
    esp_qrcode_generate(&cfg, text);
    return ctx.svg;
}
