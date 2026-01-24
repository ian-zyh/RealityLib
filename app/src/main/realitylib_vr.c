/**
 * RealityLib VR - OpenXR Implementation for Meta Quest
 * 
 * This file implements the VR functionality using OpenXR.
 */

#include "realitylib_vr.h"
#include <android/log.h>
#include <android/looper.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <jni.h>

// XR_USE_GRAPHICS_API_OPENGL_ES and XR_USE_PLATFORM_ANDROID are defined in CMakeLists.txt
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#define LOG_TAG "RealityLib"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

#define MAX_VIEWS 2
#define PI 3.14159265358979323846f

// =============================================================================
// Global State
// =============================================================================

typedef struct {
    // Android
    struct android_app* app;
    JNIEnv* jni;
    jobject activity;
    
    // EGL
    EGLDisplay eglDisplay;
    EGLConfig eglConfig;
    EGLContext eglContext;
    EGLSurface eglSurface;
    
    // OpenXR
    XrInstance instance;
    XrSystemId systemId;
    XrSession session;
    XrSpace stageSpace;
    XrSpace headSpace;
    XrSpace leftHandSpace;
    XrSpace rightHandSpace;
    
    // Swapchain
    XrSwapchain swapchain[MAX_VIEWS];
    uint32_t swapchainLength[MAX_VIEWS];
    XrSwapchainImageOpenGLESKHR* swapchainImages[MAX_VIEWS];
    GLuint framebuffer[MAX_VIEWS];
    GLuint depthBuffer[MAX_VIEWS];
    
    // View config
    XrViewConfigurationView viewConfig[MAX_VIEWS];
    XrView views[MAX_VIEWS];
    uint32_t viewCount;
    
    // Actions (input)
    XrActionSet actionSet;
    XrAction poseAction;
    XrAction triggerAction;
    XrAction gripAction;
    XrAction thumbstickAction;
    XrAction thumbstickClickAction;
    XrAction buttonAAction;
    XrAction buttonBAction;
    XrAction menuAction;
    XrAction hapticAction;
    XrPath leftHandPath;
    XrPath rightHandPath;
    
    // State
    bool sessionRunning;
    bool sessionFocused;
    bool shouldExit;
    XrSessionState sessionState;
    XrTime predictedDisplayTime;
    
    // Input state
    VRController controllers[2];
    VRHeadset headset;
    
    // Rendering
    Color clearColor;
    int currentEye;
    Matrix currentViewMatrix;
    Matrix currentProjectionMatrix;
    
    // Player position offset (for locomotion)
    Vector3 playerPosition;
    float playerYaw;  // Degrees
    
    bool initialized;
} VRState;

static VRState vrState = {0};

// OpenGL resources (forward declared, initialized later)
static GLuint shaderProgram = 0;
static GLint uniformMVP = -1;
static GLint uniformColor = -1;
static GLuint cubeVAO = 0;
static GLuint cubeVBO = 0;
static GLuint cubeEBO = 0;

// =============================================================================
// Draw Command Buffer (for deferred rendering to each eye)
// =============================================================================

typedef enum {
    CMD_DRAW_CUBE,
    CMD_DRAW_LINE,
} DrawCommandType;

typedef struct {
    DrawCommandType type;
    Vector3 position;
    Vector3 size;        // or end position for lines
    Vector3 color;       // normalized 0-1
} DrawCommand;

#define MAX_DRAW_COMMANDS 4096
static DrawCommand drawCommands[MAX_DRAW_COMMANDS];
static int drawCommandCount = 0;

static void ClearDrawCommands(void) {
    drawCommandCount = 0;
}

static void AddDrawCommand(DrawCommand cmd) {
    if (drawCommandCount < MAX_DRAW_COMMANDS) {
        drawCommands[drawCommandCount++] = cmd;
    }
    // Log occasionally for debugging
    static int logCounter = 0;
    if (++logCounter % 1000 == 0) {
        LOGD("Draw commands this frame: %d", drawCommandCount);
    }
}

// =============================================================================
// Forward Declarations
// =============================================================================

static bool InitializeEGL(void);
static void ShutdownEGL(void);
static bool InitializeOpenXR(void);
static void ShutdownOpenXR(void);
static bool CreateSession(void);
static void DestroySession(void);
static bool CreateSwapchains(void);
static void DestroySwapchains(void);
static bool CreateActions(void);
static void PollXREvents(void);
static void UpdateInput(void);
static void BeginFrame(void);
static void EndFrame(void);
static void RenderEye(int eye, uint32_t imageIndex);
static void InitShaders(void);
static void InitCubeGeometry(void);
static void DrawCubeInternal(Vector3 position, Vector3 size, Vector3 color);
static void DrawLineInternal(Vector3 startPos, Vector3 endPos, Vector3 color);

// Helper to check XR results
static bool XrCheck(XrResult result, const char* operation) {
    if (XR_FAILED(result)) {
        char buffer[XR_MAX_RESULT_STRING_SIZE];
        xrResultToString(vrState.instance, result, buffer);
        LOGE("OpenXR Error in %s: %s", operation, buffer);
        return false;
    }
    return true;
}

#define XR_CHECK(result, op) if (!XrCheck(result, op)) return false

// =============================================================================
// Math Helpers
// =============================================================================

Vector3 Vector3Create(float x, float y, float z) {
    return (Vector3){x, y, z};
}

Vector3 Vector3Add(Vector3 v1, Vector3 v2) {
    return (Vector3){v1.x + v2.x, v1.y + v2.y, v1.z + v2.z};
}

Vector3 Vector3Subtract(Vector3 v1, Vector3 v2) {
    return (Vector3){v1.x - v2.x, v1.y - v2.y, v1.z - v2.z};
}

Vector3 Vector3Scale(Vector3 v, float scalar) {
    return (Vector3){v.x * scalar, v.y * scalar, v.z * scalar};
}

float Vector3Length(Vector3 v) {
    return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
}

float Vector3Distance(Vector3 v1, Vector3 v2) {
    Vector3 diff = Vector3Subtract(v1, v2);
    return Vector3Length(diff);
}

Vector3 Vector3Normalize(Vector3 v) {
    float len = Vector3Length(v);
    if (len > 0.0001f) {
        return Vector3Scale(v, 1.0f / len);
    }
    return (Vector3){0, 0, 0};
}

Vector3 QuaternionForward(Quaternion q) {
    return (Vector3){
        2.0f * (q.x * q.z + q.w * q.y),
        2.0f * (q.y * q.z - q.w * q.x),
        1.0f - 2.0f * (q.x * q.x + q.y * q.y)
    };
}

Vector3 QuaternionRight(Quaternion q) {
    return (Vector3){
        1.0f - 2.0f * (q.y * q.y + q.z * q.z),
        2.0f * (q.x * q.y + q.w * q.z),
        2.0f * (q.x * q.z - q.w * q.y)
    };
}

Vector3 QuaternionUp(Quaternion q) {
    return (Vector3){
        2.0f * (q.x * q.y - q.w * q.z),
        1.0f - 2.0f * (q.x * q.x + q.z * q.z),
        2.0f * (q.y * q.z + q.w * q.x)
    };
}

static Matrix MatrixIdentity(void) {
    Matrix m = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };
    return m;
}

