/**
 * RealityLib VR - Simple VR Framework for Meta Quest
 * 
 * This header provides a simple API for creating VR applications.
 * Users only need to modify main.c and use these functions.
 */

#ifndef REALITYLIB_VR_H
#define REALITYLIB_VR_H

#include <stdbool.h>
#include <android_native_app_glue.h>

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// Basic Types (raylib-compatible)
// =============================================================================

typedef struct Vector3 {
    float x;
    float y;
    float z;
} Vector3;

typedef struct Vector4 {
    float x;
    float y;
    float z;
    float w;
} Vector4;

typedef struct Quaternion {
    float x;
    float y;
    float z;
    float w;
} Quaternion;

typedef struct Matrix {
    float m0, m4, m8, m12;
    float m1, m5, m9, m13;
    float m2, m6, m10, m14;
    float m3, m7, m11, m15;
} Matrix;

typedef struct Color {
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;
} Color;

// Common colors
#define LIGHTGRAY  (Color){ 200, 200, 200, 255 }
#define GRAY       (Color){ 130, 130, 130, 255 }
#define DARKGRAY   (Color){ 80, 80, 80, 255 }
#define YELLOW     (Color){ 253, 249, 0, 255 }
#define GOLD       (Color){ 255, 203, 0, 255 }
#define ORANGE     (Color){ 255, 161, 0, 255 }
#define PINK       (Color){ 255, 109, 194, 255 }
#define RED        (Color){ 230, 41, 55, 255 }
#define MAROON     (Color){ 190, 33, 55, 255 }
#define GREEN      (Color){ 0, 228, 48, 255 }
#define LIME       (Color){ 0, 158, 47, 255 }
#define DARKGREEN  (Color){ 0, 117, 44, 255 }
#define SKYBLUE    (Color){ 102, 191, 255, 255 }
#define BLUE       (Color){ 0, 121, 241, 255 }
#define DARKBLUE   (Color){ 0, 82, 172, 255 }
#define PURPLE     (Color){ 200, 122, 255, 255 }
#define VIOLET     (Color){ 135, 60, 190, 255 }
#define DARKPURPLE (Color){ 112, 31, 126, 255 }
#define BEIGE      (Color){ 211, 176, 131, 255 }
#define BROWN      (Color){ 127, 106, 79, 255 }
#define DARKBROWN  (Color){ 76, 63, 47, 255 }
#define WHITE      (Color){ 255, 255, 255, 255 }
#define BLACK      (Color){ 0, 0, 0, 255 }
#define BLANK      (Color){ 0, 0, 0, 0 }
#define MAGENTA    (Color){ 255, 0, 255, 255 }
#define RAYWHITE   (Color){ 245, 245, 245, 255 }

// =============================================================================
// VR Controller Data
// =============================================================================

typedef enum {
    CONTROLLER_LEFT = 0,
    CONTROLLER_RIGHT = 1
} ControllerHand;

typedef struct VRController {
    Vector3 position;
    Quaternion orientation;
    Vector3 velocity;
    Vector3 angularVelocity;
    
    // Buttons (0.0 - 1.0 for triggers, 0 or 1 for buttons)
    float trigger;          // Index trigger
    float grip;             // Grip/squeeze
    float thumbstickX;      // Thumbstick X axis (-1 to 1)
    float thumbstickY;      // Thumbstick Y axis (-1 to 1)
    bool thumbstickClick;   // Thumbstick pressed
    bool buttonA;           // A button (right) / X button (left)
    bool buttonB;           // B button (right) / Y button (left)
    bool menuButton;        // Menu button
    
    bool isTracking;        // Controller is being tracked
} VRController;

typedef struct VRHeadset {
    Vector3 position;
    Quaternion orientation;
    Vector3 velocity;
    Vector3 angularVelocity;
    
    // Per-eye data
    Matrix leftEyeProjection;
    Matrix rightEyeProjection;
    Matrix leftEyeView;
    Matrix rightEyeView;
    Vector3 leftEyePosition;
    Vector3 rightEyePosition;
    
    // Display info
    int displayWidth;
    int displayHeight;
    float displayRefreshRate;
} VRHeadset;

// =============================================================================
// Core VR Functions - Application Lifecycle
// =============================================================================

/**
 * Initialize the VR application
 * Call this once at the start of your application
 * @param app Android native app handle
 * @return true if initialization succeeded
 */
bool InitApp(struct android_app* app);

/**
 * Close and cleanup the VR application
 * Call this before exiting
 * @param app Android native app handle
 */
void CloseApp(struct android_app* app);

/**
 * Check if the application should close
 * Use this as your main loop condition
 * @param app Android native app handle
 * @return true if app should close
 */
bool AppShouldClose(struct android_app* app);

// =============================================================================
// VR Rendering Functions
// =============================================================================

/**
 * Begin VR rendering for this frame
 * Call this at the start of each frame before drawing
 */
void BeginVRMode(void);

/**
 * End VR rendering for this frame
 * Call this at the end of each frame after drawing
 */
void EndVRMode(void);

/**
 * Set the clear/background color for VR rendering
 * @param color Background color
 */
void SetVRClearColor(Color color);

// =============================================================================
// Input Functions
// =============================================================================

/**
 * Sync controller and headset data for this frame
 * Call this once per frame before reading input
 */
void SyncControllers(void);

/**
 * Get controller data
 * @param hand Which controller (CONTROLLER_LEFT or CONTROLLER_RIGHT)
 * @return Controller state data
 */
VRController GetController(ControllerHand hand);

/**
 * Get headset data
 * @return Headset state data
 */
VRHeadset GetHeadset(void);

