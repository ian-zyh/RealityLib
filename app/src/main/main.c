#include <raylib.h>
#include <android/log.h>
#include <android_native_app_glue.h>

#define LOG_TAG "RealityLib"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// Forward declarations of VR functions (these should be in your framework)
void InitApp(struct android_app* app);
void CloseApp(struct android_app* app);
bool AppShouldClose(struct android_app* app);
void BeginVRMode(void);
void EndVRMode(void);
void SyncControllers(void);
void DrawVRCuboid(Vector3 position, Vector3 size, Vector3 color);
void inLoop(struct android_app* app);

// Global variables
float speed = 0.1f;
Vector3 selfLoc = {0.0f, 0.0f, 0.0f};

/**
 * Main entry point for the native application
 */
void android_main(struct android_app* app) {
    LOGD("RealityLib Starting...");
    
    // Initialize the VR application
    InitApp(app);
    LOGD("App Initialized");
    
    // Main game loop
    while(!AppShouldClose(app)) {
        // Begin VR rendering for this frame
        BeginVRMode();
        
        // Sync controller data
        SyncControllers();
        
        // Call custom loop logic
        inLoop(app);
        
        // Draw 20 cuboids in a line
        for(int i = 0; i < 20; i++) {
            Vector3 position = {i * 0.2f, 0.0f, -1.0f};
            Vector3 size = {0.1f, 0.1f, 0.1f};
            Vector3 color = {1.0f, 0.05f * i, 0.02f * i};
            
            DrawVRCuboid(position, size, color);
        }
        
        // End VR rendering for this frame
        EndVRMode();
    }
    
    // Cleanup
    LOGD("Closing App");
    CloseApp(app);
}