static Matrix MatrixMultiply(Matrix left, Matrix right) {
    Matrix result;
    result.m0 = left.m0*right.m0 + left.m1*right.m4 + left.m2*right.m8 + left.m3*right.m12;
    result.m1 = left.m0*right.m1 + left.m1*right.m5 + left.m2*right.m9 + left.m3*right.m13;
    result.m2 = left.m0*right.m2 + left.m1*right.m6 + left.m2*right.m10 + left.m3*right.m14;
    result.m3 = left.m0*right.m3 + left.m1*right.m7 + left.m2*right.m11 + left.m3*right.m15;
    result.m4 = left.m4*right.m0 + left.m5*right.m4 + left.m6*right.m8 + left.m7*right.m12;
    result.m5 = left.m4*right.m1 + left.m5*right.m5 + left.m6*right.m9 + left.m7*right.m13;
    result.m6 = left.m4*right.m2 + left.m5*right.m6 + left.m6*right.m10 + left.m7*right.m14;
    result.m7 = left.m4*right.m3 + left.m5*right.m7 + left.m6*right.m11 + left.m7*right.m15;
    result.m8 = left.m8*right.m0 + left.m9*right.m4 + left.m10*right.m8 + left.m11*right.m12;
    result.m9 = left.m8*right.m1 + left.m9*right.m5 + left.m10*right.m9 + left.m11*right.m13;
    result.m10 = left.m8*right.m2 + left.m9*right.m6 + left.m10*right.m10 + left.m11*right.m14;
    result.m11 = left.m8*right.m3 + left.m9*right.m7 + left.m10*right.m11 + left.m11*right.m15;
    result.m12 = left.m12*right.m0 + left.m13*right.m4 + left.m14*right.m8 + left.m15*right.m12;
    result.m13 = left.m12*right.m1 + left.m13*right.m5 + left.m14*right.m9 + left.m15*right.m13;
    result.m14 = left.m12*right.m2 + left.m13*right.m6 + left.m14*right.m10 + left.m15*right.m14;
    result.m15 = left.m12*right.m3 + left.m13*right.m7 + left.m14*right.m11 + left.m15*right.m15;
    return result;
}

static Matrix MatrixTranslate(float x, float y, float z) {
    Matrix m = MatrixIdentity();
    m.m12 = x;
    m.m13 = y;
    m.m14 = z;
    return m;
}

static Matrix MatrixScale(float x, float y, float z) {
    Matrix m = {
        x, 0.0f, 0.0f, 0.0f,
        0.0f, y, 0.0f, 0.0f,
        0.0f, 0.0f, z, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };
    return m;
}

static Matrix QuaternionToMatrix(Quaternion q) {
    Matrix m = MatrixIdentity();
    
    float xx = q.x * q.x;
    float yy = q.y * q.y;
    float zz = q.z * q.z;
    float xy = q.x * q.y;
    float xz = q.x * q.z;
    float yz = q.y * q.z;
    float wx = q.w * q.x;
    float wy = q.w * q.y;
    float wz = q.w * q.z;
    
    m.m0 = 1.0f - 2.0f * (yy + zz);
    m.m1 = 2.0f * (xy + wz);
    m.m2 = 2.0f * (xz - wy);
    
    m.m4 = 2.0f * (xy - wz);
    m.m5 = 1.0f - 2.0f * (xx + zz);
    m.m6 = 2.0f * (yz + wx);
    
    m.m8 = 2.0f * (xz + wy);
    m.m9 = 2.0f * (yz - wx);
    m.m10 = 1.0f - 2.0f * (xx + yy);
    
    return m;
}

static Matrix CreateProjectionMatrix(XrFovf fov, float nearZ, float farZ) {
    float tanLeft = tanf(fov.angleLeft);
    float tanRight = tanf(fov.angleRight);
    float tanUp = tanf(fov.angleUp);
    float tanDown = tanf(fov.angleDown);
    
    float tanWidth = tanRight - tanLeft;
    float tanHeight = tanUp - tanDown;
    
    Matrix m = {0};
    m.m0 = 2.0f / tanWidth;
    m.m5 = 2.0f / tanHeight;
    m.m8 = (tanRight + tanLeft) / tanWidth;
    m.m9 = (tanUp + tanDown) / tanHeight;
    m.m10 = -(farZ + nearZ) / (farZ - nearZ);
    m.m11 = -1.0f;
    m.m14 = -(2.0f * farZ * nearZ) / (farZ - nearZ);
    
    return m;
}

static Matrix CreateRotationY(float angleRadians) {
    Matrix m = {
        cosf(angleRadians), 0.0f, sinf(angleRadians), 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        -sinf(angleRadians), 0.0f, cosf(angleRadians), 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };
    return m;
}

static Matrix CreateTranslation(float x, float y, float z) {
    Matrix m = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        x, y, z, 1.0f
    };
    return m;
}

static Matrix CreateViewMatrix(XrPosef pose) {
    // First, create the base view matrix from headset pose
    // Invert the pose for view matrix
    Quaternion q = {-pose.orientation.x, -pose.orientation.y, -pose.orientation.z, pose.orientation.w};
    Matrix headsetRot = QuaternionToMatrix(q);
    
    // Get headset position (will be combined with player offset)
    Vector3 headsetPos = {pose.position.x, pose.position.y, pose.position.z};
    
    // Apply player yaw rotation
    float playerYawRad = vrState.playerYaw * PI / 180.0f;
    
    // Rotate headset position around player yaw
    float cosYaw = cosf(playerYawRad);
    float sinYaw = sinf(playerYawRad);
    Vector3 rotatedHeadsetPos = {
        headsetPos.x * cosYaw - headsetPos.z * sinYaw,
        headsetPos.y,
        headsetPos.x * sinYaw + headsetPos.z * cosYaw
    };
    
    // Combine with player position offset
    Vector3 finalPos = {
        -(rotatedHeadsetPos.x + vrState.playerPosition.x),
        -(rotatedHeadsetPos.y + vrState.playerPosition.y),
        -(rotatedHeadsetPos.z + vrState.playerPosition.z)
    };
    
    // Create player yaw rotation matrix (inverted for view)
    Matrix playerYawMatrix = CreateRotationY(-playerYawRad);
    
    // Combine rotations: headset rotation * player yaw rotation
    Matrix combinedRot = MatrixMultiply(headsetRot, playerYawMatrix);
    
    // Transform final position by combined rotation
    Vector3 transformedPos = {
        combinedRot.m0 * finalPos.x + combinedRot.m4 * finalPos.y + combinedRot.m8 * finalPos.z,
        combinedRot.m1 * finalPos.x + combinedRot.m5 * finalPos.y + combinedRot.m9 * finalPos.z,
        combinedRot.m2 * finalPos.x + combinedRot.m6 * finalPos.y + combinedRot.m10 * finalPos.z
    };
    
    combinedRot.m12 = transformedPos.x;
    combinedRot.m13 = transformedPos.y;
    combinedRot.m14 = transformedPos.z;
    
    return combinedRot;
}

// =============================================================================
// EGL Initialization
// =============================================================================

static bool InitializeEGL(void) {
    LOGI("Initializing EGL...");
    
    vrState.eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (vrState.eglDisplay == EGL_NO_DISPLAY) {
        LOGE("Failed to get EGL display");
        return false;
    }
    
    EGLint major, minor;
    if (!eglInitialize(vrState.eglDisplay, &major, &minor)) {
        LOGE("Failed to initialize EGL");
        return false;
    }
    LOGI("EGL initialized: %d.%d", major, minor);
    
    const EGLint configAttribs[] = {
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 24,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT_KHR,
        EGL_NONE
    };
    
    EGLint numConfigs;
    if (!eglChooseConfig(vrState.eglDisplay, configAttribs, &vrState.eglConfig, 1, &numConfigs) || numConfigs == 0) {
        LOGE("Failed to choose EGL config");
        return false;
    }
    
    const EGLint contextAttribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 3,
        EGL_NONE
    };
    
    vrState.eglContext = eglCreateContext(vrState.eglDisplay, vrState.eglConfig, EGL_NO_CONTEXT, contextAttribs);
    if (vrState.eglContext == EGL_NO_CONTEXT) {
        LOGE("Failed to create EGL context");
        return false;
    }
    
    // Create a dummy surface (required for some operations)
    const EGLint surfaceAttribs[] = {
        EGL_WIDTH, 16,
        EGL_HEIGHT, 16,
        EGL_NONE
    };
    vrState.eglSurface = eglCreatePbufferSurface(vrState.eglDisplay, vrState.eglConfig, surfaceAttribs);
    
    if (!eglMakeCurrent(vrState.eglDisplay, vrState.eglSurface, vrState.eglSurface, vrState.eglContext)) {
        LOGE("Failed to make EGL context current");
        return false;
    }
    
    LOGI("EGL initialized successfully");
    return true;
}

