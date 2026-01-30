/**
 * RealityLib Hand Tracking Implementation
 * 
 * Uses OpenXR XR_EXT_hand_tracking extension for skeletal hand tracking.
 */

#include "realitylib_hands.h"
#include <android/log.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

// EGL headers must come before OpenXR platform headers
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <jni.h>

// OpenXR headers (macros already defined by CMake)
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#define LOG_TAG "RealityLib_Hands"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

// =============================================================================
// External Access to VR State (defined in realitylib_vr.c)
// =============================================================================

// These functions provide access to the core VR state
extern XrInstance GetXrInstance(void);
extern XrSession GetXrSession(void);
extern XrSpace GetXrStageSpace(void);
extern XrTime GetPredictedDisplayTime(void);
extern bool IsVRSessionRunning(void);

// =============================================================================
// Hand Tracking State
// =============================================================================

typedef struct {
    // Extension availability
    bool extensionSupported;
    bool initialized;
    
    // Hand trackers
    XrHandTrackerEXT handTracker[2];  // 0 = left, 1 = right
    
    // Joint data
    XrHandJointLocationEXT jointLocations[2][XR_HAND_JOINT_COUNT_EXT];
    XrHandJointVelocityEXT jointVelocities[2][XR_HAND_JOINT_COUNT_EXT];
    
    // Processed hand data
    VRHand hands[2];
    
    // Function pointers (loaded dynamically)
    PFN_xrCreateHandTrackerEXT xrCreateHandTrackerEXT;
    PFN_xrDestroyHandTrackerEXT xrDestroyHandTrackerEXT;
    PFN_xrLocateHandJointsEXT xrLocateHandJointsEXT;
} HandTrackingState;

static HandTrackingState htState = {0};

// =============================================================================
// Helper Functions
// =============================================================================

static float Vector3Len(Vector3 v) {
    return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
}

static Vector3 Vector3Sub(Vector3 a, Vector3 b) {
    return (Vector3){a.x - b.x, a.y - b.y, a.z - b.z};
}

