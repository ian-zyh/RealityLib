/**
 * RealityLib - Simple VR World Example
 * 
 * This example demonstrates a basic VR environment where you can:
 * - Look around in 360 degrees
 * - See floating cubes in the environment
 * - See your controllers represented as colored cubes
 * - Move around using the thumbsticks
 * 
 * To create your own VR game, modify the inLoop() function below!
 */

#include "realitylib_vr.h"
#include <android/log.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#define PI M_PI

#define LOG_TAG "RealityLibApp"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// =============================================================================
// World Configuration
// =============================================================================

#define NUM_FLOATING_CUBES 30
#define WORLD_SIZE 10.0f

// World state
typedef struct {
    // Player position (for movement)
    Vector3 playerPosition;
    float playerYaw;  // Rotation around Y axis
    
    // Floating cubes
    Vector3 cubePositions[NUM_FLOATING_CUBES];
    Color cubeColors[NUM_FLOATING_CUBES];
    float cubeRotations[NUM_FLOATING_CUBES];
    float cubeSpeeds[NUM_FLOATING_CUBES];
    float cubeBobPhase[NUM_FLOATING_CUBES];
    
    // Time tracking
    float time;
    
    // Initialized flag
    bool initialized;
} WorldState;

static WorldState world = {0};

// =============================================================================
// Helper Functions
// =============================================================================

// Simple pseudo-random number generator
static unsigned int randomSeed = 12345;
static float RandomFloat(void) {
    randomSeed = randomSeed * 1103515245 + 12345;
    return (float)(randomSeed % 10000) / 10000.0f;
}

static float RandomRange(float min, float max) {
    return min + RandomFloat() * (max - min);
}

static Color RandomColor(void) {
    return (Color){
        (unsigned char)(50 + RandomFloat() * 205),
        (unsigned char)(50 + RandomFloat() * 205),
        (unsigned char)(50 + RandomFloat() * 205),
        255
    };
}

// =============================================================================
// World Initialization
// =============================================================================

static void InitWorld(void) {
    if (world.initialized) return;
    
    LOGI("Initializing VR World...");
    
    // Player starts at origin
    world.playerPosition = Vector3Create(0.0f, 0.0f, 0.0f);
    world.playerYaw = 0.0f;
    
    // Initialize floating cubes in a sphere around the player
    for (int i = 0; i < NUM_FLOATING_CUBES; i++) {
        // Random position in a sphere around the player
        float angle = RandomFloat() * 2.0f * PI;
        float elevation = RandomRange(-0.5f, 1.5f);
        float distance = RandomRange(2.0f, WORLD_SIZE);
        
        world.cubePositions[i] = Vector3Create(
            cosf(angle) * distance,
            elevation + 1.0f,  // Offset so cubes are at eye level and above
            sinf(angle) * distance
        );
        
        world.cubeColors[i] = RandomColor();
        world.cubeRotations[i] = RandomFloat() * 360.0f;
        world.cubeSpeeds[i] = RandomRange(10.0f, 30.0f);
        world.cubeBobPhase[i] = RandomFloat() * 2.0f * PI;
    }
    
    world.time = 0.0f;
    world.initialized = true;
    
    LOGI("VR World initialized with %d floating cubes", NUM_FLOATING_CUBES);
}

// =============================================================================
// Drawing Functions
// =============================================================================

static void DrawEnvironment(void) {
    // Draw a large ground plane
    DrawVRPlane(
        Vector3Create(0.0f, 0.0f, 0.0f),
        Vector3Create(WORLD_SIZE * 2, 0.0f, WORLD_SIZE * 2),
        (Color){40, 40, 60, 255}
    );
    
    // Draw grid on the ground
    DrawVRGrid(20, 1.0f);
    
    // Draw coordinate axes at origin
    DrawVRAxes(Vector3Create(0.0f, 0.01f, 0.0f), 1.0f);
}