static void ShutdownEGL(void) {
    if (vrState.eglDisplay != EGL_NO_DISPLAY) {
        eglMakeCurrent(vrState.eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (vrState.eglContext != EGL_NO_CONTEXT) {
            eglDestroyContext(vrState.eglDisplay, vrState.eglContext);
        }
        if (vrState.eglSurface != EGL_NO_SURFACE) {
            eglDestroySurface(vrState.eglDisplay, vrState.eglSurface);
        }
        eglTerminate(vrState.eglDisplay);
    }
    vrState.eglDisplay = EGL_NO_DISPLAY;
    vrState.eglContext = EGL_NO_CONTEXT;
    vrState.eglSurface = EGL_NO_SURFACE;
}

// =============================================================================
// OpenXR Initialization
// =============================================================================

static bool InitializeOpenXR(void) {
    LOGI("Initializing OpenXR...");
    
    // Initialize loader
    PFN_xrInitializeLoaderKHR xrInitializeLoader = NULL;
    xrGetInstanceProcAddr(XR_NULL_HANDLE, "xrInitializeLoaderKHR", (PFN_xrVoidFunction*)&xrInitializeLoader);
    
    if (xrInitializeLoader) {
        XrLoaderInitInfoAndroidKHR loaderInfo = {
            .type = XR_TYPE_LOADER_INIT_INFO_ANDROID_KHR,
            .next = NULL,
            .applicationVM = vrState.app->activity->vm,
            .applicationContext = vrState.app->activity->clazz
        };
        xrInitializeLoader((XrLoaderInitInfoBaseHeaderKHR*)&loaderInfo);
    }
    
    // Get required extensions
    const char* extensions[] = {
        XR_KHR_OPENGL_ES_ENABLE_EXTENSION_NAME,
        XR_KHR_ANDROID_CREATE_INSTANCE_EXTENSION_NAME,
    };
    
    // Create instance
    XrInstanceCreateInfoAndroidKHR androidInfo = {
        .type = XR_TYPE_INSTANCE_CREATE_INFO_ANDROID_KHR,
        .next = NULL,
        .applicationVM = vrState.app->activity->vm,
        .applicationActivity = vrState.app->activity->clazz
    };
    
    XrInstanceCreateInfo createInfo = {
        .type = XR_TYPE_INSTANCE_CREATE_INFO,
        .next = &androidInfo,
        .createFlags = 0,
        .applicationInfo = {
            .applicationName = "RealityLib",
            .applicationVersion = 1,
            .engineName = "RealityLib",
            .engineVersion = 1,
            .apiVersion = XR_API_VERSION_1_0
        },
        .enabledApiLayerCount = 0,
        .enabledApiLayerNames = NULL,
        .enabledExtensionCount = sizeof(extensions) / sizeof(extensions[0]),
        .enabledExtensionNames = extensions
    };
    
    XrResult result = xrCreateInstance(&createInfo, &vrState.instance);
    if (XR_FAILED(result)) {
        LOGE("Failed to create OpenXR instance: %d", result);
        return false;
    }
    LOGI("OpenXR instance created");
    
    // Get system
    XrSystemGetInfo systemInfo = {
        .type = XR_TYPE_SYSTEM_GET_INFO,
        .next = NULL,
        .formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY
    };
    
    XR_CHECK(xrGetSystem(vrState.instance, &systemInfo, &vrState.systemId), "xrGetSystem");
    LOGI("OpenXR system obtained: %llu", (unsigned long long)vrState.systemId);
    
    // Get view configuration
    uint32_t viewConfigTypeCount = 0;
    xrEnumerateViewConfigurations(vrState.instance, vrState.systemId, 0, &viewConfigTypeCount, NULL);
    
    XrViewConfigurationType* viewConfigTypes = malloc(viewConfigTypeCount * sizeof(XrViewConfigurationType));
    xrEnumerateViewConfigurations(vrState.instance, vrState.systemId, viewConfigTypeCount, &viewConfigTypeCount, viewConfigTypes);
    
    bool foundStereo = false;
    for (uint32_t i = 0; i < viewConfigTypeCount; i++) {
        if (viewConfigTypes[i] == XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO) {
            foundStereo = true;
            break;
        }
    }
    free(viewConfigTypes);
    
    if (!foundStereo) {
        LOGE("Stereo view configuration not supported");
        return false;
    }
    
    // Get view configuration views
    uint32_t viewCount = 0;
    xrEnumerateViewConfigurationViews(vrState.instance, vrState.systemId, 
        XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0, &viewCount, NULL);
    
    for (uint32_t i = 0; i < viewCount && i < MAX_VIEWS; i++) {
        vrState.viewConfig[i].type = XR_TYPE_VIEW_CONFIGURATION_VIEW;
        vrState.viewConfig[i].next = NULL;
    }
    
    xrEnumerateViewConfigurationViews(vrState.instance, vrState.systemId,
        XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, viewCount, &viewCount, vrState.viewConfig);
    
    vrState.viewCount = viewCount < MAX_VIEWS ? viewCount : MAX_VIEWS;
    LOGI("View count: %d, resolution: %dx%d", vrState.viewCount, 
        vrState.viewConfig[0].recommendedImageRectWidth,
        vrState.viewConfig[0].recommendedImageRectHeight);
    
    LOGI("OpenXR initialized successfully");
    return true;
}

static void ShutdownOpenXR(void) {
    if (vrState.instance != XR_NULL_HANDLE) {
        xrDestroyInstance(vrState.instance);
        vrState.instance = XR_NULL_HANDLE;
    }
}

// =============================================================================
// Session Management
// =============================================================================

static bool CreateSession(void) {
    LOGI("Creating OpenXR session...");
    
    // Get graphics requirements
    PFN_xrGetOpenGLESGraphicsRequirementsKHR xrGetOpenGLESGraphicsRequirementsKHR = NULL;
    xrGetInstanceProcAddr(vrState.instance, "xrGetOpenGLESGraphicsRequirementsKHR", 
        (PFN_xrVoidFunction*)&xrGetOpenGLESGraphicsRequirementsKHR);
    
    XrGraphicsRequirementsOpenGLESKHR graphicsReqs = {
        .type = XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_ES_KHR,
        .next = NULL
    };
    xrGetOpenGLESGraphicsRequirementsKHR(vrState.instance, vrState.systemId, &graphicsReqs);
    
    // Create session with graphics binding
    XrGraphicsBindingOpenGLESAndroidKHR graphicsBinding = {
        .type = XR_TYPE_GRAPHICS_BINDING_OPENGL_ES_ANDROID_KHR,
        .next = NULL,
        .display = vrState.eglDisplay,
        .config = vrState.eglConfig,
        .context = vrState.eglContext
    };
    
    XrSessionCreateInfo sessionInfo = {
        .type = XR_TYPE_SESSION_CREATE_INFO,
        .next = &graphicsBinding,
        .createFlags = 0,
        .systemId = vrState.systemId
    };
    
    XR_CHECK(xrCreateSession(vrState.instance, &sessionInfo, &vrState.session), "xrCreateSession");
    LOGI("OpenXR session created");
    
    // Create reference spaces
    XrReferenceSpaceCreateInfo spaceInfo = {
        .type = XR_TYPE_REFERENCE_SPACE_CREATE_INFO,
        .next = NULL,
        .referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE,
        .poseInReferenceSpace = {
            .orientation = {0, 0, 0, 1},
            .position = {0, 0, 0}
        }
    };
    
    XR_CHECK(xrCreateReferenceSpace(vrState.session, &spaceInfo, &vrState.stageSpace), "xrCreateReferenceSpace (stage)");
    
    spaceInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
    XR_CHECK(xrCreateReferenceSpace(vrState.session, &spaceInfo, &vrState.headSpace), "xrCreateReferenceSpace (head)");
    
    LOGI("Reference spaces created");
    
    // Create actions
    if (!CreateActions()) {
        return false;
    }
    
    // Create swapchains
    if (!CreateSwapchains()) {
        return false;
    }
    
    return true;
}

static void DestroySession(void) {
    DestroySwapchains();
    
    if (vrState.leftHandSpace != XR_NULL_HANDLE) {
        xrDestroySpace(vrState.leftHandSpace);
        vrState.leftHandSpace = XR_NULL_HANDLE;
    }
    if (vrState.rightHandSpace != XR_NULL_HANDLE) {
        xrDestroySpace(vrState.rightHandSpace);
        vrState.rightHandSpace = XR_NULL_HANDLE;
    }
    if (vrState.stageSpace != XR_NULL_HANDLE) {
        xrDestroySpace(vrState.stageSpace);
        vrState.stageSpace = XR_NULL_HANDLE;
    }
    if (vrState.headSpace != XR_NULL_HANDLE) {
        xrDestroySpace(vrState.headSpace);
        vrState.headSpace = XR_NULL_HANDLE;
    }
    if (vrState.actionSet != XR_NULL_HANDLE) {
        xrDestroyActionSet(vrState.actionSet);
        vrState.actionSet = XR_NULL_HANDLE;
    }
    if (vrState.session != XR_NULL_HANDLE) {
        xrDestroySession(vrState.session);
        vrState.session = XR_NULL_HANDLE;
    }
}

// =============================================================================
// Swapchain Management
// =============================================================================

static bool CreateSwapchains(void) {
    LOGI("Creating swapchains...");
    
    for (uint32_t i = 0; i < vrState.viewCount; i++) {
        XrSwapchainCreateInfo swapchainInfo = {
            .type = XR_TYPE_SWAPCHAIN_CREATE_INFO,
            .next = NULL,
            .createFlags = 0,
            .usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT,
            .format = GL_SRGB8_ALPHA8,
            .sampleCount = 1,
            .width = vrState.viewConfig[i].recommendedImageRectWidth,
            .height = vrState.viewConfig[i].recommendedImageRectHeight,
            .faceCount = 1,
            .arraySize = 1,
            .mipCount = 1
        };
        
        XR_CHECK(xrCreateSwapchain(vrState.session, &swapchainInfo, &vrState.swapchain[i]), "xrCreateSwapchain");
        
        // Get swapchain images
        xrEnumerateSwapchainImages(vrState.swapchain[i], 0, &vrState.swapchainLength[i], NULL);
        
        vrState.swapchainImages[i] = malloc(vrState.swapchainLength[i] * sizeof(XrSwapchainImageOpenGLESKHR));
        for (uint32_t j = 0; j < vrState.swapchainLength[i]; j++) {
            vrState.swapchainImages[i][j].type = XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR;
            vrState.swapchainImages[i][j].next = NULL;
        }
        
        xrEnumerateSwapchainImages(vrState.swapchain[i], vrState.swapchainLength[i], 
            &vrState.swapchainLength[i], (XrSwapchainImageBaseHeader*)vrState.swapchainImages[i]);
        
        // Create framebuffer and depth buffer
        glGenFramebuffers(1, &vrState.framebuffer[i]);
        glGenRenderbuffers(1, &vrState.depthBuffer[i]);
        
        glBindRenderbuffer(GL_RENDERBUFFER, vrState.depthBuffer[i]);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8,
            vrState.viewConfig[i].recommendedImageRectWidth,
            vrState.viewConfig[i].recommendedImageRectHeight);
        
        LOGI("Swapchain %d created: %d images, %dx%d", i, vrState.swapchainLength[i],
            vrState.viewConfig[i].recommendedImageRectWidth,
            vrState.viewConfig[i].recommendedImageRectHeight);
    }
    
    return true;
}

