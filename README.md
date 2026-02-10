# RealityLib - Simple VR Framework for Meta Quest

A lightweight VR framework that makes it easy to create VR applications for Meta Quest devices. Built on OpenXR, RealityLib provides a simple API where you only need to modify `main.c` to create your VR experience.

## Features

- **Simple API** - Just modify `main.c` to create your VR game
- **OpenXR Native** - Direct OpenXR implementation for best performance
- **Quest Optimized** - Built specifically for Meta Quest 2/3/Pro
- **Full Controller Support** - Triggers, grips, thumbsticks, buttons, and haptics
- **Hand Tracking** - Full skeletal hand tracking with gesture detection (pinch, fist, point)
- **Minimal Dependencies** - Only requires Android NDK and OpenXR loader

## Quick Start

### Prerequisites

1. **Android SDK** with API level 26+
2. **Android NDK** version 29.x (r29)
3. **OpenXR Loader** from Meta Quest SDK
4. **ADB** for deployment
5. **Meta Quest** device with Developer Mode enabled

### Setup on Mac

#### 1. Install Android SDK/NDK

If you haven't already, install Android Studio or the command-line tools:

```bash
# Using Homebrew
brew install --cask android-studio

# Or download from: https://developer.android.com/studio
```

Ensure these environment variables are set in your `~/.zshrc` or `~/.bashrc`:

```bash
export ANDROID_HOME="$HOME/Library/Android/sdk"
export ANDROID_SDK_ROOT="$ANDROID_HOME"
export PATH="$PATH:$ANDROID_HOME/platform-tools:$ANDROID_HOME/cmdline-tools/latest/bin"
```

Install NDK r29:
```bash
sdkmanager "ndk;29.0.14206865"
```

#### 2. Download OpenXR Loader

The OpenXR loader for Meta Quest must be downloaded from Meta:

1. Go to: https://developer.oculus.com/downloads/package/oculus-openxr-mobile-sdk/
2. Download the latest version (e.g., `ovr-openxr-mobile-sdk-72.0.zip`)
3. Extract and copy the loader:

```bash
# After extracting the SDK
cp /path/to/OpenXR/Libs/Android/arm64-v8a/Release/libopenxr_loader.so \
   app/src/main/deps/OpenXR-SDK/libs/arm64-v8a/
```

#### 3. Build the Project

```bash
# Clean and build
./gradlew clean assembleDebug

# Or for release
./gradlew assembleRelease
```

#### 4. Deploy to Quest

1. Connect your Quest via USB
2. Enable Developer Mode on Quest (Settings → System → Developer)
3. Allow USB debugging when prompted on the headset

```bash
# Check device connection
adb devices

# Install the app
./gradlew installDebug

# Or manually
adb install -r app/build/outputs/apk/debug/app-debug.apk
```

#### 5. Run the App

The app will appear in your Quest's app library under "Unknown Sources". Put on your headset and launch "RealityLib"!

## Project Structure

```
RealityLibTest/
├── app/
│   ├── src/main/
│   │   ├── main.c              # ← YOUR GAME CODE GOES HERE
│   │   ├── realitylib_vr.h     # VR API header (includes hand tracking)
│   │   ├── realitylib_vr.c     # VR implementation (OpenXR)
│   │   ├── realitylib_hands.h  # Hand tracking API header
│   │   ├── realitylib_hands.c  # Hand tracking implementation
│   │   ├── CMakeLists.txt      # Build configuration
│   │   ├── AndroidManifest.xml # Android configuration
│   │   └── deps/
│   │       └── OpenXR-SDK/
│   │           ├── include/    # OpenXR headers (auto-downloaded)
│   │           └── libs/       # OpenXR loader (manual download)
│   └── build.gradle
├── scripts/
│   └── setup_deps.sh           # Dependency setup script
├── build.gradle
├── settings.gradle
└── README.md
```

## API Reference

### Core Functions

```c
// Initialize VR system - call once at start
bool InitApp(struct android_app* app);

// Check if app should exit
bool AppShouldClose(struct android_app* app);

// Begin/End VR rendering frame
void BeginVRMode(void);
void EndVRMode(void);

// Sync controller input
void SyncControllers(void);

// Cleanup - call before exit
void CloseApp(struct android_app* app);
```

