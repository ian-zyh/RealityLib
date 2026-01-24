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

// Physics constants
#define GRAVITY -9.8f
#define JUMP_VELOCITY 4.0f
#define GROUND_HEIGHT 0.0f
#define MOVE_SPEED 3.0f
#define SPRINT_MULTIPLIER 2.0f
#define SMOOTH_TURN_SPEED 90.0f  // Degrees per second for smooth turning

// World state
typedef struct {
    // Player physics
    float playerVelocityY;  // Vertical velocity for jump/fall
    bool isGrounded;
    bool canJump;
    
    // Floating cubes
    Vector3 cubePositions[NUM_FLOATING_CUBES];
    Color cubeColors[NUM_FLOATING_CUBES];
    float cubeRotations[NUM_FLOATING_CUBES];
    float cubeSpeeds[NUM_FLOATING_CUBES];
    float cubeBobPhase[NUM_FLOATING_CUBES];
    
    // Time tracking
    float time;
    float deltaTime;
    
    // Input state (for detecting button presses)
    bool jumpReady;
    bool spawnReady;
    
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
    
    // Player starts at origin, on the ground
    SetPlayerPosition(Vector3Create(0.0f, 0.0f, 0.0f));
    SetPlayerYaw(0.0f);
    
    // Player physics
    world.playerVelocityY = 0.0f;
    world.isGrounded = true;
    world.canJump = true;
    
    // Input states
    world.jumpReady = true;
    world.spawnReady = true;
    
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
    world.deltaTime = 1.0f / 72.0f;  // Approximate at 72Hz
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
    Vector3 playerPos = GetPlayerPosition();
    
    for (int i = 0; i < NUM_FLOATING_CUBES; i++) {
        // Add bobbing motion
        float bob = sinf(world.time * 2.0f + world.cubeBobPhase[i]) * 0.1f;
        
        Vector3 pos = world.cubePositions[i];
        pos.y += bob;
        
        // Different sizes based on distance (closer = smaller for variety)
        float distance = Vector3Distance(pos, playerPos);
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
// Physics Update
// =============================================================================

static void UpdatePhysics(void) {
    Vector3 playerPos = GetPlayerPosition();
    
    // Apply gravity if not grounded
    if (!world.isGrounded) {
        world.playerVelocityY += GRAVITY * world.deltaTime;
    }
    
    // Apply vertical velocity
    playerPos.y += world.playerVelocityY * world.deltaTime;
    
    // Ground collision
    if (playerPos.y <= GROUND_HEIGHT) {
        playerPos.y = GROUND_HEIGHT;
        world.playerVelocityY = 0.0f;
        world.isGrounded = true;
    } else {
        world.isGrounded = false;
    }
    
    // Update player position
    SetPlayerPosition(playerPos);
}

// =============================================================================
// Input Handling
// =============================================================================

static void HandleInput(void) {
    VRController leftController = GetController(CONTROLLER_LEFT);
    VRController rightController = GetController(CONTROLLER_RIGHT);
    VRHeadset headset = GetHeadset();
    
    // =========================================================================
    // LEFT THUMBSTICK: Movement (forward/back/strafe)
    // =========================================================================
    if (leftController.isTracking) {
        float moveX = leftController.thumbstickX;  // Strafe (positive = right)
        float moveZ = leftController.thumbstickY;  // Forward/back (positive = forward)
        
        // Apply deadzone
        if (fabsf(moveX) < 0.1f) moveX = 0.0f;
        if (fabsf(moveZ) < 0.1f) moveZ = 0.0f;
        
        if (fabsf(moveX) > 0.0f || fabsf(moveZ) > 0.0f) {
            // Calculate movement speed (sprint if trigger held)
            float speed = MOVE_SPEED;
            if (leftController.trigger > 0.5f) {
                speed *= SPRINT_MULTIPLIER;
            }
            
            // Get player yaw to determine movement direction
            float playerYaw = GetPlayerYaw();
            float yawRad = playerYaw * PI / 180.0f;
            
            // Also factor in headset look direction for more natural movement
            Vector3 headForward = QuaternionForward(headset.orientation);
            float headYaw = atan2f(-headForward.x, -headForward.z);
            
            // Combined yaw (player + head)
            float combinedYaw = yawRad + headYaw;
            
            // Calculate movement delta
            // Forward: -Z direction in OpenGL, moveZ positive = forward
            // Strafe: +X is right, -X is left, moveX positive = right
            float dx = sinf(combinedYaw) * moveZ - cosf(combinedYaw) * moveX;
            float dz = cosf(combinedYaw) * moveZ + sinf(combinedYaw) * moveX;
            
            // Apply movement
            Vector3 playerPos = GetPlayerPosition();
            playerPos.x += dx * speed * world.deltaTime;
            playerPos.z += dz * speed * world.deltaTime;
            SetPlayerPosition(playerPos);
        }
    }
    
    // =========================================================================
    // RIGHT THUMBSTICK: Smooth Turn (left/right)
    // =========================================================================
    if (rightController.isTracking) {
        float turnX = rightController.thumbstickX;
        
        // Apply deadzone
        if (fabsf(turnX) < 0.15f) turnX = 0.0f;
        
        // Smooth continuous turning
        if (fabsf(turnX) > 0.0f) {
            float currentYaw = GetPlayerYaw();
            // turnX positive = push right = turn right (decrease yaw)
            // turnX negative = push left = turn left (increase yaw)
            float turnAmount = -turnX * SMOOTH_TURN_SPEED * world.deltaTime;
            SetPlayerYaw(currentYaw + turnAmount);
        }
    }
    
    // =========================================================================
    // A BUTTON (Right Controller): Jump
    // =========================================================================
    if (rightController.buttonA && world.jumpReady && world.isGrounded) {
        world.playerVelocityY = JUMP_VELOCITY;
        world.isGrounded = false;
        world.jumpReady = false;
        TriggerVRHaptic(CONTROLLER_RIGHT, 0.5f, 0.1f);
        LOGI("Jump! Velocity: %.2f", world.playerVelocityY);
    }
    if (!rightController.buttonA) {
        world.jumpReady = true;
    }
    
    // =========================================================================
    // B BUTTON (Right Controller): Spawn cube at controller
    // =========================================================================
    if (rightController.buttonB && world.spawnReady) {
        // Find a random cube and move it to controller position
        int cubeIndex = (int)(RandomFloat() * NUM_FLOATING_CUBES);
        
        // Get controller position in world space (need to account for player offset)
        Vector3 playerPos = GetPlayerPosition();
        float playerYaw = GetPlayerYaw() * PI / 180.0f;
        
        // Transform controller position by player offset
        Vector3 ctrlPos = rightController.position;
        Vector3 worldPos = {
            ctrlPos.x * cosf(playerYaw) - ctrlPos.z * sinf(playerYaw) + playerPos.x,
            ctrlPos.y + playerPos.y,
            ctrlPos.x * sinf(playerYaw) + ctrlPos.z * cosf(playerYaw) + playerPos.z
        };
        
        world.cubePositions[cubeIndex] = worldPos;
        world.cubeColors[cubeIndex] = RandomColor();
        world.spawnReady = false;
        TriggerVRHaptic(CONTROLLER_RIGHT, 1.0f, 0.1f);
    }
    if (!rightController.buttonB) {
        world.spawnReady = true;
    }
    
    // =========================================================================
    // GRIP BUTTONS: Haptic feedback test
    // =========================================================================
    if (leftController.grip > 0.5f) {
        TriggerVRHaptic(CONTROLLER_LEFT, leftController.grip * 0.5f, 0.016f);
    }
    if (rightController.grip > 0.5f) {
        TriggerVRHaptic(CONTROLLER_RIGHT, rightController.grip * 0.5f, 0.016f);
    }
    
    // =========================================================================
    // X BUTTON (Left Controller): Teleport to origin / Reset position
    // =========================================================================
    static bool xButtonReady = true;
    if (leftController.buttonA && xButtonReady) {  // buttonA on left = X button
        SetPlayerPosition(Vector3Create(0.0f, 0.0f, 0.0f));
        SetPlayerYaw(0.0f);
        world.playerVelocityY = 0.0f;
        world.isGrounded = true;
        xButtonReady = false;
        TriggerVRHaptic(CONTROLLER_LEFT, 1.0f, 0.2f);
        LOGI("Teleported to origin");
    }
    if (!leftController.buttonA) {
        xButtonReady = true;
    }
    
    // =========================================================================
    // Y BUTTON (Left Controller): Toggle fly mode (no gravity)
    // =========================================================================
    static bool flyMode = false;
    static bool yButtonReady = true;
    if (leftController.buttonB && yButtonReady) {  // buttonB on left = Y button
        flyMode = !flyMode;
        yButtonReady = false;
        TriggerVRHaptic(CONTROLLER_LEFT, 0.5f, 0.15f);
        LOGI("Fly mode: %s", flyMode ? "ON" : "OFF");
        
        if (flyMode) {
            world.playerVelocityY = 0.0f;
        }
    }
    if (!leftController.buttonB) {
        yButtonReady = true;
    }
    
    // Fly mode movement (use right thumbstick Y for up/down)
    if (flyMode && rightController.isTracking) {
        float flyY = rightController.thumbstickY;
        if (fabsf(flyY) > 0.1f) {
            Vector3 playerPos = GetPlayerPosition();
            playerPos.y += flyY * MOVE_SPEED * world.deltaTime;
            SetPlayerPosition(playerPos);
            world.isGrounded = false;  // Not grounded while flying
        }
    }
}

// =============================================================================
// Main Loop Function (Called Every Frame)
// =============================================================================

void inLoop(struct android_app* app) {
    // Initialize world on first frame
    InitWorld();
    
    // Update time (approximate 72Hz)
    world.deltaTime = 1.0f / 72.0f;
    world.time += world.deltaTime;
    
    // Handle input first
    HandleInput();
    
    // Update physics (gravity, jumping)
    UpdatePhysics();
    
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
    
    // Debug: Log player position occasionally
    static int frameCount = 0;
    if (++frameCount % 500 == 0) {
        Vector3 pos = GetPlayerPosition();
        LOGI("Player pos: (%.2f, %.2f, %.2f) Yaw: %.1f Grounded: %s",
             pos.x, pos.y, pos.z, GetPlayerYaw(), world.isGrounded ? "yes" : "no");
    }
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