static void DestroySwapchains(void) {
    for (uint32_t i = 0; i < vrState.viewCount; i++) {
        if (vrState.framebuffer[i]) {
            glDeleteFramebuffers(1, &vrState.framebuffer[i]);
            vrState.framebuffer[i] = 0;
        }
        if (vrState.depthBuffer[i]) {
            glDeleteRenderbuffers(1, &vrState.depthBuffer[i]);
            vrState.depthBuffer[i] = 0;
        }
        if (vrState.swapchainImages[i]) {
            free(vrState.swapchainImages[i]);
            vrState.swapchainImages[i] = NULL;
        }
        if (vrState.swapchain[i] != XR_NULL_HANDLE) {
            xrDestroySwapchain(vrState.swapchain[i]);
            vrState.swapchain[i] = XR_NULL_HANDLE;
        }
    }
}

// =============================================================================
// Input Actions
// =============================================================================

static bool CreateActions(void) {
    LOGI("Creating input actions...");
    
    // Create action set
    XrActionSetCreateInfo actionSetInfo = {
        .type = XR_TYPE_ACTION_SET_CREATE_INFO,
        .next = NULL,
        .priority = 0
    };
    strcpy(actionSetInfo.actionSetName, "gameplay");
    strcpy(actionSetInfo.localizedActionSetName, "Gameplay");
    
    XR_CHECK(xrCreateActionSet(vrState.instance, &actionSetInfo, &vrState.actionSet), "xrCreateActionSet");
    
    // Get hand paths
    xrStringToPath(vrState.instance, "/user/hand/left", &vrState.leftHandPath);
    xrStringToPath(vrState.instance, "/user/hand/right", &vrState.rightHandPath);
    
    XrPath handPaths[] = {vrState.leftHandPath, vrState.rightHandPath};
    
    // Create pose action
    XrActionCreateInfo actionInfo = {
        .type = XR_TYPE_ACTION_CREATE_INFO,
        .next = NULL,
        .actionType = XR_ACTION_TYPE_POSE_INPUT,
        .countSubactionPaths = 2,
        .subactionPaths = handPaths
    };
    strcpy(actionInfo.actionName, "hand_pose");
    strcpy(actionInfo.localizedActionName, "Hand Pose");
    XR_CHECK(xrCreateAction(vrState.actionSet, &actionInfo, &vrState.poseAction), "xrCreateAction (pose)");
    
    // Create trigger action
    actionInfo.actionType = XR_ACTION_TYPE_FLOAT_INPUT;
    strcpy(actionInfo.actionName, "trigger");
    strcpy(actionInfo.localizedActionName, "Trigger");
    XR_CHECK(xrCreateAction(vrState.actionSet, &actionInfo, &vrState.triggerAction), "xrCreateAction (trigger)");
    
    // Create grip action
    strcpy(actionInfo.actionName, "grip");
    strcpy(actionInfo.localizedActionName, "Grip");
    XR_CHECK(xrCreateAction(vrState.actionSet, &actionInfo, &vrState.gripAction), "xrCreateAction (grip)");
    
    // Create thumbstick action
    actionInfo.actionType = XR_ACTION_TYPE_VECTOR2F_INPUT;
    strcpy(actionInfo.actionName, "thumbstick");
    strcpy(actionInfo.localizedActionName, "Thumbstick");
    XR_CHECK(xrCreateAction(vrState.actionSet, &actionInfo, &vrState.thumbstickAction), "xrCreateAction (thumbstick)");
    
    // Create button actions
    actionInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
    strcpy(actionInfo.actionName, "thumbstick_click");
    strcpy(actionInfo.localizedActionName, "Thumbstick Click");
    XR_CHECK(xrCreateAction(vrState.actionSet, &actionInfo, &vrState.thumbstickClickAction), "xrCreateAction (thumbstick_click)");
    
    strcpy(actionInfo.actionName, "button_a");
    strcpy(actionInfo.localizedActionName, "Button A/X");
    XR_CHECK(xrCreateAction(vrState.actionSet, &actionInfo, &vrState.buttonAAction), "xrCreateAction (button_a)");
    
    strcpy(actionInfo.actionName, "button_b");
    strcpy(actionInfo.localizedActionName, "Button B/Y");
    XR_CHECK(xrCreateAction(vrState.actionSet, &actionInfo, &vrState.buttonBAction), "xrCreateAction (button_b)");
    
    actionInfo.countSubactionPaths = 0;
    actionInfo.subactionPaths = NULL;
    strcpy(actionInfo.actionName, "menu");
    strcpy(actionInfo.localizedActionName, "Menu");
    XR_CHECK(xrCreateAction(vrState.actionSet, &actionInfo, &vrState.menuAction), "xrCreateAction (menu)");
    
    // Create haptic action
    actionInfo.actionType = XR_ACTION_TYPE_VIBRATION_OUTPUT;
    actionInfo.countSubactionPaths = 2;
    actionInfo.subactionPaths = handPaths;
    strcpy(actionInfo.actionName, "haptic");
    strcpy(actionInfo.localizedActionName, "Haptic");
    XR_CHECK(xrCreateAction(vrState.actionSet, &actionInfo, &vrState.hapticAction), "xrCreateAction (haptic)");
    
    // Create action spaces for hand tracking
    XrActionSpaceCreateInfo spaceInfo = {
        .type = XR_TYPE_ACTION_SPACE_CREATE_INFO,
        .next = NULL,
        .action = vrState.poseAction,
        .poseInActionSpace = {
            .orientation = {0, 0, 0, 1},
            .position = {0, 0, 0}
        }
    };
    
    spaceInfo.subactionPath = vrState.leftHandPath;
    XR_CHECK(xrCreateActionSpace(vrState.session, &spaceInfo, &vrState.leftHandSpace), "xrCreateActionSpace (left)");
    
    spaceInfo.subactionPath = vrState.rightHandPath;
    XR_CHECK(xrCreateActionSpace(vrState.session, &spaceInfo, &vrState.rightHandSpace), "xrCreateActionSpace (right)");
    
    // Suggest bindings for Oculus Touch controllers
    XrPath interactionProfilePath;
    xrStringToPath(vrState.instance, "/interaction_profiles/oculus/touch_controller", &interactionProfilePath);
    
    XrPath bindingPaths[20];
    xrStringToPath(vrState.instance, "/user/hand/left/input/grip/pose", &bindingPaths[0]);
    xrStringToPath(vrState.instance, "/user/hand/right/input/grip/pose", &bindingPaths[1]);
    xrStringToPath(vrState.instance, "/user/hand/left/input/trigger/value", &bindingPaths[2]);
    xrStringToPath(vrState.instance, "/user/hand/right/input/trigger/value", &bindingPaths[3]);
    xrStringToPath(vrState.instance, "/user/hand/left/input/squeeze/value", &bindingPaths[4]);
    xrStringToPath(vrState.instance, "/user/hand/right/input/squeeze/value", &bindingPaths[5]);
    xrStringToPath(vrState.instance, "/user/hand/left/input/thumbstick", &bindingPaths[6]);
    xrStringToPath(vrState.instance, "/user/hand/right/input/thumbstick", &bindingPaths[7]);
    xrStringToPath(vrState.instance, "/user/hand/left/input/thumbstick/click", &bindingPaths[8]);
    xrStringToPath(vrState.instance, "/user/hand/right/input/thumbstick/click", &bindingPaths[9]);
    xrStringToPath(vrState.instance, "/user/hand/left/input/x/click", &bindingPaths[10]);
    xrStringToPath(vrState.instance, "/user/hand/right/input/a/click", &bindingPaths[11]);
    xrStringToPath(vrState.instance, "/user/hand/left/input/y/click", &bindingPaths[12]);
    xrStringToPath(vrState.instance, "/user/hand/right/input/b/click", &bindingPaths[13]);
    xrStringToPath(vrState.instance, "/user/hand/left/input/menu/click", &bindingPaths[14]);
    xrStringToPath(vrState.instance, "/user/hand/left/output/haptic", &bindingPaths[15]);
    xrStringToPath(vrState.instance, "/user/hand/right/output/haptic", &bindingPaths[16]);
    
    XrActionSuggestedBinding bindings[] = {
        {vrState.poseAction, bindingPaths[0]},
        {vrState.poseAction, bindingPaths[1]},
        {vrState.triggerAction, bindingPaths[2]},
        {vrState.triggerAction, bindingPaths[3]},
        {vrState.gripAction, bindingPaths[4]},
        {vrState.gripAction, bindingPaths[5]},
        {vrState.thumbstickAction, bindingPaths[6]},
        {vrState.thumbstickAction, bindingPaths[7]},
        {vrState.thumbstickClickAction, bindingPaths[8]},
        {vrState.thumbstickClickAction, bindingPaths[9]},
        {vrState.buttonAAction, bindingPaths[10]},
        {vrState.buttonAAction, bindingPaths[11]},
        {vrState.buttonBAction, bindingPaths[12]},
        {vrState.buttonBAction, bindingPaths[13]},
        {vrState.menuAction, bindingPaths[14]},
        {vrState.hapticAction, bindingPaths[15]},
        {vrState.hapticAction, bindingPaths[16]},
    };
    
    XrInteractionProfileSuggestedBinding suggestedBindings = {
        .type = XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING,
        .next = NULL,
        .interactionProfile = interactionProfilePath,
        .countSuggestedBindings = sizeof(bindings) / sizeof(bindings[0]),
        .suggestedBindings = bindings
    };
    
    xrSuggestInteractionProfileBindings(vrState.instance, &suggestedBindings);
    
    // Attach action set to session
    XrSessionActionSetsAttachInfo attachInfo = {
        .type = XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO,
        .next = NULL,
        .countActionSets = 1,
        .actionSets = &vrState.actionSet
    };
    
    XR_CHECK(xrAttachSessionActionSets(vrState.session, &attachInfo), "xrAttachSessionActionSets");
    
    LOGI("Input actions created successfully");
    return true;
}