static void DrawFloatingCubes(void) {
    for (int i = 0; i < NUM_FLOATING_CUBES; i++) {
        // Add bobbing motion
        float bob = sinf(world.time * 2.0f + world.cubeBobPhase[i]) * 0.1f;
        
        Vector3 pos = world.cubePositions[i];
        pos.y += bob;
        
        // Different sizes based on distance (closer = smaller for variety)
        float distance = Vector3Distance(pos, world.playerPosition);
        float size = 0.1f + (distance / WORLD_SIZE) * 0.2f;
        
        DrawVRCube(pos, size, world.cubeColors[i]);
    }
}

static void DrawPillars(void) {
    // Draw some pillars around the edge of the world
    int numPillars = 8;
    float pillarRadius = WORLD_SIZE - 1.0f;
    
    for (int i = 0; i < numPillars; i++) {
        float angle = (float)i / numPillars * 2.0f * PI;
        Vector3 pos = Vector3Create(
            cosf(angle) * pillarRadius,
            1.0f,  // Half height
            sinf(angle) * pillarRadius
        );
        
        // Pillar (tall thin cube)
        DrawVRCuboid(pos, Vector3Create(0.3f, 2.0f, 0.3f),
            Vector3Create(0.6f, 0.6f, 0.7f));
        
        // Light on top of pillar
        Vector3 lightPos = pos;
        lightPos.y = 2.1f;
        
        // Pulsing light color
        float pulse = (sinf(world.time * 3.0f + angle) + 1.0f) * 0.5f;
        DrawVRCuboid(lightPos, Vector3Create(0.15f, 0.15f, 0.15f),
            Vector3Create(1.0f, pulse, 0.2f));
    }
}

static void DrawControllers(void) {
    VRController leftController = GetController(CONTROLLER_LEFT);
    VRController rightController = GetController(CONTROLLER_RIGHT);
    
    // Draw left controller (blue)
    if (leftController.isTracking) {
        // Main controller body
        DrawVRCube(leftController.position, 0.05f, BLUE);
        
        // Draw a pointer ray when trigger is pressed
        if (leftController.trigger > 0.1f) {
            Vector3 forward = QuaternionForward(leftController.orientation);
            Vector3 rayEnd = Vector3Add(leftController.position, 
                Vector3Scale(forward, -2.0f * leftController.trigger));
            DrawVRLine3D(leftController.position, rayEnd, SKYBLUE);
        }
    }
    
    // Draw right controller (green)
    if (rightController.isTracking) {
        // Main controller body
        DrawVRCube(rightController.position, 0.05f, GREEN);
        
        // Draw a pointer ray when trigger is pressed
        if (rightController.trigger > 0.1f) {
            Vector3 forward = QuaternionForward(rightController.orientation);
            Vector3 rayEnd = Vector3Add(rightController.position, 
                Vector3Scale(forward, -2.0f * rightController.trigger));
            DrawVRLine3D(rightController.position, rayEnd, LIME);
        }
    }
}

static void DrawSkybox(void) {
    // Simple colored "walls" to give a sense of enclosure
    float skyDistance = WORLD_SIZE * 1.5f;
    float skyHeight = 5.0f;
    
    // North wall (blue gradient)
    DrawVRCuboid(
        Vector3Create(0.0f, skyHeight/2, -skyDistance),
        Vector3Create(skyDistance * 2, skyHeight, 0.1f),
        Vector3Create(0.1f, 0.1f, 0.3f)
    );
    
    // South wall
    DrawVRCuboid(
        Vector3Create(0.0f, skyHeight/2, skyDistance),
        Vector3Create(skyDistance * 2, skyHeight, 0.1f),
        Vector3Create(0.3f, 0.1f, 0.1f)
    );
    
    // East wall
    DrawVRCuboid(
        Vector3Create(skyDistance, skyHeight/2, 0.0f),
        Vector3Create(0.1f, skyHeight, skyDistance * 2),
        Vector3Create(0.1f, 0.3f, 0.1f)
    );
    
    // West wall
    DrawVRCuboid(
        Vector3Create(-skyDistance, skyHeight/2, 0.0f),
        Vector3Create(0.1f, skyHeight, skyDistance * 2),
        Vector3Create(0.3f, 0.3f, 0.1f)
    );
}

// =============================================================================
// Input Handling
// =============================================================================

