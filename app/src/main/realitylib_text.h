/**
 * RealityLib Text - 3x5 Pixel Font Rendering for VR
 *
 * Provides bitmap-based text rendering using small cubes as pixels.
 * Supports A-Z (case-insensitive), 0-9, and spaces.
 * All text is rendered facing a given angle for VR HUD positioning.
 */

#pragma once

#include "realitylib_vr.h"

// Draw a single character at the given origin
void DrawPixelChar(char ch, Vector3 origin, float pixSize, Color color,
                   float faceAngle);

// Get the width (in world units) of a rendered string
float GetTextWidth(const char* text, float pixSize);

// Draw a string starting at origin
void DrawPixelText(const char* text, Vector3 origin, float pixSize,
                   Color color, float faceAngle);

// Draw a string centered horizontally around (cx, y, cz)
void DrawTextCentered(const char* text, float cx, float y, float cz,
                      float pixSize, Color color, float faceAngle);

// Draw an integer at origin
void DrawNumberAt(int number, Vector3 origin, float pixSize, Color color,
                  float faceAngle);

// Draw an integer centered horizontally around (cx, y, cz)
void DrawNumberCentered(int number, float cx, float y, float cz,
                        float pixSize, Color color, float faceAngle);