// =============================================================================
// Event Handling
// =============================================================================

static void PollXREvents(void) {
    XrEventDataBuffer eventBuffer;
    
    while (true) {
        eventBuffer.type = XR_TYPE_EVENT_DATA_BUFFER;
        eventBuffer.next = NULL;
        
        XrResult result = xrPollEvent(vrState.instance, &eventBuffer);
        if (result == XR_EVENT_UNAVAILABLE) {
            break;
        }
        
        if (XR_FAILED(result)) {
            LOGE("Error polling events");
            break;
        }
        
        switch (eventBuffer.type) {
            case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED: {
                XrEventDataSessionStateChanged* stateEvent = (XrEventDataSessionStateChanged*)&eventBuffer;
                vrState.sessionState = stateEvent->state;
                
                LOGI("Session state changed: %d", stateEvent->state);
                
                switch (stateEvent->state) {
                    case XR_SESSION_STATE_READY: {
                        XrSessionBeginInfo beginInfo = {
                            .type = XR_TYPE_SESSION_BEGIN_INFO,
                            .next = NULL,
                            .primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO
                        };
                        xrBeginSession(vrState.session, &beginInfo);
                        vrState.sessionRunning = true;
                        LOGI("Session started");
                        break;
                    }
                    case XR_SESSION_STATE_STOPPING:
                        xrEndSession(vrState.session);
                        vrState.sessionRunning = false;
                        LOGI("Session stopped");
                        break;
                    case XR_SESSION_STATE_EXITING:
                    case XR_SESSION_STATE_LOSS_PENDING:
                        vrState.shouldExit = true;
                        break;
                    case XR_SESSION_STATE_FOCUSED:
                        vrState.sessionFocused = true;
                        break;
                    case XR_SESSION_STATE_VISIBLE:
                        vrState.sessionFocused = false;
                        break;
                    default:
                        break;
                }
                break;
            }
            case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING:
                vrState.shouldExit = true;
                break;
            default:
                break;
        }
    }
}