static void HandleInput(void) {
    VRController leftController = GetController(CONTROLLER_LEFT);
    VRController rightController = GetController(CONTROLLER_RIGHT);
    VRHeadset headset = GetHeadset();
    
    // Movement speed
    float moveSpeed = 0.02f;
    
    // Left thumbstick: Move forward/backward and strafe
    if (leftController.isTracking) {
        float moveX = leftController.thumbstickX;
        float moveZ = leftController.thumbstickY;
        
        if (fabsf(moveX) > 0.1f || fabsf(moveZ) > 0.1f) {
            // Get headset forward direction (projected onto XZ plane)
            Vector3 forward = QuaternionForward(headset.orientation);
            forward.y = 0;
            forward = Vector3Normalize(forward);
            
            Vector3 right = QuaternionRight(headset.orientation);
            right.y = 0;
            right = Vector3Normalize(right);
            
            // Apply movement
            world.playerPosition = Vector3Add(world.playerPosition,
                Vector3Scale(forward, -moveZ * moveSpeed));
            world.playerPosition = Vector3Add(world.playerPosition,
                Vector3Scale(right, moveX * moveSpeed));
        }
    }
    
    // Right thumbstick: Snap turn
    static bool canTurn = true;
    if (rightController.isTracking) {
        float turnX = rightController.thumbstickX;
        
        if (fabsf(turnX) > 0.7f && canTurn) {
            world.playerYaw += (turnX > 0) ? -45.0f : 45.0f;
            canTurn = false;
            TriggerVRHaptic(CONTROLLER_RIGHT, 0.3f, 0.05f);
        }
        if (fabsf(turnX) < 0.3f) {
            canTurn = true;
        }
    }
    
    // Grip buttons: Haptic feedback test
    if (leftController.grip > 0.5f) {
        TriggerVRHaptic(CONTROLLER_LEFT, leftController.grip * 0.5f, 0.016f);
    }
    if (rightController.grip > 0.5f) {
        TriggerVRHaptic(CONTROLLER_RIGHT, rightController.grip * 0.5f, 0.016f);
    }
    
    // A button: Spawn a cube at right controller position
    static bool aButtonWasPressed = false;
    if (rightController.buttonA && !aButtonWasPressed) {
        // Find an empty slot and place a new cube
        for (int i = 0; i < NUM_FLOATING_CUBES; i++) {
            // Reset a random cube to controller position
            if (RandomFloat() < 0.1f) {
                world.cubePositions[i] = rightController.position;
                world.cubeColors[i] = RandomColor();
                TriggerVRHaptic(CONTROLLER_RIGHT, 1.0f, 0.1f);
                break;
            }
        }
        aButtonWasPressed = true;
    }
    if (!rightController.buttonA) {
        aButtonWasPressed = false;
    }
}

// =============================================================================
// Main Loop Function (Called Every Frame)
// =============================================================================

void inLoop(struct android_app* app) {
    // Initialize world on first frame
    InitWorld();
    
    // Update time (approximate 72Hz)
    world.time += 1.0f / 72.0f;
    
    // Handle input
    HandleInput();
    
    // Draw the VR scene
    // Note: Drawing happens automatically for both eyes
    
    // Draw environment
    DrawEnvironment();
    DrawSkybox();
    DrawPillars();
    
    // Draw floating cubes
    DrawFloatingCubes();
    
    // Draw controllers
    DrawControllers();
}

// =============================================================================
// Application Entry Point
// =============================================================================

void android_main(struct android_app* app) {
    LOGI("RealityLib VR Application Starting...");
    
    // Initialize the VR system
    if (!InitApp(app)) {
        LOGE("Failed to initialize VR application!");
        return;
    }
    LOGI("VR Application Initialized Successfully");
    
    // Set background color (dark purple-blue space feeling)
    SetVRClearColor((Color){15, 15, 30, 255});
    
    // Main application loop
    while (!AppShouldClose(app)) {
        // Begin VR frame
        BeginVRMode();
        
        // Sync controller input
        SyncControllers();
        
        // Run user's game logic and drawing
        inLoop(app);
        
        // End VR frame (submits to headset)
        EndVRMode();
    }
    
    // Cleanup
    LOGI("Shutting down VR Application...");
    CloseApp(app);
    LOGI("VR Application Closed");
}
