#include <raylib.h>
#include <android/log.h>

#define LOG_TAG "InteractiveScene"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

// Scene state
typedef struct {
    Vector3 cubePositions[10];
    Color cubeColors[10];
    bool cubeGrabbed[10];
    int grabbedCubeIndex;
} SceneState;

static SceneState scene = {0};

void InitScene(void) {
    // Initialize cube positions in a grid
    for (int i = 0; i < 10; i++) {
        scene.cubePositions[i] = (Vector3){
            (i % 5) * 0.3f - 0.6f,
            1.5f,
            (i / 5) * 0.3f - 2.0f
        };
        scene.cubeColors[i] = (Color){
            (unsigned char)(rand() % 256),
            (unsigned char)(rand() % 256),
            (unsigned char)(rand() % 256),
            255
        };
        scene.cubeGrabbed[i] = false;
    }
    scene.grabbedCubeIndex = -1;
}

void UpdateScene(void) {
    // Get controller data (implement these in your VR framework)
    Vector3 rightControllerPos = GetVRControllerPosition(1);
    bool gripPressed = GetVRControllerGrip(1) > 0.5f;
    
    if (gripPressed && scene.grabbedCubeIndex == -1) {
        // Check if grabbing any cube
        for (int i = 0; i < 10; i++) {
            float distance = Vector3Distance(rightControllerPos, scene.cubePositions[i]);
            if (distance < 0.2f) {
                scene.grabbedCubeIndex = i;
                scene.cubeGrabbed[i] = true;
                TriggerVRHaptic(1, 0.5f, 0.1f);
                break;
            }
        }
    } else if (!gripPressed && scene.grabbedCubeIndex != -1) {
        // Release grabbed cube
        scene.cubeGrabbed[scene.grabbedCubeIndex] = false;
        scene.grabbedCubeIndex = -1;
    }
    
    // Update grabbed cube position
    if (scene.grabbedCubeIndex != -1) {
        scene.cubePositions[scene.grabbedCubeIndex] = rightControllerPos;
    }
}

void DrawScene(void) {
    // Draw floor grid
    DrawGrid(10, 1.0f);
    
    // Draw all cubes
    for (int i = 0; i < 10; i++) {
        Vector3 size = scene.cubeGrabbed[i] ? 
            (Vector3){0.12f, 0.12f, 0.12f} : 
            (Vector3){0.1f, 0.1f, 0.1f};
        
        DrawVRCuboid(scene.cubePositions[i], size, 
            (Vector3){
                scene.cubeColors[i].r / 255.0f,
                scene.cubeColors[i].g / 255.0f,
                scene.cubeColors[i].b / 255.0f
            });
    }
    
    // Draw controllers
    Vector3 leftPos = GetVRControllerPosition(0);
    Vector3 rightPos = GetVRControllerPosition(1);
    
    DrawVRCuboid(leftPos, (Vector3){0.05f, 0.05f, 0.05f}, 
        (Vector3){0.0f, 0.0f, 1.0f});
    DrawVRCuboid(rightPos, (Vector3){0.05f, 0.05f, 0.05f}, 
        (Vector3){0.0f, 1.0f, 0.0f});
}