static void UpdateInput(void) {
    if (!vrState.sessionRunning) return;
    
    // Sync actions
    XrActiveActionSet activeActionSet = {
        .actionSet = vrState.actionSet,
        .subactionPath = XR_NULL_PATH
    };
    
    XrActionsSyncInfo syncInfo = {
        .type = XR_TYPE_ACTIONS_SYNC_INFO,
        .next = NULL,
        .countActiveActionSets = 1,
        .activeActionSets = &activeActionSet
    };
    
    xrSyncActions(vrState.session, &syncInfo);
    
    // Get hand poses
    XrPath hands[] = {vrState.leftHandPath, vrState.rightHandPath};
    XrSpace handSpaces[] = {vrState.leftHandSpace, vrState.rightHandSpace};
    
    for (int i = 0; i < 2; i++) {
        // Get pose
        XrSpaceLocation location = {
            .type = XR_TYPE_SPACE_LOCATION,
            .next = NULL
        };
        
        XrSpaceVelocity velocity = {
            .type = XR_TYPE_SPACE_VELOCITY,
            .next = NULL
        };
        location.next = &velocity;
        
        xrLocateSpace(handSpaces[i], vrState.stageSpace, vrState.predictedDisplayTime, &location);
        
        if (location.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) {
            vrState.controllers[i].position = (Vector3){
                location.pose.position.x,
                location.pose.position.y,
                location.pose.position.z
            };
            vrState.controllers[i].orientation = (Quaternion){
                location.pose.orientation.x,
                location.pose.orientation.y,
                location.pose.orientation.z,
                location.pose.orientation.w
            };
            vrState.controllers[i].isTracking = true;
        } else {
            vrState.controllers[i].isTracking = false;
        }
        
        if (velocity.velocityFlags & XR_SPACE_VELOCITY_LINEAR_VALID_BIT) {
            vrState.controllers[i].velocity = (Vector3){
                velocity.linearVelocity.x,
                velocity.linearVelocity.y,
                velocity.linearVelocity.z
            };
        }
        
        if (velocity.velocityFlags & XR_SPACE_VELOCITY_ANGULAR_VALID_BIT) {
            vrState.controllers[i].angularVelocity = (Vector3){
                velocity.angularVelocity.x,
                velocity.angularVelocity.y,
                velocity.angularVelocity.z
            };
        }
        
        // Get trigger
        XrActionStateGetInfo getInfo = {
            .type = XR_TYPE_ACTION_STATE_GET_INFO,
            .next = NULL,
            .action = vrState.triggerAction,
            .subactionPath = hands[i]
        };
        
        XrActionStateFloat floatState = {.type = XR_TYPE_ACTION_STATE_FLOAT};
        xrGetActionStateFloat(vrState.session, &getInfo, &floatState);
        vrState.controllers[i].trigger = floatState.currentState;
        
        // Get grip
        getInfo.action = vrState.gripAction;
        xrGetActionStateFloat(vrState.session, &getInfo, &floatState);
        vrState.controllers[i].grip = floatState.currentState;
        
        // Get thumbstick
        getInfo.action = vrState.thumbstickAction;
        XrActionStateVector2f vec2State = {.type = XR_TYPE_ACTION_STATE_VECTOR2F};
        xrGetActionStateVector2f(vrState.session, &getInfo, &vec2State);
        vrState.controllers[i].thumbstickX = vec2State.currentState.x;
        vrState.controllers[i].thumbstickY = vec2State.currentState.y;
        
        // Get buttons
        getInfo.action = vrState.thumbstickClickAction;
        XrActionStateBoolean boolState = {.type = XR_TYPE_ACTION_STATE_BOOLEAN};
        xrGetActionStateBoolean(vrState.session, &getInfo, &boolState);
        vrState.controllers[i].thumbstickClick = boolState.currentState;
        
        getInfo.action = vrState.buttonAAction;
        xrGetActionStateBoolean(vrState.session, &getInfo, &boolState);
        vrState.controllers[i].buttonA = boolState.currentState;
        
        getInfo.action = vrState.buttonBAction;
        xrGetActionStateBoolean(vrState.session, &getInfo, &boolState);
        vrState.controllers[i].buttonB = boolState.currentState;
    }
    
    // Get menu button (left hand only)
    XrActionStateGetInfo menuGetInfo = {
        .type = XR_TYPE_ACTION_STATE_GET_INFO,
        .action = vrState.menuAction,
        .subactionPath = XR_NULL_PATH
    };
    XrActionStateBoolean menuState = {.type = XR_TYPE_ACTION_STATE_BOOLEAN};
    xrGetActionStateBoolean(vrState.session, &menuGetInfo, &menuState);
    vrState.controllers[0].menuButton = menuState.currentState;
    
    // Update headset
    XrSpaceLocation headLocation = {
        .type = XR_TYPE_SPACE_LOCATION,
        .next = NULL
    };
    xrLocateSpace(vrState.headSpace, vrState.stageSpace, vrState.predictedDisplayTime, &headLocation);
    
    vrState.headset.position = (Vector3){
        headLocation.pose.position.x,
        headLocation.pose.position.y,
        headLocation.pose.position.z
    };
    vrState.headset.orientation = (Quaternion){
        headLocation.pose.orientation.x,
        headLocation.pose.orientation.y,
        headLocation.pose.orientation.z,
        headLocation.pose.orientation.w
    };
}

// =============================================================================
// Public API Implementation
// =============================================================================

bool InitApp(struct android_app* app) {
    LOGI("InitApp starting...");
    
    memset(&vrState, 0, sizeof(vrState));
    vrState.app = app;
    vrState.clearColor = (Color){30, 30, 50, 255};  // Dark blue default
    
    // Initialize EGL
    if (!InitializeEGL()) {
        LOGE("Failed to initialize EGL");
        return false;
    }
    
    // Initialize OpenXR
    if (!InitializeOpenXR()) {
        LOGE("Failed to initialize OpenXR");
        ShutdownEGL();
        return false;
    }
    
    // Create session
    if (!CreateSession()) {
        LOGE("Failed to create session");
        ShutdownOpenXR();
        ShutdownEGL();
        return false;
    }
    
    vrState.initialized = true;
    LOGI("InitApp completed successfully");
    return true;
}

void CloseApp(struct android_app* app) {
    LOGI("CloseApp starting...");
    
    DestroySession();
    ShutdownOpenXR();
    ShutdownEGL();
    
    vrState.initialized = false;
    LOGI("CloseApp completed");
}

bool AppShouldClose(struct android_app* app) {
    // Poll Android events using ALooper_pollOnce (ALooper_pollAll is deprecated)
    int events;
    struct android_poll_source* source;
    
    while (ALooper_pollOnce(0, NULL, &events, (void**)&source) >= 0) {
        if (source != NULL) {
            source->process(app, source);
        }
        if (app->destroyRequested) {
            vrState.shouldExit = true;
        }
    }
    
    // Poll XR events
    PollXREvents();
    
    return vrState.shouldExit;
}

void BeginVRMode(void) {
    if (!vrState.sessionRunning) return;
    
    // Clear the draw command buffer for this frame
    ClearDrawCommands();
    
    // Wait for frame
    XrFrameWaitInfo waitInfo = {
        .type = XR_TYPE_FRAME_WAIT_INFO,
        .next = NULL
    };
    
    XrFrameState frameState = {
        .type = XR_TYPE_FRAME_STATE,
        .next = NULL
    };
    
    xrWaitFrame(vrState.session, &waitInfo, &frameState);
    vrState.predictedDisplayTime = frameState.predictedDisplayTime;
    
    // Begin frame
    XrFrameBeginInfo beginInfo = {
        .type = XR_TYPE_FRAME_BEGIN_INFO,
        .next = NULL
    };
    xrBeginFrame(vrState.session, &beginInfo);
    
    // Get views
    XrViewLocateInfo locateInfo = {
        .type = XR_TYPE_VIEW_LOCATE_INFO,
        .next = NULL,
        .viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
        .displayTime = vrState.predictedDisplayTime,
        .space = vrState.stageSpace
    };
    
    XrViewState viewState = {
        .type = XR_TYPE_VIEW_STATE,
        .next = NULL
    };
    
    for (uint32_t i = 0; i < vrState.viewCount; i++) {
        vrState.views[i].type = XR_TYPE_VIEW;
        vrState.views[i].next = NULL;
    }
    
    uint32_t viewCount = 0;
    xrLocateViews(vrState.session, &locateInfo, &viewState, vrState.viewCount, &viewCount, vrState.views);
    
    // Update headset eye data
    for (uint32_t i = 0; i < viewCount && i < 2; i++) {
        Matrix proj = CreateProjectionMatrix(vrState.views[i].fov, 0.01f, 100.0f);
        Matrix view = CreateViewMatrix(vrState.views[i].pose);
        
        if (i == 0) {
            vrState.headset.leftEyeProjection = proj;
            vrState.headset.leftEyeView = view;
            vrState.headset.leftEyePosition = (Vector3){
                vrState.views[i].pose.position.x,
                vrState.views[i].pose.position.y,
                vrState.views[i].pose.position.z
            };
        } else {
            vrState.headset.rightEyeProjection = proj;
            vrState.headset.rightEyeView = view;
            vrState.headset.rightEyePosition = (Vector3){
                vrState.views[i].pose.position.x,
                vrState.views[i].pose.position.y,
                vrState.views[i].pose.position.z
            };
        }
    }
    
    vrState.headset.displayWidth = vrState.viewConfig[0].recommendedImageRectWidth;
    vrState.headset.displayHeight = vrState.viewConfig[0].recommendedImageRectHeight;
}

