/**
 * RealityLib - Simple VR World Example
 * 
 * This example demonstrates a basic VR environment where you can:
 * - Look around in 360 degrees
 * - See floating cubes in the environment
 * - See your controllers represented as colored cubes
 * - Move around using the thumbsticks
 * - Use HAND TRACKING: see your hands, pinch to grab!
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
    
    // Hand tracking state
    bool handTrackingEnabled;
    bool leftPinchReady;   // For detecting pinch press/release
    bool rightPinchReady;
    
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
    // Controllers are now represented by synthetic hands in DrawHands()
    // This function now only draws pointer rays when triggers are pressed
    
    VRController leftController = GetController(CONTROLLER_LEFT);
    VRController rightController = GetController(CONTROLLER_RIGHT);
    
    // Draw pointer ray for left controller when trigger pressed
    if (leftController.isTracking && leftController.trigger > 0.1f) {
        Vector3 forward = QuaternionForward(leftController.orientation);
        Vector3 rayEnd = Vector3Add(leftController.position, 
            Vector3Scale(forward, -2.0f * leftController.trigger));
        DrawVRLine3D(leftController.position, rayEnd, SKYBLUE);
    }
    
    // Draw pointer ray for right controller when trigger pressed
    if (rightController.isTracking && rightController.trigger > 0.1f) {
        Vector3 forward = QuaternionForward(rightController.orientation);
        Vector3 rayEnd = Vector3Add(rightController.position, 
            Vector3Scale(forward, -2.0f * rightController.trigger));
        DrawVRLine3D(rightController.position, rayEnd, LIME);
    }
}

// =============================================================================
// Hand Tracking Visualization
// =============================================================================

// Draw a synthetic hand based on controller pose and input
static void DrawControllerHand(ControllerHand hand, VRController controller, Color color) {
    if (!controller.isTracking) return;
    
    Vector3 pos = controller.position;
    Quaternion ori = controller.orientation;
    
    // Get direction vectors from controller orientation
    Vector3 forward = QuaternionForward(ori);
    Vector3 right = QuaternionRight(ori);
    Vector3 up = QuaternionUp(ori);
    
    // Mirror for left hand
    float handSign = (hand == CONTROLLER_LEFT) ? -1.0f : 1.0f;
    
    // Wrist position (slightly behind controller)
    Vector3 wrist = Vector3Add(pos, Vector3Scale(forward, 0.05f));
    
    // Palm position
    Vector3 palm = Vector3Add(pos, Vector3Scale(forward, -0.02f));
    
    // Draw wrist to palm
    DrawVRLine3D(wrist, palm, color);
    DrawVRSphere(wrist, 0.012f, color);
    DrawVRSphere(palm, 0.015f, color);
    
    // Finger base positions (spread across palm)
    float fingerSpread = 0.015f;
    Vector3 fingerBases[5];
    fingerBases[0] = Vector3Add(palm, Vector3Scale(right, handSign * 0.025f));  // Thumb
    fingerBases[1] = Vector3Add(palm, Vector3Scale(right, handSign * 0.015f));  // Index
    fingerBases[2] = palm;  // Middle
    fingerBases[3] = Vector3Add(palm, Vector3Scale(right, handSign * -0.012f)); // Ring
    fingerBases[4] = Vector3Add(palm, Vector3Scale(right, handSign * -0.022f)); // Little
    
    // Calculate finger curl based on grip and trigger
    float indexCurl = controller.trigger;  // Index finger follows trigger
    float otherCurl = controller.grip;     // Other fingers follow grip
    float thumbCurl = (controller.trigger > 0.5f || controller.grip > 0.5f) ? 0.5f : 0.0f;
    
    // Finger lengths
    float fingerLengths[5] = {0.03f, 0.045f, 0.05f, 0.045f, 0.035f};
    float curls[5] = {thumbCurl, indexCurl, otherCurl, otherCurl, otherCurl};
    
    // Draw each finger
    for (int f = 0; f < 5; f++) {
        Vector3 base = fingerBases[f];
        float len = fingerLengths[f];
        float curl = curls[f];
        
        // Finger direction (forward when open, curled toward palm when closed)
        Vector3 fingerDir;
        if (f == 0) {
            // Thumb points outward and forward
            fingerDir = Vector3Add(
                Vector3Scale(forward, -0.7f * (1.0f - curl)),
                Vector3Scale(right, handSign * 0.7f * (1.0f - curl * 0.5f))
            );
            fingerDir = Vector3Add(fingerDir, Vector3Scale(up, -curl * 0.5f));
        } else {
            // Other fingers point forward when open, curl down when closed
            fingerDir = Vector3Add(
                Vector3Scale(forward, -(1.0f - curl * 0.8f)),
                Vector3Scale(up, -curl * 0.6f)
            );
        }
        fingerDir = Vector3Normalize(fingerDir);
        
        // Draw finger segments
        Vector3 mid = Vector3Add(base, Vector3Scale(fingerDir, len * 0.5f));
        Vector3 tip = Vector3Add(base, Vector3Scale(fingerDir, len));
        
        // Add some curl to the tip segment
        if (curl > 0.3f) {
            Vector3 curlDir = Vector3Scale(up, -curl * 0.02f);
            tip = Vector3Add(tip, curlDir);
        }
        
        DrawVRLine3D(base, mid, color);
        DrawVRLine3D(mid, tip, color);
        DrawVRSphere(tip, 0.008f, WHITE);
    }
}

static void DrawHands(void) {
    VRController leftController = GetController(CONTROLLER_LEFT);
    VRController rightController = GetController(CONTROLLER_RIGHT);
    
    // Draw left hand
    bool leftHandTracked = world.handTrackingEnabled && IsHandTracked(CONTROLLER_LEFT);
    
    if (leftHandTracked) {
        // Real hand tracking - draw skeleton
        VRHand leftHand = GetLeftHand();
        
        DrawHandSkeleton(CONTROLLER_LEFT, SKYBLUE);
        DrawVRSphere(GetIndexTip(CONTROLLER_LEFT), 0.01f, WHITE);
        DrawVRSphere(GetThumbTip(CONTROLLER_LEFT), 0.01f, WHITE);
        
        if (leftHand.isPinching) {
            Vector3 pinchPos = GetPinchPosition(CONTROLLER_LEFT);
            DrawVRSphere(pinchPos, 0.02f, YELLOW);
        }
        
        if (leftHand.isPointing) {
            Vector3 palmPos = GetPalmPosition(CONTROLLER_LEFT);
            Vector3 pointDir = GetPointingDirection(CONTROLLER_LEFT);
            Vector3 rayEnd = Vector3Add(palmPos, Vector3Scale(pointDir, 1.0f));
            DrawVRLine3D(palmPos, rayEnd, MAGENTA);
        }
        
        if (leftHand.isFist) {
            Vector3 palmPos = GetPalmPosition(CONTROLLER_LEFT);
            DrawVRSphere(palmPos, 0.03f, RED);
        }
    } else if (leftController.isTracking) {
        // Controller mode - draw synthetic hand
        DrawControllerHand(CONTROLLER_LEFT, leftController, SKYBLUE);
    }
    
    // Draw right hand
    bool rightHandTracked = world.handTrackingEnabled && IsHandTracked(CONTROLLER_RIGHT);
    
    if (rightHandTracked) {
        // Real hand tracking - draw skeleton
        VRHand rightHand = GetRightHand();
        
        DrawHandSkeleton(CONTROLLER_RIGHT, LIME);
        DrawVRSphere(GetIndexTip(CONTROLLER_RIGHT), 0.01f, WHITE);
        DrawVRSphere(GetThumbTip(CONTROLLER_RIGHT), 0.01f, WHITE);
        
        if (rightHand.isPinching) {
            Vector3 pinchPos = GetPinchPosition(CONTROLLER_RIGHT);
            DrawVRSphere(pinchPos, 0.02f, YELLOW);
        }
        
        if (rightHand.isPointing) {
            Vector3 palmPos = GetPalmPosition(CONTROLLER_RIGHT);
            Vector3 pointDir = GetPointingDirection(CONTROLLER_RIGHT);
            Vector3 rayEnd = Vector3Add(palmPos, Vector3Scale(pointDir, 1.0f));
            DrawVRLine3D(palmPos, rayEnd, MAGENTA);
        }
        
        if (rightHand.isFist) {
            Vector3 palmPos = GetPalmPosition(CONTROLLER_RIGHT);
            DrawVRSphere(palmPos, 0.03f, RED);
        }
    } else if (rightController.isTracking) {
        // Controller mode - draw synthetic hand
        DrawControllerHand(CONTROLLER_RIGHT, rightController, LIME);
    }
}

// =============================================================================
// Hand Tracking Input
// =============================================================================

static void HandleHandInput(void) {
    if (!world.handTrackingEnabled) return;
    
    // =========================================================================
    // RIGHT HAND PINCH: Spawn a cube at pinch position
    // =========================================================================
    if (IsHandTracked(CONTROLLER_RIGHT)) {
        bool isPinching = IsHandPinching(CONTROLLER_RIGHT);
        
        if (isPinching && world.rightPinchReady) {
            // Spawn a cube at the pinch position
            int cubeIndex = (int)(RandomFloat() * NUM_FLOATING_CUBES);
            
            // Get pinch position in hand tracking space
            Vector3 pinchPos = GetPinchPosition(CONTROLLER_RIGHT);
            
            // Transform to world space (account for player position/rotation)
            Vector3 playerPos = GetPlayerPosition();
            float playerYaw = GetPlayerYaw() * PI / 180.0f;
            
            Vector3 worldPos = {
                pinchPos.x * cosf(playerYaw) - pinchPos.z * sinf(playerYaw) + playerPos.x,
                pinchPos.y + playerPos.y,
                pinchPos.x * sinf(playerYaw) + pinchPos.z * cosf(playerYaw) + playerPos.z
            };
            
            world.cubePositions[cubeIndex] = worldPos;
            world.cubeColors[cubeIndex] = RandomColor();
            world.rightPinchReady = false;
            
            LOGI("Pinch spawn! Cube at (%.2f, %.2f, %.2f)", worldPos.x, worldPos.y, worldPos.z);
        }
        
        if (!isPinching) {
            world.rightPinchReady = true;
        }
    }
    
    // =========================================================================
    // LEFT HAND FIST: Teleport to origin
    // =========================================================================
    static bool fistReady = true;
    if (IsHandTracked(CONTROLLER_LEFT)) {
        bool isFist = IsHandFist(CONTROLLER_LEFT);
        
        if (isFist && fistReady) {
            SetPlayerPosition(Vector3Create(0.0f, 0.0f, 0.0f));
            SetPlayerYaw(0.0f);
            world.playerVelocityY = 0.0f;
            world.isGrounded = true;
            fistReady = false;
            LOGI("Fist teleport to origin");
        }
        
        if (!isFist) {
            fistReady = true;
        }
    }
    
    // =========================================================================
    // LEFT HAND PINCH: Move forward in pointing direction
    // =========================================================================
    if (IsHandTracked(CONTROLLER_LEFT)) {
        if (IsHandPinching(CONTROLLER_LEFT) && IsHandPointing(CONTROLLER_LEFT)) {
            Vector3 pointDir = GetPointingDirection(CONTROLLER_LEFT);
            Vector3 playerPos = GetPlayerPosition();
            
            // Move in pointing direction
            playerPos.x += pointDir.x * MOVE_SPEED * 0.5f * world.deltaTime;
            playerPos.z += pointDir.z * MOVE_SPEED * 0.5f * world.deltaTime;
            
            SetPlayerPosition(playerPos);
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
    
    // Update hand tracking (if enabled)
    if (world.handTrackingEnabled) {
        UpdateHandTracking();
    }
    
    // Handle controller input
    HandleInput();
    
    // Handle hand tracking input (gestures)
    HandleHandInput();
    
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
    
    // Draw controllers (hidden when using hands)
    DrawControllers();
    
    // Draw tracked hands
    DrawHands();
    
    // Debug: Log player position and hand tracking status occasionally
    static int frameCount = 0;
    if (++frameCount % 500 == 0) {
        Vector3 pos = GetPlayerPosition();
        LOGI("Player pos: (%.2f, %.2f, %.2f) Yaw: %.1f Grounded: %s",
             pos.x, pos.y, pos.z, GetPlayerYaw(), world.isGrounded ? "yes" : "no");
        
        if (world.handTrackingEnabled) {
            bool leftTracked = IsHandTracked(CONTROLLER_LEFT);
            bool rightTracked = IsHandTracked(CONTROLLER_RIGHT);
            LOGI("Hand tracking - Left: %s, Right: %s", 
                 leftTracked ? "tracked" : "not tracked",
                 rightTracked ? "tracked" : "not tracked");
        }
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
    
    // Initialize hand tracking (optional - will gracefully fail if not supported)
    if (InitHandTracking()) {
        world.handTrackingEnabled = true;
        world.leftPinchReady = true;
        world.rightPinchReady = true;
        LOGI("Hand tracking initialized successfully!");
    } else {
        world.handTrackingEnabled = false;
        LOGI("Hand tracking not available - using controllers only");
    }
    
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
    
    // Cleanup hand tracking
    if (world.handTrackingEnabled) {
        ShutdownHandTracking();
    }
    
    // Cleanup VR
    LOGI("Shutting down VR Application...");
    CloseApp(app);
    LOGI("VR Application Closed");
}
