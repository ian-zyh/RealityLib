/**
 * RealityLib Hand Tracking Module
 * 
 * Provides hand tracking support for VR applications using OpenXR.
 * This module is optional - apps can work with controllers only.
 * 
 * Usage:
 *   1. Call InitHandTracking() after InitApp()
 *   2. Call UpdateHandTracking() each frame to get latest data
 *   3. Use GetHand() or individual joint functions to read hand state
 *   4. Call ShutdownHandTracking() before CloseApp()
 */

#ifndef REALITYLIB_HANDS_H
#define REALITYLIB_HANDS_H

#include <stdbool.h>
#include "realitylib_vr.h"

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// Hand Joint Indices (OpenXR XR_HAND_JOINT_EXT standard)
// =============================================================================

typedef enum {
    HAND_JOINT_PALM = 0,
    HAND_JOINT_WRIST = 1,
    HAND_JOINT_THUMB_METACARPAL = 2,
    HAND_JOINT_THUMB_PROXIMAL = 3,
    HAND_JOINT_THUMB_DISTAL = 4,
    HAND_JOINT_THUMB_TIP = 5,
    HAND_JOINT_INDEX_METACARPAL = 6,
    HAND_JOINT_INDEX_PROXIMAL = 7,
    HAND_JOINT_INDEX_INTERMEDIATE = 8,
    HAND_JOINT_INDEX_DISTAL = 9,
    HAND_JOINT_INDEX_TIP = 10,
    HAND_JOINT_MIDDLE_METACARPAL = 11,
    HAND_JOINT_MIDDLE_PROXIMAL = 12,
    HAND_JOINT_MIDDLE_INTERMEDIATE = 13,
    HAND_JOINT_MIDDLE_DISTAL = 14,
    HAND_JOINT_MIDDLE_TIP = 15,
    HAND_JOINT_RING_METACARPAL = 16,
    HAND_JOINT_RING_PROXIMAL = 17,
    HAND_JOINT_RING_INTERMEDIATE = 18,
    HAND_JOINT_RING_DISTAL = 19,
    HAND_JOINT_RING_TIP = 20,
    HAND_JOINT_LITTLE_METACARPAL = 21,
    HAND_JOINT_LITTLE_PROXIMAL = 22,
    HAND_JOINT_LITTLE_INTERMEDIATE = 23,
    HAND_JOINT_LITTLE_DISTAL = 24,
    HAND_JOINT_LITTLE_TIP = 25,
    HAND_JOINT_COUNT = 26
} HandJoint;

// =============================================================================
// Hand Data Structures
// =============================================================================

/**
 * Single joint data
 */
typedef struct {
    Vector3 position;       // World-space position
    Quaternion orientation; // World-space orientation
    float radius;           // Joint radius in meters
    bool isValid;           // True if tracking data is valid
} VRHandJoint;

/**
 * Complete hand tracking data
 */
typedef struct {
    VRHandJoint joints[HAND_JOINT_COUNT];  // All 26 joints
    
    // Convenience accessors (derived from joints)
    Vector3 palmPosition;      // Palm center position
    Quaternion palmOrientation; // Palm orientation
    Vector3 palmNormal;        // Direction palm is facing
    Vector3 palmDirection;     // Direction fingers are pointing
    
    // Tracking state
    bool isTracking;           // True if hand is currently being tracked
    bool isActive;             // True if hand tracking is enabled for this hand
    
    // Gesture detection (simple built-in gestures)
    bool isPinching;           // Thumb and index finger close together
    float pinchStrength;       // 0.0 to 1.0, how close to full pinch
    bool isFist;               // Hand is making a fist
    bool isPointing;           // Index finger extended, others curled
    bool isOpen;               // All fingers extended (open hand)
} VRHand;

// =============================================================================
// Initialization Functions
// =============================================================================

/**
 * Initialize hand tracking
 * Call this after InitApp() has succeeded
 * @return true if hand tracking is available and initialized
 */
bool InitHandTracking(void);

/**
 * Check if hand tracking is available on this device
 * @return true if hand tracking extension is supported
 */
bool IsHandTrackingAvailable(void);

/**
 * Check if hand tracking is currently active
 * @return true if hand tracking was successfully initialized
 */
bool IsHandTrackingActive(void);

/**
 * Shutdown hand tracking
 * Call this before CloseApp()
 */
void ShutdownHandTracking(void);

// =============================================================================
// Per-Frame Update
// =============================================================================