void EndVRMode(void) {
    if (!vrState.sessionRunning) return;
    
    XrCompositionLayerProjectionView projectionViews[MAX_VIEWS] = {0};
    
    for (uint32_t i = 0; i < vrState.viewCount; i++) {
        // Acquire swapchain image
        XrSwapchainImageAcquireInfo acquireInfo = {
            .type = XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO,
            .next = NULL
        };
        uint32_t imageIndex;
        xrAcquireSwapchainImage(vrState.swapchain[i], &acquireInfo, &imageIndex);
        
        // Wait for image
        XrSwapchainImageWaitInfo waitInfo = {
            .type = XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO,
            .next = NULL,
            .timeout = XR_INFINITE_DURATION
        };
        xrWaitSwapchainImage(vrState.swapchain[i], &waitInfo);
        
        // Render to this eye
        vrState.currentEye = i;
        vrState.currentViewMatrix = (i == 0) ? vrState.headset.leftEyeView : vrState.headset.rightEyeView;
        vrState.currentProjectionMatrix = (i == 0) ? vrState.headset.leftEyeProjection : vrState.headset.rightEyeProjection;
        
        RenderEye(i, imageIndex);
        
        // Release swapchain image
        XrSwapchainImageReleaseInfo releaseInfo = {
            .type = XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO,
            .next = NULL
        };
        xrReleaseSwapchainImage(vrState.swapchain[i], &releaseInfo);
        
        // Set up projection view
        projectionViews[i].type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;
        projectionViews[i].next = NULL;
        projectionViews[i].pose = vrState.views[i].pose;
        projectionViews[i].fov = vrState.views[i].fov;
        projectionViews[i].subImage.swapchain = vrState.swapchain[i];
        projectionViews[i].subImage.imageRect.offset = (XrOffset2Di){0, 0};
        projectionViews[i].subImage.imageRect.extent = (XrExtent2Di){
            vrState.viewConfig[i].recommendedImageRectWidth,
            vrState.viewConfig[i].recommendedImageRectHeight
        };
        projectionViews[i].subImage.imageArrayIndex = 0;
    }
    
    // Submit frame
    XrCompositionLayerProjection projectionLayer = {
        .type = XR_TYPE_COMPOSITION_LAYER_PROJECTION,
        .next = NULL,
        .layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT,
        .space = vrState.stageSpace,
        .viewCount = vrState.viewCount,
        .views = projectionViews
    };
    
    const XrCompositionLayerBaseHeader* layers[] = {
        (XrCompositionLayerBaseHeader*)&projectionLayer
    };
    
    XrFrameEndInfo endInfo = {
        .type = XR_TYPE_FRAME_END_INFO,
        .next = NULL,
        .displayTime = vrState.predictedDisplayTime,
        .environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE,
        .layerCount = 1,
        .layers = layers
    };
    
    xrEndFrame(vrState.session, &endInfo);
}

void SetVRClearColor(Color color) {
    vrState.clearColor = color;
}

void SyncControllers(void) {
    UpdateInput();
}

VRController GetController(ControllerHand hand) {
    return vrState.controllers[hand];
}

VRHeadset GetHeadset(void) {
    return vrState.headset;
}

Vector3 GetVRControllerPosition(int hand) {
    if (hand < 0 || hand > 1) return (Vector3){0, 0, 0};
    return vrState.controllers[hand].position;
}

Quaternion GetVRControllerOrientation(int hand) {
    if (hand < 0 || hand > 1) return (Quaternion){0, 0, 0, 1};
    return vrState.controllers[hand].orientation;
}

float GetVRControllerGrip(int hand) {
    if (hand < 0 || hand > 1) return 0.0f;
    return vrState.controllers[hand].grip;
}

float GetVRControllerTrigger(int hand) {
    if (hand < 0 || hand > 1) return 0.0f;
    return vrState.controllers[hand].trigger;
}

Vector3 GetVRControllerThumbstick(int hand) {
    if (hand < 0 || hand > 1) return (Vector3){0, 0, 0};
    return (Vector3){vrState.controllers[hand].thumbstickX, vrState.controllers[hand].thumbstickY, 0};
}

void TriggerVRHaptic(int hand, float amplitude, float duration) {
    if (hand < 0 || hand > 1) return;
    if (!vrState.sessionRunning) return;
    
    XrHapticActionInfo hapticInfo = {
        .type = XR_TYPE_HAPTIC_ACTION_INFO,
        .next = NULL,
        .action = vrState.hapticAction,
        .subactionPath = (hand == 0) ? vrState.leftHandPath : vrState.rightHandPath
    };
    
    XrHapticVibration vibration = {
        .type = XR_TYPE_HAPTIC_VIBRATION,
        .next = NULL,
        .duration = (XrDuration)(duration * 1000000000.0),  // seconds to nanoseconds
        .frequency = XR_FREQUENCY_UNSPECIFIED,
        .amplitude = amplitude
    };
    
    xrApplyHapticFeedback(vrState.session, &hapticInfo, (XrHapticBaseHeader*)&vibration);
}

// =============================================================================
// Player Movement Implementation
// =============================================================================

void SetPlayerPosition(Vector3 position) {
    vrState.playerPosition = position;
}

Vector3 GetPlayerPosition(void) {
    return vrState.playerPosition;
}

void SetPlayerYaw(float yaw) {
    vrState.playerYaw = yaw;
}

float GetPlayerYaw(void) {
    return vrState.playerYaw;
}

void MovePlayer(float forward, float strafe, float up) {
    // Convert yaw to radians
    float yawRad = vrState.playerYaw * PI / 180.0f;
    
    // Calculate movement direction based on player yaw
    float sinYaw = sinf(yawRad);
    float cosYaw = cosf(yawRad);
    
    // Forward is -Z in OpenGL convention
    vrState.playerPosition.x += -sinYaw * forward + cosYaw * strafe;
    vrState.playerPosition.z += -cosYaw * forward - sinYaw * strafe;
    vrState.playerPosition.y += up;
}

bool IsPlayerGrounded(float groundHeight) {
    return vrState.playerPosition.y <= groundHeight;
}

// =============================================================================
// Rendering Implementation
// =============================================================================

// Simple shader programs (embedded source)
static const char* vertexShaderSource = 
    "#version 300 es\n"
    "layout(location = 0) in vec3 aPosition;\n"
    "uniform mat4 uMVP;\n"
    "void main() {\n"
    "    gl_Position = uMVP * vec4(aPosition, 1.0);\n"
    "}\n";

static const char* fragmentShaderSource = 
    "#version 300 es\n"
    "precision mediump float;\n"
    "uniform vec4 uColor;\n"
    "out vec4 fragColor;\n"
    "void main() {\n"
    "    fragColor = uColor;\n"
    "}\n";

static GLuint CompileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    
    GLint compiled;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        char log[512];
        glGetShaderInfoLog(shader, 512, NULL, log);
        LOGE("Shader compile error: %s", log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

static void InitShaders(void) {
    if (shaderProgram != 0) return;
    
    GLuint vs = CompileShader(GL_VERTEX_SHADER, vertexShaderSource);
    GLuint fs = CompileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);
    
    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vs);
    glAttachShader(shaderProgram, fs);
    glLinkProgram(shaderProgram);
    
    glDeleteShader(vs);
    glDeleteShader(fs);
    
    uniformMVP = glGetUniformLocation(shaderProgram, "uMVP");
    uniformColor = glGetUniformLocation(shaderProgram, "uColor");
}

// Internal function to draw a cube (used by RenderEye)
static void DrawCubeInternal(Vector3 position, Vector3 size, Vector3 color) {
    InitShaders();
    InitCubeGeometry();
    
    glUseProgram(shaderProgram);
    
    // Create model matrix
    Matrix model = MatrixMultiply(MatrixTranslate(position.x, position.y, position.z),
                                   MatrixScale(size.x, size.y, size.z));
    
    // Create MVP matrix
    Matrix mvp = MatrixMultiply(MatrixMultiply(model, vrState.currentViewMatrix), vrState.currentProjectionMatrix);
    
    // Convert to column-major for OpenGL
    float mvpArray[16] = {
        mvp.m0, mvp.m1, mvp.m2, mvp.m3,
        mvp.m4, mvp.m5, mvp.m6, mvp.m7,
        mvp.m8, mvp.m9, mvp.m10, mvp.m11,
        mvp.m12, mvp.m13, mvp.m14, mvp.m15
    };
    
    glUniformMatrix4fv(uniformMVP, 1, GL_FALSE, mvpArray);
    glUniform4f(uniformColor, color.x, color.y, color.z, 1.0f);
    
    glBindVertexArray(cubeVAO);
    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_SHORT, 0);
    glBindVertexArray(0);
}

