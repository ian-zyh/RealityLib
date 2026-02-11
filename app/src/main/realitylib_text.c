/**
 * RealityLib Text - 3x5 Pixel Font Rendering for VR
 *
 * Implementation of bitmap text rendering using DrawVRCube as pixel output.
 */

#include "realitylib_text.h"
#include <math.h>
#include <string.h>

// =============================================================================
// Font Data - 3x5 Bitmap
// =============================================================================

// Each row: 3 bits, bit2=left, bit1=center, bit0=right
static const unsigned char fontDigits[10][5] = {
    {7,5,5,5,7}, {2,6,2,2,7}, {7,1,7,4,7}, {7,1,7,1,7}, {5,5,7,1,1},
    {7,4,7,1,7}, {7,4,7,5,7}, {7,1,1,1,1}, {7,5,7,5,7}, {7,5,7,1,7}
};

static const unsigned char fontAlpha[26][5] = {
    {2,5,7,5,5}, // A
    {6,5,6,5,6}, // B
    {3,4,4,4,3}, // C
    {6,5,5,5,6}, // D
    {7,4,7,4,7}, // E
    {7,4,6,4,4}, // F
    {7,4,5,5,7}, // G
    {5,5,7,5,5}, // H
    {7,2,2,2,7}, // I
    {1,1,1,5,2}, // J
    {5,6,4,6,5}, // K
    {4,4,4,4,7}, // L
    {5,7,7,5,5}, // M
    {5,7,7,5,5}, // N
    {7,5,5,5,7}, // O
    {7,5,7,4,4}, // P
    {7,5,5,7,1}, // Q
    {7,5,7,6,5}, // R
    {7,4,7,1,7}, // S
    {7,2,2,2,2}, // T
    {5,5,5,5,7}, // U
    {5,5,5,5,2}, // V
    {5,5,5,7,5}, // W
    {5,5,2,5,5}, // X
    {5,5,2,2,2}, // Y
    {7,1,2,4,7}  // Z
};

static const unsigned char* GetFontBitmap(char ch) {
    if (ch >= '0' && ch <= '9') return fontDigits[ch - '0'];
    if (ch >= 'A' && ch <= 'Z') return fontAlpha[ch - 'A'];
    if (ch >= 'a' && ch <= 'z') return fontAlpha[ch - 'a'];
    return NULL;
}

// =============================================================================
// Rendering Functions
// =============================================================================

void DrawPixelChar(char ch, Vector3 origin, float pixSize, Color color,
                   float faceAngle) {
    const unsigned char* bmp = GetFontBitmap(ch);
    if (!bmp || pixSize < 0.001f) return;
    float step = pixSize * 1.25f;
    float cosA = cosf(faceAngle);
    float sinA = sinf(faceAngle);
    for (int row = 0; row < 5; row++) {
        unsigned char bits = bmp[row];
        for (int col = 0; col < 3; col++) {
            if (bits & (4 >> col)) {
                float rx = col * step;
                Vector3 p = Vector3Create(
                    origin.x + rx * cosA,
                    origin.y - row * step,
                    origin.z + rx * sinA
                );
                DrawVRCube(p, pixSize, color);
            }
        }
    }
}

float GetTextWidth(const char* text, float pixSize) {
    float step = pixSize * 1.25f;
    float cw   = 3.0f * step + step;
    float w    = 0;
    for (int i = 0; text[i]; i++) {
        w += (text[i] == ' ') ? cw * 0.7f : cw;
    }
    return w;
}

void DrawPixelText(const char* text, Vector3 origin, float pixSize,
                   Color color, float faceAngle) {
    float step = pixSize * 1.25f;
    float cw   = 3.0f * step + step;
    float cosA = cosf(faceAngle);
    float sinA = sinf(faceAngle);
    Vector3 pos = origin;
    for (int i = 0; text[i]; i++) {
        if (text[i] == ' ') {
            float adv = cw * 0.7f;
            pos.x += adv * cosA;
            pos.z += adv * sinA;
            continue;
        }
        DrawPixelChar(text[i], pos, pixSize, color, faceAngle);
        pos.x += cw * cosA;
        pos.z += cw * sinA;
    }
}

void DrawTextCentered(const char* text, float cx, float y, float cz,
                      float pixSize, Color color, float faceAngle) {
    float w    = GetTextWidth(text, pixSize);
    float cosA = cosf(faceAngle);
    float sinA = sinf(faceAngle);
    Vector3 start = Vector3Create(cx - w * 0.5f * cosA, y, cz - w * 0.5f * sinA);
    DrawPixelText(text, start, pixSize, color, faceAngle);
}

void DrawNumberAt(int number, Vector3 origin, float pixSize, Color color,
                  float faceAngle) {
    char buf[16];
    int len = 0, n = number < 0 ? 0 : number;
    if (n == 0) { buf[len++] = '0'; }
    else {
        char tmp[16]; int tl = 0;
        while (n > 0 && tl < 15) { tmp[tl++] = '0' + (n % 10); n /= 10; }
        for (int i = tl - 1; i >= 0; i--) buf[len++] = tmp[i];
    }
    buf[len] = '\0';
    DrawPixelText(buf, origin, pixSize, color, faceAngle);
}

void DrawNumberCentered(int number, float cx, float y, float cz,
                        float pixSize, Color color, float faceAngle) {
    char buf[16];
    int len = 0, n = number < 0 ? 0 : number;
    if (n == 0) { buf[len++] = '0'; }
    else {
        char tmp[16]; int tl = 0;
        while (n > 0 && tl < 15) { tmp[tl++] = '0' + (n % 10); n /= 10; }
        for (int i = tl - 1; i >= 0; i--) buf[len++] = tmp[i];
    }
    buf[len] = '\0';
    DrawTextCentered(buf, cx, y, cz, pixSize, color, faceAngle);
}