/**
 * Update hand tracking data for this frame
 * Call this once per frame before reading hand data
 * Usually called alongside SyncControllers()
 */
void UpdateHandTracking(void);

// =============================================================================
// Hand Data Access
// =============================================================================

/**
 * Get complete hand tracking data
 * @param hand Which hand (CONTROLLER_LEFT or CONTROLLER_RIGHT)
 * @return Complete hand data structure
 */
VRHand GetHand(ControllerHand hand);

/**
 * Get left hand data (convenience function)
 * @return Left hand tracking data
 */
VRHand GetLeftHand(void);

/**
 * Get right hand data (convenience function)
 * @return Right hand tracking data
 */
VRHand GetRightHand(void);

/**
 * Check if a specific hand is being tracked
 * @param hand Which hand
 * @return true if hand is currently tracked
 */
bool IsHandTracked(ControllerHand hand);

// =============================================================================
// Joint Access Functions
// =============================================================================

/**
 * Get a specific joint's position
 * @param hand Which hand
 * @param joint Which joint (use HandJoint enum)
 * @return Joint position in world space
 */
Vector3 GetHandJointPosition(ControllerHand hand, HandJoint joint);

/**
 * Get a specific joint's orientation
 * @param hand Which hand
 * @param joint Which joint
 * @return Joint orientation as quaternion
 */
Quaternion GetHandJointOrientation(ControllerHand hand, HandJoint joint);

/**
 * Get a specific joint's radius
 * @param hand Which hand
 * @param joint Which joint
 * @return Joint radius in meters
 */
float GetHandJointRadius(ControllerHand hand, HandJoint joint);

// =============================================================================
// Convenience Functions for Common Joints
// =============================================================================

/**
 * Get fingertip positions
 */
Vector3 GetThumbTip(ControllerHand hand);
Vector3 GetIndexTip(ControllerHand hand);
Vector3 GetMiddleTip(ControllerHand hand);
Vector3 GetRingTip(ControllerHand hand);
Vector3 GetLittleTip(ControllerHand hand);

/**
 * Get palm position
 */
Vector3 GetPalmPosition(ControllerHand hand);

/**
 * Get wrist position
 */
Vector3 GetWristPosition(ControllerHand hand);

// =============================================================================
// Gesture Detection
// =============================================================================

/**
 * Check if hand is making a pinch gesture
 * @param hand Which hand
 * @return true if pinching
 */
bool IsHandPinching(ControllerHand hand);

/**
 * Get pinch strength (how close thumb and index are)
 * @param hand Which hand
 * @return 0.0 (not pinching) to 1.0 (fully pinched)
 */
float GetPinchStrength(ControllerHand hand);

/**
 * Get the pinch point (midpoint between thumb and index tips)
 * @param hand Which hand
 * @return Pinch position in world space
 */
Vector3 GetPinchPosition(ControllerHand hand);

/**
 * Check if hand is making a fist
 * @param hand Which hand
 * @return true if making fist
 */
bool IsHandFist(ControllerHand hand);

/**
 * Check if hand is pointing (index extended)
 * @param hand Which hand
 * @return true if pointing
 */
bool IsHandPointing(ControllerHand hand);

/**
 * Get pointing direction (from wrist through index finger)
 * @param hand Which hand
 * @return Normalized direction vector
 */
Vector3 GetPointingDirection(ControllerHand hand);

/**
 * Check if hand is open (all fingers extended)
 * @param hand Which hand
 * @return true if open hand
 */
bool IsHandOpen(ControllerHand hand);

// =============================================================================
// Visualization Helpers
// =============================================================================

/**
 * Draw hand skeleton visualization
 * @param hand Which hand
 * @param color Color for the skeleton lines
 */
void DrawHandSkeleton(ControllerHand hand, Color color);

/**
 * Draw hand joints as spheres
 * @param hand Which hand
 * @param color Color for the spheres
 */
void DrawHandJoints(ControllerHand hand, Color color);

// =============================================================================
// Utility Functions
// =============================================================================

/**
 * Calculate distance between two joints
 * @param hand Which hand
 * @param joint1 First joint
 * @param joint2 Second joint
 * @return Distance in meters
 */
float GetJointDistance(ControllerHand hand, HandJoint joint1, HandJoint joint2);

/**
 * Check if a joint position is valid
 * @param hand Which hand
 * @param joint Which joint
 * @return true if joint data is valid
 */
bool IsJointValid(ControllerHand hand, HandJoint joint);

#ifdef __cplusplus
}
#endif

#endif // REALITYLIB_HANDS_H