// Internal function to draw a line (used by RenderEye)
static void DrawLineInternal(Vector3 startPos, Vector3 endPos, Vector3 color) {
    InitShaders();
    glUseProgram(shaderProgram);
    
    float vertices[] = {
        startPos.x, startPos.y, startPos.z,
        endPos.x, endPos.y, endPos.z
    };
    
    GLuint lineVAO, lineVBO;
    glGenVertexArrays(1, &lineVAO);
    glGenBuffers(1, &lineVBO);
    
    glBindVertexArray(lineVAO);
    glBindBuffer(GL_ARRAY_BUFFER, lineVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);
    
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    // Create MVP matrix (identity model)
    Matrix mvp = MatrixMultiply(vrState.currentViewMatrix, vrState.currentProjectionMatrix);
    
    float mvpArray[16] = {
        mvp.m0, mvp.m1, mvp.m2, mvp.m3,
        mvp.m4, mvp.m5, mvp.m6, mvp.m7,
        mvp.m8, mvp.m9, mvp.m10, mvp.m11,
        mvp.m12, mvp.m13, mvp.m14, mvp.m15
    };
    
    glUniformMatrix4fv(uniformMVP, 1, GL_FALSE, mvpArray);
    glUniform4f(uniformColor, color.x, color.y, color.z, 1.0f);
    
    glDrawArrays(GL_LINES, 0, 2);
    
    glBindVertexArray(0);
    glDeleteBuffers(1, &lineVBO);
    glDeleteVertexArrays(1, &lineVAO);
}

static void RenderEye(int eye, uint32_t imageIndex) {
    // Bind framebuffer with the acquired swapchain image
    glBindFramebuffer(GL_FRAMEBUFFER, vrState.framebuffer[eye]);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 
        vrState.swapchainImages[eye][imageIndex].image, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, 
        GL_RENDERBUFFER, vrState.depthBuffer[eye]);
    
    // Set viewport
    glViewport(0, 0, vrState.viewConfig[eye].recommendedImageRectWidth,
        vrState.viewConfig[eye].recommendedImageRectHeight);
    
    // Clear with a dark blue color so we can see something
    glClearColor(0.1f, 0.1f, 0.2f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // Enable depth test
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    
    // Initialize shaders if needed
    InitShaders();
    InitCubeGeometry();
    
    // Replay all stored draw commands
    static int frameCount = 0;
    if (eye == 0 && ++frameCount % 100 == 0) {
        LOGD("Rendering frame %d with %d draw commands", frameCount, drawCommandCount);
    }
    
    for (int i = 0; i < drawCommandCount; i++) {
        DrawCommand* cmd = &drawCommands[i];
        switch (cmd->type) {
            case CMD_DRAW_CUBE:
                DrawCubeInternal(cmd->position, cmd->size, cmd->color);
                break;
            case CMD_DRAW_LINE:
                DrawLineInternal(cmd->position, cmd->size, cmd->color);
                break;
        }
    }
}

// Cube vertices
static const float cubeVertices[] = {
    // Front face
    -0.5f, -0.5f,  0.5f,
     0.5f, -0.5f,  0.5f,
     0.5f,  0.5f,  0.5f,
    -0.5f,  0.5f,  0.5f,
    // Back face
    -0.5f, -0.5f, -0.5f,
    -0.5f,  0.5f, -0.5f,
     0.5f,  0.5f, -0.5f,
     0.5f, -0.5f, -0.5f,
    // Top face
    -0.5f,  0.5f, -0.5f,
    -0.5f,  0.5f,  0.5f,
     0.5f,  0.5f,  0.5f,
     0.5f,  0.5f, -0.5f,
    // Bottom face
    -0.5f, -0.5f, -0.5f,
     0.5f, -0.5f, -0.5f,
     0.5f, -0.5f,  0.5f,
    -0.5f, -0.5f,  0.5f,
    // Right face
     0.5f, -0.5f, -0.5f,
     0.5f,  0.5f, -0.5f,
     0.5f,  0.5f,  0.5f,
     0.5f, -0.5f,  0.5f,
    // Left face
    -0.5f, -0.5f, -0.5f,
    -0.5f, -0.5f,  0.5f,
    -0.5f,  0.5f,  0.5f,
    -0.5f,  0.5f, -0.5f,
};

static const unsigned short cubeIndices[] = {
    0,  1,  2,    0,  2,  3,    // front
    4,  5,  6,    4,  6,  7,    // back
    8,  9,  10,   8,  10, 11,   // top
    12, 13, 14,   12, 14, 15,   // bottom
    16, 17, 18,   16, 18, 19,   // right
    20, 21, 22,   20, 22, 23,   // left
};

static void InitCubeGeometry(void) {
    if (cubeVAO != 0) return;
    
    glGenVertexArrays(1, &cubeVAO);
    glGenBuffers(1, &cubeVBO);
    glGenBuffers(1, &cubeEBO);
    
    glBindVertexArray(cubeVAO);
    
    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices, GL_STATIC_DRAW);
    
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cubeEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cubeIndices), cubeIndices, GL_STATIC_DRAW);
    
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    glBindVertexArray(0);
}

void DrawVRCuboid(Vector3 position, Vector3 size, Vector3 color) {
    if (!vrState.sessionRunning) return;
    
    // Add to command buffer - will be drawn for each eye in EndVRMode
    DrawCommand cmd = {
        .type = CMD_DRAW_CUBE,
        .position = position,
        .size = size,
        .color = color
    };
    AddDrawCommand(cmd);
}

void DrawVRCube(Vector3 position, float size, Color color) {
    DrawVRCuboid(position, (Vector3){size, size, size}, 
        (Vector3){color.r / 255.0f, color.g / 255.0f, color.b / 255.0f});
}

void DrawVRSphere(Vector3 position, float radius, Color color) {
    // Simplified: draw as a cube for now (proper sphere needs more geometry)
    DrawVRCube(position, radius * 2, color);
}

void DrawVRGrid(int slices, float spacing) {
    if (!vrState.sessionRunning) return;
    
    InitShaders();
    glUseProgram(shaderProgram);
    
    // Draw grid lines
    float half = (slices * spacing) / 2.0f;
    
    for (int i = -slices/2; i <= slices/2; i++) {
        float pos = i * spacing;
        
        // Line along X
        DrawVRLine3D(
            (Vector3){-half, 0, pos},
            (Vector3){half, 0, pos},
            GRAY
        );
        
        // Line along Z
        DrawVRLine3D(
            (Vector3){pos, 0, -half},
            (Vector3){pos, 0, half},
            GRAY
        );
    }
}

void DrawVRLine3D(Vector3 startPos, Vector3 endPos, Color color) {
    if (!vrState.sessionRunning) return;
    
    // Add to command buffer - will be drawn for each eye in EndVRMode
    // We store startPos in position and endPos in size
    DrawCommand cmd = {
        .type = CMD_DRAW_LINE,
        .position = startPos,
        .size = endPos,  // Repurposed as end position for lines
        .color = (Vector3){color.r / 255.0f, color.g / 255.0f, color.b / 255.0f}
    };
    AddDrawCommand(cmd);
}

void DrawVRCylinder(Vector3 position, float radiusTop, float radiusBottom, float height, Color color) {
    // Simplified: draw as stretched cube
    DrawVRCube((Vector3){position.x, position.y + height/2, position.z}, 
        (radiusTop + radiusBottom), color);
}

void DrawVRPlane(Vector3 centerPos, Vector3 size, Color color) {
    DrawVRCuboid(centerPos, (Vector3){size.x, 0.01f, size.z}, 
        (Vector3){color.r / 255.0f, color.g / 255.0f, color.b / 255.0f});
}

void DrawVRAxes(Vector3 position, float scale) {
    // X axis (red)
    DrawVRLine3D(position, Vector3Add(position, (Vector3){scale, 0, 0}), RED);
    // Y axis (green)
    DrawVRLine3D(position, Vector3Add(position, (Vector3){0, scale, 0}), GREEN);
    // Z axis (blue)
    DrawVRLine3D(position, Vector3Add(position, (Vector3){0, 0, scale}), BLUE);
}