static Vector3 Vector3Mid(Vector3 a, Vector3 b) {
    return (Vector3){(a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f, (a.z + b.z) * 0.5f};
}

static Vector3 Vector3Norm(Vector3 v) {
    float len = Vector3Len(v);
    if (len > 0.0001f) {
        return (Vector3){v.x / len, v.y / len, v.z / len};
    }
    return (Vector3){0, 0, 0};
}

// Convert XR pose to our types
static Vector3 XrVec3ToVector3(XrVector3f v) {
    return (Vector3){v.x, v.y, v.z};
}

static Quaternion XrQuatToQuaternion(XrQuaternionf q) {
    return (Quaternion){q.x, q.y, q.z, q.w};
}

// =============================================================================
// Gesture Detection Implementation
// =============================================================================

static void DetectGestures(VRHand* hand) {
    if (!hand->isTracking) {
        hand->isPinching = false;
        hand->pinchStrength = 0.0f;
        hand->isFist = false;
        hand->isPointing = false;
        hand->isOpen = false;
        return;
    }
    
    // Get key joints
    VRHandJoint* thumbTip = &hand->joints[HAND_JOINT_THUMB_TIP];
    VRHandJoint* indexTip = &hand->joints[HAND_JOINT_INDEX_TIP];
    VRHandJoint* middleTip = &hand->joints[HAND_JOINT_MIDDLE_TIP];
    VRHandJoint* ringTip = &hand->joints[HAND_JOINT_RING_TIP];
    VRHandJoint* littleTip = &hand->joints[HAND_JOINT_LITTLE_TIP];
    VRHandJoint* palm = &hand->joints[HAND_JOINT_PALM];
    VRHandJoint* wrist = &hand->joints[HAND_JOINT_WRIST];
    
    // Pinch detection - distance between thumb and index tips
    if (thumbTip->isValid && indexTip->isValid) {
        float pinchDist = Vector3Len(Vector3Sub(thumbTip->position, indexTip->position));
        
        // Pinch threshold: ~2cm when touching, ~8cm when open
        const float PINCH_CLOSE = 0.02f;
        const float PINCH_OPEN = 0.08f;
        
        hand->pinchStrength = 1.0f - ((pinchDist - PINCH_CLOSE) / (PINCH_OPEN - PINCH_CLOSE));
        hand->pinchStrength = fmaxf(0.0f, fminf(1.0f, hand->pinchStrength));
        hand->isPinching = hand->pinchStrength > 0.8f;
    } else {
        hand->isPinching = false;
        hand->pinchStrength = 0.0f;
    }
    
    // Fist detection - all fingertips close to palm
    if (palm->isValid && indexTip->isValid && middleTip->isValid && 
        ringTip->isValid && littleTip->isValid) {
        float indexDist = Vector3Len(Vector3Sub(indexTip->position, palm->position));
        float middleDist = Vector3Len(Vector3Sub(middleTip->position, palm->position));
        float ringDist = Vector3Len(Vector3Sub(ringTip->position, palm->position));
        float littleDist = Vector3Len(Vector3Sub(littleTip->position, palm->position));
        
        const float FIST_THRESHOLD = 0.05f;  // 5cm
        hand->isFist = (indexDist < FIST_THRESHOLD && middleDist < FIST_THRESHOLD &&
                        ringDist < FIST_THRESHOLD && littleDist < FIST_THRESHOLD);
    } else {
        hand->isFist = false;
    }
    
    // Pointing detection - index extended, others curled
    if (palm->isValid && indexTip->isValid && middleTip->isValid) {
        float indexDist = Vector3Len(Vector3Sub(indexTip->position, palm->position));
        float middleDist = Vector3Len(Vector3Sub(middleTip->position, palm->position));
        float ringDist = Vector3Len(Vector3Sub(ringTip->position, palm->position));
        float littleDist = Vector3Len(Vector3Sub(littleTip->position, palm->position));
        
        const float EXTENDED_THRESHOLD = 0.10f;  // 10cm
        const float CURLED_THRESHOLD = 0.06f;    // 6cm
        
        hand->isPointing = (indexDist > EXTENDED_THRESHOLD && 
                           middleDist < CURLED_THRESHOLD &&
                           ringDist < CURLED_THRESHOLD);
    } else {
        hand->isPointing = false;
    }
    
    // Open hand detection - all fingers extended
    if (palm->isValid && indexTip->isValid && middleTip->isValid &&
        ringTip->isValid && littleTip->isValid) {
        float indexDist = Vector3Len(Vector3Sub(indexTip->position, palm->position));
        float middleDist = Vector3Len(Vector3Sub(middleTip->position, palm->position));
        float ringDist = Vector3Len(Vector3Sub(ringTip->position, palm->position));
        float littleDist = Vector3Len(Vector3Sub(littleTip->position, palm->position));
        
        const float EXTENDED_THRESHOLD = 0.08f;
        hand->isOpen = (indexDist > EXTENDED_THRESHOLD && middleDist > EXTENDED_THRESHOLD &&
                       ringDist > EXTENDED_THRESHOLD && littleDist > EXTENDED_THRESHOLD);
    } else {
        hand->isOpen = false;
    }
    
    // Palm direction/normal (from orientation)
    if (palm->isValid) {
        hand->palmPosition = palm->position;
        hand->palmOrientation = palm->orientation;
        
        // Calculate palm normal (up direction of palm) from quaternion
        Quaternion q = palm->orientation;
        hand->palmNormal = (Vector3){
            2.0f * (q.x * q.y - q.w * q.z),
            1.0f - 2.0f * (q.x * q.x + q.z * q.z),
            2.0f * (q.y * q.z + q.w * q.x)
        };
        
        // Palm direction (forward, where fingers point)
        hand->palmDirection = (Vector3){
            2.0f * (q.x * q.z + q.w * q.y),
            2.0f * (q.y * q.z - q.w * q.x),
            1.0f - 2.0f * (q.x * q.x + q.y * q.y)
        };
    }
}

// =============================================================================
// Initialization
// =============================================================================

bool InitHandTracking(void) {
    if (htState.initialized) {
        LOGI("Hand tracking already initialized");
        return true;
    }
    
    XrInstance instance = GetXrInstance();
    XrSession session = GetXrSession();
    
    if (instance == XR_NULL_HANDLE || session == XR_NULL_HANDLE) {
        LOGE("Cannot initialize hand tracking: VR not initialized");
        return false;
    }
    
    LOGI("Initializing hand tracking...");
    
    // Check if extension is supported
    uint32_t extensionCount = 0;
    xrEnumerateInstanceExtensionProperties(NULL, 0, &extensionCount, NULL);
    
    XrExtensionProperties* extensions = malloc(extensionCount * sizeof(XrExtensionProperties));
    for (uint32_t i = 0; i < extensionCount; i++) {
        extensions[i].type = XR_TYPE_EXTENSION_PROPERTIES;
        extensions[i].next = NULL;
    }
    xrEnumerateInstanceExtensionProperties(NULL, extensionCount, &extensionCount, extensions);
    
    htState.extensionSupported = false;
    for (uint32_t i = 0; i < extensionCount; i++) {
        if (strcmp(extensions[i].extensionName, XR_EXT_HAND_TRACKING_EXTENSION_NAME) == 0) {
            htState.extensionSupported = true;
            LOGI("XR_EXT_hand_tracking extension found");
            break;
        }
    }
    free(extensions);
    
    if (!htState.extensionSupported) {
        LOGE("XR_EXT_hand_tracking extension not supported on this device");
        return false;
    }
    
    // Get function pointers
    XrResult result;
    result = xrGetInstanceProcAddr(instance, "xrCreateHandTrackerEXT", 
        (PFN_xrVoidFunction*)&htState.xrCreateHandTrackerEXT);
    if (XR_FAILED(result)) {
        LOGE("Failed to get xrCreateHandTrackerEXT");
        return false;
    }
    
    result = xrGetInstanceProcAddr(instance, "xrDestroyHandTrackerEXT",
        (PFN_xrVoidFunction*)&htState.xrDestroyHandTrackerEXT);
    if (XR_FAILED(result)) {
        LOGE("Failed to get xrDestroyHandTrackerEXT");
        return false;
    }
    
    result = xrGetInstanceProcAddr(instance, "xrLocateHandJointsEXT",
        (PFN_xrVoidFunction*)&htState.xrLocateHandJointsEXT);
    if (XR_FAILED(result)) {
        LOGE("Failed to get xrLocateHandJointsEXT");
        return false;
    }
    
    // Create hand trackers
    for (int hand = 0; hand < 2; hand++) {
        XrHandTrackerCreateInfoEXT createInfo = {
            .type = XR_TYPE_HAND_TRACKER_CREATE_INFO_EXT,
            .next = NULL,
            .hand = (hand == 0) ? XR_HAND_LEFT_EXT : XR_HAND_RIGHT_EXT,
            .handJointSet = XR_HAND_JOINT_SET_DEFAULT_EXT
        };
        
        result = htState.xrCreateHandTrackerEXT(session, &createInfo, &htState.handTracker[hand]);
        if (XR_FAILED(result)) {
            LOGE("Failed to create hand tracker for %s hand: %d", 
                 (hand == 0) ? "left" : "right", result);
            
            // Clean up any already created tracker
            if (hand == 1 && htState.handTracker[0] != XR_NULL_HANDLE) {
                htState.xrDestroyHandTrackerEXT(htState.handTracker[0]);
                htState.handTracker[0] = XR_NULL_HANDLE;
            }
            return false;
        }
        
        LOGI("Created hand tracker for %s hand", (hand == 0) ? "left" : "right");
    }
    
    // Initialize joint location structures (zero them out)
    for (int hand = 0; hand < 2; hand++) {
        memset(htState.jointLocations[hand], 0, sizeof(htState.jointLocations[hand]));
        memset(htState.jointVelocities[hand], 0, sizeof(htState.jointVelocities[hand]));
    }
    
    htState.initialized = true;
    LOGI("Hand tracking initialized successfully");
    return true;
}

bool IsHandTrackingAvailable(void) {
    // Quick check without full initialization
    XrInstance instance = GetXrInstance();
    if (instance == XR_NULL_HANDLE) return false;
    
    // If already checked, return cached result
    if (htState.initialized) return true;
    
    // Check extension availability
    uint32_t extensionCount = 0;
    xrEnumerateInstanceExtensionProperties(NULL, 0, &extensionCount, NULL);
    
    XrExtensionProperties* extensions = malloc(extensionCount * sizeof(XrExtensionProperties));
    for (uint32_t i = 0; i < extensionCount; i++) {
        extensions[i].type = XR_TYPE_EXTENSION_PROPERTIES;
        extensions[i].next = NULL;
    }
    xrEnumerateInstanceExtensionProperties(NULL, extensionCount, &extensionCount, extensions);
    
    bool found = false;
    for (uint32_t i = 0; i < extensionCount; i++) {
        if (strcmp(extensions[i].extensionName, XR_EXT_HAND_TRACKING_EXTENSION_NAME) == 0) {
            found = true;
            break;
        }
    }
    free(extensions);
    
    return found;
}

bool IsHandTrackingActive(void) {
    return htState.initialized;
}

void ShutdownHandTracking(void) {
    if (!htState.initialized) return;
    
    LOGI("Shutting down hand tracking...");
    
    for (int hand = 0; hand < 2; hand++) {
        if (htState.handTracker[hand] != XR_NULL_HANDLE && htState.xrDestroyHandTrackerEXT) {
            htState.xrDestroyHandTrackerEXT(htState.handTracker[hand]);
            htState.handTracker[hand] = XR_NULL_HANDLE;
        }
    }
    
    memset(&htState, 0, sizeof(htState));
    LOGI("Hand tracking shut down");
}

// =============================================================================
// Per-Frame Update
// =============================================================================

void UpdateHandTracking(void) {
    if (!htState.initialized || !IsVRSessionRunning()) {
        // Clear tracking state
        for (int i = 0; i < 2; i++) {
            htState.hands[i].isTracking = false;
            htState.hands[i].isActive = false;
        }
        return;
    }
    
    XrSpace stageSpace = GetXrStageSpace();
    XrTime displayTime = GetPredictedDisplayTime();
    
    if (stageSpace == XR_NULL_HANDLE || displayTime == 0) {
        return;
    }
    
    for (int hand = 0; hand < 2; hand++) {
        if (htState.handTracker[hand] == XR_NULL_HANDLE) continue;
        
        // Set up velocity locations
        XrHandJointVelocitiesEXT velocities = {
            .type = XR_TYPE_HAND_JOINT_VELOCITIES_EXT,
            .next = NULL,
            .jointCount = XR_HAND_JOINT_COUNT_EXT,
            .jointVelocities = htState.jointVelocities[hand]
        };
        
        // Set up joint locations  
        XrHandJointLocationsEXT locations = {
            .type = XR_TYPE_HAND_JOINT_LOCATIONS_EXT,
            .next = &velocities,
            .jointCount = XR_HAND_JOINT_COUNT_EXT,
            .jointLocations = htState.jointLocations[hand]
        };
        
        XrHandJointsLocateInfoEXT locateInfo = {
            .type = XR_TYPE_HAND_JOINTS_LOCATE_INFO_EXT,
            .next = NULL,
            .baseSpace = stageSpace,
            .time = displayTime
        };
        
        XrResult result = htState.xrLocateHandJointsEXT(htState.handTracker[hand], 
                                                         &locateInfo, &locations);
        
        htState.hands[hand].isActive = true;
        
        if (XR_SUCCEEDED(result) && locations.isActive) {
            htState.hands[hand].isTracking = true;
            
            // Copy joint data to our format
            for (int j = 0; j < XR_HAND_JOINT_COUNT_EXT && j < HAND_JOINT_COUNT; j++) {
                XrHandJointLocationEXT* src = &htState.jointLocations[hand][j];
                VRHandJoint* dst = &htState.hands[hand].joints[j];
                
                bool posValid = (src->locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) != 0;
                bool oriValid = (src->locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT) != 0;
                
                dst->isValid = posValid && oriValid;
                
                if (posValid) {
                    dst->position = XrVec3ToVector3(src->pose.position);
                }
                if (oriValid) {
                    dst->orientation = XrQuatToQuaternion(src->pose.orientation);
                }
                dst->radius = src->radius;
            }
            
            // Detect gestures
            DetectGestures(&htState.hands[hand]);
            
        } else {
            htState.hands[hand].isTracking = false;
            
            // Clear all joint data
            for (int j = 0; j < HAND_JOINT_COUNT; j++) {
                htState.hands[hand].joints[j].isValid = false;
            }
            
            // Clear gestures
            htState.hands[hand].isPinching = false;
            htState.hands[hand].pinchStrength = 0.0f;
            htState.hands[hand].isFist = false;
            htState.hands[hand].isPointing = false;
            htState.hands[hand].isOpen = false;
        }
    }
}

// =============================================================================
// Hand Data Access
// =============================================================================

VRHand GetHand(ControllerHand hand) {
    if (hand < 0 || hand > 1) {
        VRHand empty = {0};
        return empty;
    }
    return htState.hands[hand];
}

VRHand GetLeftHand(void) {
    return htState.hands[0];
}

VRHand GetRightHand(void) {
    return htState.hands[1];
}

bool IsHandTracked(ControllerHand hand) {
    if (hand < 0 || hand > 1) return false;
    return htState.hands[hand].isTracking;
}

// =============================================================================
// Joint Access
// =============================================================================

Vector3 GetHandJointPosition(ControllerHand hand, HandJoint joint) {
    if (hand < 0 || hand > 1 || joint < 0 || joint >= HAND_JOINT_COUNT) {
        return (Vector3){0, 0, 0};
    }
    return htState.hands[hand].joints[joint].position;
}

Quaternion GetHandJointOrientation(ControllerHand hand, HandJoint joint) {
    if (hand < 0 || hand > 1 || joint < 0 || joint >= HAND_JOINT_COUNT) {
        return (Quaternion){0, 0, 0, 1};
    }
    return htState.hands[hand].joints[joint].orientation;
}

float GetHandJointRadius(ControllerHand hand, HandJoint joint) {
    if (hand < 0 || hand > 1 || joint < 0 || joint >= HAND_JOINT_COUNT) {
        return 0.0f;
    }
    return htState.hands[hand].joints[joint].radius;
}

// =============================================================================
// Convenience Functions
// =============================================================================

Vector3 GetThumbTip(ControllerHand hand) {
    return GetHandJointPosition(hand, HAND_JOINT_THUMB_TIP);
}

Vector3 GetIndexTip(ControllerHand hand) {
    return GetHandJointPosition(hand, HAND_JOINT_INDEX_TIP);
}

Vector3 GetMiddleTip(ControllerHand hand) {
    return GetHandJointPosition(hand, HAND_JOINT_MIDDLE_TIP);
}

Vector3 GetRingTip(ControllerHand hand) {
    return GetHandJointPosition(hand, HAND_JOINT_RING_TIP);
}

Vector3 GetLittleTip(ControllerHand hand) {
    return GetHandJointPosition(hand, HAND_JOINT_LITTLE_TIP);
}

Vector3 GetPalmPosition(ControllerHand hand) {
    return GetHandJointPosition(hand, HAND_JOINT_PALM);
}

Vector3 GetWristPosition(ControllerHand hand) {
    return GetHandJointPosition(hand, HAND_JOINT_WRIST);
}

// =============================================================================
// Gesture Functions
// =============================================================================

bool IsHandPinching(ControllerHand hand) {
    if (hand < 0 || hand > 1) return false;
    return htState.hands[hand].isPinching;
}

float GetPinchStrength(ControllerHand hand) {
    if (hand < 0 || hand > 1) return 0.0f;
    return htState.hands[hand].pinchStrength;
}

Vector3 GetPinchPosition(ControllerHand hand) {
    if (hand < 0 || hand > 1) return (Vector3){0, 0, 0};
    
    Vector3 thumbTip = GetThumbTip(hand);
    Vector3 indexTip = GetIndexTip(hand);
    return Vector3Mid(thumbTip, indexTip);
}

bool IsHandFist(ControllerHand hand) {
    if (hand < 0 || hand > 1) return false;
    return htState.hands[hand].isFist;
}

bool IsHandPointing(ControllerHand hand) {
    if (hand < 0 || hand > 1) return false;
    return htState.hands[hand].isPointing;
}

Vector3 GetPointingDirection(ControllerHand hand) {
    if (hand < 0 || hand > 1) return (Vector3){0, 0, -1};
    
    if (!htState.hands[hand].isTracking) {
        return (Vector3){0, 0, -1};
    }
    
    Vector3 wrist = GetWristPosition(hand);
    Vector3 indexTip = GetIndexTip(hand);
    
    return Vector3Norm(Vector3Sub(indexTip, wrist));
}

bool IsHandOpen(ControllerHand hand) {
    if (hand < 0 || hand > 1) return false;
    return htState.hands[hand].isOpen;
}

// =============================================================================
// Visualization
// =============================================================================

// Joint connections for skeleton drawing
static const int skeletonConnections[][2] = {
    // Thumb
    {HAND_JOINT_WRIST, HAND_JOINT_THUMB_METACARPAL},
    {HAND_JOINT_THUMB_METACARPAL, HAND_JOINT_THUMB_PROXIMAL},
    {HAND_JOINT_THUMB_PROXIMAL, HAND_JOINT_THUMB_DISTAL},
    {HAND_JOINT_THUMB_DISTAL, HAND_JOINT_THUMB_TIP},
    // Index
    {HAND_JOINT_WRIST, HAND_JOINT_INDEX_METACARPAL},
    {HAND_JOINT_INDEX_METACARPAL, HAND_JOINT_INDEX_PROXIMAL},
    {HAND_JOINT_INDEX_PROXIMAL, HAND_JOINT_INDEX_INTERMEDIATE},
    {HAND_JOINT_INDEX_INTERMEDIATE, HAND_JOINT_INDEX_DISTAL},
    {HAND_JOINT_INDEX_DISTAL, HAND_JOINT_INDEX_TIP},
    // Middle
    {HAND_JOINT_WRIST, HAND_JOINT_MIDDLE_METACARPAL},
    {HAND_JOINT_MIDDLE_METACARPAL, HAND_JOINT_MIDDLE_PROXIMAL},
    {HAND_JOINT_MIDDLE_PROXIMAL, HAND_JOINT_MIDDLE_INTERMEDIATE},
    {HAND_JOINT_MIDDLE_INTERMEDIATE, HAND_JOINT_MIDDLE_DISTAL},
    {HAND_JOINT_MIDDLE_DISTAL, HAND_JOINT_MIDDLE_TIP},
    // Ring
    {HAND_JOINT_WRIST, HAND_JOINT_RING_METACARPAL},
    {HAND_JOINT_RING_METACARPAL, HAND_JOINT_RING_PROXIMAL},
    {HAND_JOINT_RING_PROXIMAL, HAND_JOINT_RING_INTERMEDIATE},
    {HAND_JOINT_RING_INTERMEDIATE, HAND_JOINT_RING_DISTAL},
    {HAND_JOINT_RING_DISTAL, HAND_JOINT_RING_TIP},
    // Little
    {HAND_JOINT_WRIST, HAND_JOINT_LITTLE_METACARPAL},
    {HAND_JOINT_LITTLE_METACARPAL, HAND_JOINT_LITTLE_PROXIMAL},
    {HAND_JOINT_LITTLE_PROXIMAL, HAND_JOINT_LITTLE_INTERMEDIATE},
    {HAND_JOINT_LITTLE_INTERMEDIATE, HAND_JOINT_LITTLE_DISTAL},
    {HAND_JOINT_LITTLE_DISTAL, HAND_JOINT_LITTLE_TIP},
    // Palm connections
    {HAND_JOINT_INDEX_METACARPAL, HAND_JOINT_MIDDLE_METACARPAL},
    {HAND_JOINT_MIDDLE_METACARPAL, HAND_JOINT_RING_METACARPAL},
    {HAND_JOINT_RING_METACARPAL, HAND_JOINT_LITTLE_METACARPAL},
};

static const int skeletonConnectionCount = sizeof(skeletonConnections) / sizeof(skeletonConnections[0]);

void DrawHandSkeleton(ControllerHand hand, Color color) {
    if (hand < 0 || hand > 1) return;
    if (!htState.hands[hand].isTracking) return;
    
    VRHand* h = &htState.hands[hand];
    
    for (int i = 0; i < skeletonConnectionCount; i++) {
        int j1 = skeletonConnections[i][0];
        int j2 = skeletonConnections[i][1];
        
        if (h->joints[j1].isValid && h->joints[j2].isValid) {
            DrawVRLine3D(h->joints[j1].position, h->joints[j2].position, color);
        }
    }
}

void DrawHandJoints(ControllerHand hand, Color color) {
    if (hand < 0 || hand > 1) return;
    if (!htState.hands[hand].isTracking) return;
    
    VRHand* h = &htState.hands[hand];
    
    for (int j = 0; j < HAND_JOINT_COUNT; j++) {
        if (h->joints[j].isValid) {
            float radius = h->joints[j].radius;
            if (radius < 0.005f) radius = 0.005f;  // Minimum visible size
            DrawVRSphere(h->joints[j].position, radius, color);
        }
    }
}

// =============================================================================
// Utility Functions
// =============================================================================

float GetJointDistance(ControllerHand hand, HandJoint joint1, HandJoint joint2) {
    if (hand < 0 || hand > 1) return 0.0f;
    if (joint1 < 0 || joint1 >= HAND_JOINT_COUNT) return 0.0f;
    if (joint2 < 0 || joint2 >= HAND_JOINT_COUNT) return 0.0f;
    
    if (!htState.hands[hand].joints[joint1].isValid ||
        !htState.hands[hand].joints[joint2].isValid) {
        return 0.0f;
    }
    
    Vector3 p1 = htState.hands[hand].joints[joint1].position;
    Vector3 p2 = htState.hands[hand].joints[joint2].position;
    
    return Vector3Len(Vector3Sub(p1, p2));
}

bool IsJointValid(ControllerHand hand, HandJoint joint) {
    if (hand < 0 || hand > 1) return false;
    if (joint < 0 || joint >= HAND_JOINT_COUNT) return false;
    return htState.hands[hand].joints[joint].isValid;
}