### Drawing Functions

```c
// Draw a cube
void DrawVRCube(Vector3 position, float size, Color color);

// Draw a cuboid (box with different dimensions)
void DrawVRCuboid(Vector3 position, Vector3 size, Vector3 color);

// Draw a grid on the ground
void DrawVRGrid(int slices, float spacing);

// Draw a line
void DrawVRLine3D(Vector3 startPos, Vector3 endPos, Color color);

// Draw coordinate axes
void DrawVRAxes(Vector3 position, float scale);

// Draw a flat plane
void DrawVRPlane(Vector3 centerPos, Vector3 size, Color color);
```

### Input Functions

```c
// Get controller state
VRController GetController(ControllerHand hand);  // CONTROLLER_LEFT or CONTROLLER_RIGHT

// Get headset state
VRHeadset GetHeadset(void);

// Convenience functions
Vector3 GetVRControllerPosition(int hand);
float GetVRControllerTrigger(int hand);
float GetVRControllerGrip(int hand);
Vector3 GetVRControllerThumbstick(int hand);

// Haptic feedback
void TriggerVRHaptic(int hand, float amplitude, float duration);
```

### Hand Tracking Functions

```c
// Initialize and shutdown
bool InitHandTracking(void);       // Call after InitApp()
void ShutdownHandTracking(void);   // Call before CloseApp()
bool IsHandTrackingAvailable(void);
bool IsHandTrackingActive(void);

// Per-frame update
void UpdateHandTracking(void);     // Call each frame

// Get hand data
VRHand GetHand(ControllerHand hand);
VRHand GetLeftHand(void);
VRHand GetRightHand(void);
bool IsHandTracked(ControllerHand hand);

// Get joint positions (26 joints per hand)
Vector3 GetHandJointPosition(ControllerHand hand, HandJoint joint);
Vector3 GetThumbTip(ControllerHand hand);
Vector3 GetIndexTip(ControllerHand hand);
Vector3 GetPalmPosition(ControllerHand hand);
Vector3 GetWristPosition(ControllerHand hand);

// Gesture detection
bool IsHandPinching(ControllerHand hand);
float GetPinchStrength(ControllerHand hand);  // 0.0 to 1.0
Vector3 GetPinchPosition(ControllerHand hand);
bool IsHandFist(ControllerHand hand);
bool IsHandPointing(ControllerHand hand);
Vector3 GetPointingDirection(ControllerHand hand);
bool IsHandOpen(ControllerHand hand);

// Visualization helpers
void DrawHandSkeleton(ControllerHand hand, Color color);
void DrawHandJoints(ControllerHand hand, Color color);
```

### Hand Joint Indices

```c
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
    // ... (26 total joints per hand)
    HAND_JOINT_COUNT = 26
} HandJoint;
```

### Types

```c
typedef struct VRController {
    Vector3 position;
    Quaternion orientation;
    float trigger;          // 0.0 - 1.0
    float grip;             // 0.0 - 1.0
    float thumbstickX;      // -1.0 to 1.0
    float thumbstickY;      // -1.0 to 1.0
    bool thumbstickClick;
    bool buttonA;           // A (right) / X (left)
    bool buttonB;           // B (right) / Y (left)
    bool isTracking;
} VRController;

typedef struct VRHeadset {
    Vector3 position;
    Quaternion orientation;
    int displayWidth;
    int displayHeight;
} VRHeadset;
```

## Example: Simple VR World

Here's a minimal example that creates a VR world with floating cubes:

```c
#include "realitylib_vr.h"

// Called every frame
void inLoop(struct android_app* app) {
    // Draw a floor grid
    DrawVRGrid(20, 1.0f);
    
    // Draw some floating cubes
    for (int i = 0; i < 10; i++) {
        Vector3 pos = {i * 0.5f - 2.5f, 1.5f, -3.0f};
        Color color = {255, 50 + i * 20, 50, 255};
        DrawVRCube(pos, 0.2f, color);
    }
    
    // Draw controllers
    VRController left = GetController(CONTROLLER_LEFT);
    VRController right = GetController(CONTROLLER_RIGHT);
    
    if (left.isTracking) {
        DrawVRCube(left.position, 0.05f, BLUE);
    }
    if (right.isTracking) {
        DrawVRCube(right.position, 0.05f, GREEN);
    }
}

void android_main(struct android_app* app) {
    InitApp(app);
    
    while (!AppShouldClose(app)) {
        BeginVRMode();
        SyncControllers();
        inLoop(app);
        EndVRMode();
    }
    
    CloseApp(app);
}
```

## Example: Hand Tracking

Here's an example showing how to use hand tracking:

```c
#include "realitylib_vr.h"

bool handTrackingEnabled = false;

void inLoop(struct android_app* app) {
    // Update hand tracking
    if (handTrackingEnabled) {
        UpdateHandTracking();
    }
    
    // Draw floor
    DrawVRGrid(20, 1.0f);
    
    // Draw hands if tracking
    if (handTrackingEnabled) {
        // Draw left hand skeleton (cyan)
        if (IsHandTracked(CONTROLLER_LEFT)) {
            DrawHandSkeleton(CONTROLLER_LEFT, SKYBLUE);
            
            // Show pinch indicator
            if (IsHandPinching(CONTROLLER_LEFT)) {
                Vector3 pinchPos = GetPinchPosition(CONTROLLER_LEFT);
                DrawVRSphere(pinchPos, 0.02f, YELLOW);
            }
        }
        
        // Draw right hand skeleton (green)
        if (IsHandTracked(CONTROLLER_RIGHT)) {
            DrawHandSkeleton(CONTROLLER_RIGHT, LIME);
            
            // Spawn cube when pinching
            static bool pinchReady = true;
            if (IsHandPinching(CONTROLLER_RIGHT) && pinchReady) {
                Vector3 pos = GetPinchPosition(CONTROLLER_RIGHT);
                // ... spawn cube at pos
                pinchReady = false;
            }
            if (!IsHandPinching(CONTROLLER_RIGHT)) {
                pinchReady = true;
            }
        }
    }
}

void android_main(struct android_app* app) {
    InitApp(app);
    
    // Initialize hand tracking (optional - gracefully fails if unavailable)
    handTrackingEnabled = InitHandTracking();
    
    while (!AppShouldClose(app)) {
        BeginVRMode();
        SyncControllers();
        inLoop(app);
        EndVRMode();
    }
    
    if (handTrackingEnabled) {
        ShutdownHandTracking();
    }
    CloseApp(app);
}
```

## VSCode / Editor Setup (fixing "missing header" errors)

If you open this project in **VSCode** (or Cursor) and see red squiggles for missing headers like `<android/log.h>`, `<openxr/openxr.h>`, or `<android_native_app_glue.h>`, do the following:

1. **Set the Android NDK path** so the C/C++ extension can find system headers:
   - **Windows:** Set environment variable `ANDROID_NDK_HOME` to your NDK folder, e.g.  
     `C:\Users\<You>\AppData\Local\Android\Sdk\ndk\29.0.14206865`
   - **Mac/Linux:** In `~/.zshrc` or `~/.bashrc`:  
     `export ANDROID_NDK_HOME="$ANDROID_HOME/ndk/29.0.14206865"`  
     (or the path where your NDK is installed)
   - **Can’t find the NDK?** See [Where is the NDK?](#where-is-the-ndk-finding-android_ndk_home) below for default paths and how to find it (e.g. via Android Studio SDK settings or `local.properties`).
   - Restart VSCode after changing the variable.

2. **OpenXR headers:** Run the dependency setup script so OpenXR headers are present:
   - `./scripts/setup_deps.sh` (Mac/Linux)  
   - Or manually place OpenXR headers under `app/src/main/deps/OpenXR-SDK/include/openxr/` (see script for URLs).

3. **Select the right configuration:** In VSCode, open the Command Palette (`Ctrl+Shift+P` / `Cmd+Shift+P`), run **C/C++: Select a Configuration**, and choose **Win32**, **Linux**, or **Mac** to match your OS.

The project includes a `.vscode/c_cpp_properties.json` that points IntelliSense to the project sources, OpenXR includes, and NDK sysroot when `ANDROID_NDK_HOME` is set.

## Troubleshooting

### Where is the NDK? (finding ANDROID_NDK_HOME)

The **Android NDK** is installed *inside* your Android SDK, in an `ndk` folder. This project needs **NDK version 29.0.14206865**.

**Default locations** (replace `<YourUsername>` with your login name):

| OS      | Typical NDK path |
|---------|-------------------|
| **Windows** | `C:\Users\<YourUsername>\AppData\Local\Android\Sdk\ndk\29.0.14206865` |
| **Mac**     | `~/Library/Android/sdk/ndk/29.0.14206865` or `$HOME/Library/Android/sdk/ndk/29.0.14206865` |
| **Linux**   | `~/Android/Sdk/ndk/29.0.14206865` or `$ANDROID_HOME/ndk/29.0.14206865` |

**How to find your SDK (and then the NDK):**

1. **If you use Android Studio:**  
   - Open **File → Settings** (or **Android Studio → Preferences** on Mac).  
   - Go to **Languages & Frameworks → Android SDK**.  
   - The top line shows **Android SDK Location** (e.g. `C:\Users\...\AppData\Local\Android\Sdk`).  
   - The NDK is the folder: **`<that path>\ndk\29.0.14206865`**.

2. **If the project builds with Gradle:**  
   - After a successful build, check **`local.properties`** in the project root (it’s generated and often in `.gitignore`).  
   - It contains a line like `sdk.dir=C\:\\Users\\...\\AppData\\Local\\Android\\Sdk`.  
   - Your NDK path is: **`<that path>\ndk\29.0.14206865`** (use forward slashes or escaped backslashes as required).

3. **If the NDK folder doesn’t exist:**  
   - Install NDK **29.0.14206865** via Android Studio: **Settings → Android SDK → SDK Tools** tab → check **NDK (Side by side)** and the version **29.0.14206865**, then Apply.  
   - Or from the command line (with `sdkmanager` on your PATH):  
     `sdkmanager "ndk;29.0.14206865"`

**Set it for VSCode:**  
Set the environment variable `ANDROID_NDK_HOME` to that full path (e.g. the path ending in `ndk\29.0.14206865`), then restart VSCode. See [VSCode / Editor Setup](#vscode--editor-setup-fixing-missing-header-errors) above.

### Build Errors

**"OpenXR headers not found"**
- Run `./scripts/setup_deps.sh` or manually download headers from Khronos

**"OpenXR loader not found"**
- Download the Meta OpenXR Mobile SDK and copy `libopenxr_loader.so` to `deps/OpenXR-SDK/libs/arm64-v8a/`

**"NDK not found"**
- Install NDK r29: `sdkmanager "ndk;29.0.14206865"`
- Then set `ANDROID_NDK_HOME` to the path in [Where is the NDK?](#where-is-the-ndk-finding-android_ndk_home) above (for builds, Gradle uses the SDK from Android Studio or `local.properties`; the env var is mainly for the editor).

### Runtime Errors

**App crashes immediately**
- Check `adb logcat | grep RealityLib` for error messages
- Ensure OpenXR loader is properly installed

**No display in headset**
- Verify Developer Mode is enabled
- Check that the app is launched from "Unknown Sources"

**Controllers not working**
- Make sure controllers are paired and tracking
- Check battery level on controllers

**Hand tracking not working**
- Ensure hand tracking is enabled in Quest settings (Settings → Movement Tracking → Hand & Body Tracking)
- Put down controllers - Quest will automatically switch to hand tracking
- Check logs with `adb logcat | grep RealityLib_Hands` for error messages
- Hand tracking requires good lighting conditions

## Contributing

Contributions are welcome! Feel free to submit issues and pull requests.

## License

MIT License - see LICENSE file for details.

## Credits

- Built with OpenXR from Khronos Group
- Meta Quest runtime and SDK
- Inspired by raylib's simplicity