/**
 * Get controller position (convenience function)
 * @param hand Which controller (0=left, 1=right)
 * @return Controller position in world space
 */
Vector3 GetVRControllerPosition(int hand);

/**
 * Get controller orientation (convenience function)
 * @param hand Which controller (0=left, 1=right)
 * @return Controller orientation as quaternion
 */
Quaternion GetVRControllerOrientation(int hand);

/**
 * Get controller grip trigger value
 * @param hand Which controller (0=left, 1=right)
 * @return Grip value 0.0 to 1.0
 */
float GetVRControllerGrip(int hand);

/**
 * Get controller index trigger value
 * @param hand Which controller (0=left, 1=right)
 * @return Trigger value 0.0 to 1.0
 */
float GetVRControllerTrigger(int hand);

/**
 * Get controller thumbstick values
 * @param hand Which controller (0=left, 1=right)
 * @return X and Y as x,y in Vector3 (z unused)
 */
Vector3 GetVRControllerThumbstick(int hand);

/**
 * Trigger haptic feedback on a controller
 * @param hand Which controller (0=left, 1=right)
 * @param amplitude Vibration strength (0.0 to 1.0)
 * @param duration Duration in seconds
 */
void TriggerVRHaptic(int hand, float amplitude, float duration);

// =============================================================================
// Player Movement Functions
// =============================================================================

/**
 * Set the player's world position offset
 * This offsets all VR rendering to simulate movement
 * @param position New player position
 */
void SetPlayerPosition(Vector3 position);

/**
 * Get the current player position
 * @return Current player position
 */
Vector3 GetPlayerPosition(void);

/**
 * Set the player's Y-axis rotation (yaw)
 * @param yaw Rotation in degrees
 */
void SetPlayerYaw(float yaw);

/**
 * Get the current player Y-axis rotation
 * @return Current yaw in degrees
 */
float GetPlayerYaw(void);

/**
 * Apply movement relative to player's current orientation
 * @param forward Forward/backward movement (positive = forward)
 * @param strafe Left/right movement (positive = right)
 * @param up Vertical movement (positive = up)
 */
void MovePlayer(float forward, float strafe, float up);

/**
 * Check if player is on the ground (for jump logic)
 * @param groundHeight The Y position of the ground
 * @return true if player is on or below ground height
 */
bool IsPlayerGrounded(float groundHeight);

// =============================================================================
// VR Drawing Functions (Simple API)
// =============================================================================

/**
 * Draw a cuboid in VR space
 * @param position Center position
 * @param size Width, height, depth
 * @param color RGB color (each component 0.0 to 1.0)
 */
void DrawVRCuboid(Vector3 position, Vector3 size, Vector3 color);

/**
 * Draw a cube with Color type
 * @param position Center position
 * @param size Uniform size
 * @param color RGBA color
 */
void DrawVRCube(Vector3 position, float size, Color color);

/**
 * Draw a sphere in VR space
 * @param position Center position
 * @param radius Sphere radius
 * @param color RGBA color
 */
void DrawVRSphere(Vector3 position, float radius, Color color);

/**
 * Draw a floor grid
 * @param slices Number of grid divisions
 * @param spacing Distance between grid lines
 */
void DrawVRGrid(int slices, float spacing);

/**
 * Draw a line in 3D space
 * @param startPos Starting position
 * @param endPos Ending position
 * @param color Line color
 */
void DrawVRLine3D(Vector3 startPos, Vector3 endPos, Color color);

/**
 * Draw a cylinder
 * @param position Base center position
 * @param radiusTop Top radius
 * @param radiusBottom Bottom radius
 * @param height Cylinder height
 * @param color RGBA color
 */
void DrawVRCylinder(Vector3 position, float radiusTop, float radiusBottom, float height, Color color);

/**
 * Draw a plane (quad)
 * @param centerPos Center position
 * @param size Width and length
 * @param color RGBA color
 */
void DrawVRPlane(Vector3 centerPos, Vector3 size, Color color);

/**
 * Draw coordinate axes at a position
 * @param position Position to draw axes
 * @param scale Size of the axes
 */
void DrawVRAxes(Vector3 position, float scale);

// =============================================================================
// Math Helper Functions
// =============================================================================

/**
 * Create a Vector3
 */
Vector3 Vector3Create(float x, float y, float z);

/**
 * Add two Vector3s
 */
Vector3 Vector3Add(Vector3 v1, Vector3 v2);

/**
 * Subtract two Vector3s
 */
Vector3 Vector3Subtract(Vector3 v1, Vector3 v2);

/**
 * Scale a Vector3
 */
Vector3 Vector3Scale(Vector3 v, float scalar);

/**
 * Get the length of a Vector3
 */
float Vector3Length(Vector3 v);

/**
 * Calculate distance between two Vector3s
 */
float Vector3Distance(Vector3 v1, Vector3 v2);

/**
 * Normalize a Vector3
 */
Vector3 Vector3Normalize(Vector3 v);

/**
 * Get forward direction from a quaternion
 */
Vector3 QuaternionForward(Quaternion q);

/**
 * Get right direction from a quaternion
 */
Vector3 QuaternionRight(Quaternion q);

/**
 * Get up direction from a quaternion
 */
Vector3 QuaternionUp(Quaternion q);

// =============================================================================
// Custom Loop Function (User Implements This)
// =============================================================================

/**
 * Called every frame inside the VR loop
 * User should implement this function in their main.c
 * @param app Android native app handle
 */
void inLoop(struct android_app* app);

#ifdef __cplusplus
}
#endif

#endif // REALITYLIB_VR_H
