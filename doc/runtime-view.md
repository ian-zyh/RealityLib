# RealityLib Runtime View (Component-and-Connector)

This document captures a **Component-and-Connector (C&C) view** of the current RealityLib codebase: the runtime elements that exist while the app is running on a Meta Quest device, and how those elements interact.

## What A C&C View Shows

- **Components**: principal runtime processing units and data stores (processes, modules with runtime state, objects, etc.).
- **Connectors**: interaction pathways between components (function calls, event queues, API protocols, shared state access, graphics pipelines).
- **Relations (Attachments)**: which components are connected by which connectors.
- **Constraints**:
  - Components attach only to connectors; connectors attach only to components.
  - Connectors do not appear in isolation; each connector must attach to at least one component.
  - Attachments should only connect compatible components/connectors (e.g., OpenXR calls require a valid `XrInstance`/`XrSession`).

## Scope

- Code paths documented here are primarily in:
  - `app/src/main/main.c` (game/app entrypoint + per-frame user loop)
  - `app/src/main/realitylib_vr.c` / `app/src/main/realitylib_vr.h` (OpenXR + EGL/GLES integration, input, rendering)
  - `app/src/main/realitylib_hands.c` / `app/src/main/realitylib_hands.h` (XR_EXT_hand_tracking support)
  - `app/src/main/AndroidManifest.xml` (NativeActivity entry configuration)
  - `app/src/main/CMakeLists.txt` (native linkage: EGL/GLES + OpenXR loader)

## Primary Runtime Context

- The application runs as an **Android `NativeActivity`** process which loads `librealitylib.so` (`android.app.lib_name` in `app/src/main/AndroidManifest.xml`).
- RealityLib integrates:
  - **EGL + OpenGL ES 3** for rendering (`InitializeEGL()` and GL calls in `app/src/main/realitylib_vr.c`)
  - **OpenXR** for tracking, swapchains, session lifecycle, input, and haptics (`InitializeOpenXR()`, `CreateSession()`, actions, swapchains)
  - **Optional XR_EXT_hand_tracking** for skeletal hands (`app/src/main/realitylib_hands.c`)

## C&C Diagram (High-Level)

```mermaid
flowchart LR
  subgraph P[Android app process (NativeActivity)]
    A[App/Game Loop\napp/src/main/main.c\nandroid_main() + inLoop()]
    V[RealityLib Core VR Runtime\napp/src/main/realitylib_vr.c\nVRState (global)]
    H[Hand Tracking Module\napp/src/main/realitylib_hands.c\nHandTrackingState (global)]
    R[Deferred Renderer\nDraw Command Buffer + GLES\n(drawCommands + RenderEye)]
  end

  subgraph S[Quest system / platform]
    L[OpenXR Loader\nlibopenxr_loader.so]
    X[OpenXR Runtime\n(system implementation)]
    O[Android/Quest OS\n(event loop, process lifecycle)]
    G[GPU / Display]
    D[HMD sensors + Controllers/Hands\n+ Haptics]
  end

  A -- "C API calls:\nInitApp/BeginVRMode/SyncControllers/\nDraw* / EndVRMode" --> V
  A -- "C API calls:\nInitHandTracking/UpdateHandTracking/\nGetHand/IsHandTracked" --> H
  H -- "shared-state accessors:\nGetXrInstance/GetXrSession/\nGetXrStageSpace/GetPredictedDisplayTime" --> V

  V -- "records draw commands" --> R
  R -- "GLES rendering\ninto swapchain images" --> G

  V -- "OpenXR API:\nxr* (session, swapchains, actions,\nlocate, frame submit)" --> L --> X
  H -- "OpenXR extension API:\nxrCreateHandTrackerEXT/\nxrLocateHandJointsEXT" --> X

  V -- "Android looper:\nALooper_pollOnce()" --> O
  X -- "tracking + composition + input + haptics" --> D
```

## Components (Runtime Elements)

### 1) App/Game Loop Component

- **Location**: `app/src/main/main.c`
- **Runtime role**:
  - Entry point: `android_main(struct android_app* app)`
  - Implements `inLoop(struct android_app* app)` called once per frame.
  - Owns game state (e.g., `world` struct) and issues draw/input calls through RealityLib.

### 2) RealityLib Core VR Runtime Component

- **Location**: `app/src/main/realitylib_vr.c`
- **Runtime state**: `static VRState vrState`
  - Android handles: `struct android_app* app`
  - EGL/GLES objects: `EGLDisplay`, `EGLContext`, swapchain FBO/depth buffers
  - OpenXR objects: `XrInstance`, `XrSession`, `XrSpace` (stage/head/hand), swapchains
  - Input/action objects: `XrActionSet`, `XrAction*`, hand paths
  - Current tracked state exposed to the app: `VRController controllers[2]`, `VRHeadset headset`
  - Frame timing: `XrTime predictedDisplayTime`
  - Session lifecycle flags: `sessionRunning`, `sessionState`, `shouldExit`, etc.
- **Responsibilities**:
  - Initialize/teardown EGL + OpenXR (`InitApp()`, `CloseApp()`)
  - Poll Android + XR events (`AppShouldClose()` calls `ALooper_pollOnce()` + `PollXREvents()`)
  - Frame pacing and submission (`BeginVRMode()` / `EndVRMode()` uses `xrWaitFrame`, `xrBeginFrame`, `xrEndFrame`)
  - Input sync (`SyncControllers()` -> `UpdateInput()` -> `xrSyncActions`, `xrLocateSpace`, `xrGetActionState*`)
  - Haptics (`TriggerVRHaptic()` -> `xrApplyHapticFeedback`)

### 3) Hand Tracking Component (Optional)

- **Location**: `app/src/main/realitylib_hands.c`
- **Runtime state**: `static HandTrackingState htState`
  - Availability/initialization flags
  - `XrHandTrackerEXT handTracker[2]`
  - Joint locations/velocities buffers
  - Derived `VRHand hands[2]` including simple gesture detection
  - Dynamically loaded function pointers: `xrCreateHandTrackerEXT`, `xrLocateHandJointsEXT`, ...
- **Responsibilities**:
  - Check for `XR_EXT_hand_tracking` and initialize trackers (`InitHandTracking()`)
  - Per-frame update of joints (`UpdateHandTracking()` -> `xrLocateHandJointsEXT`)
  - Provide gesture-level queries (pinch/fist/point/open) derived from joints
- **Dependency**:
  - Reads core VR state via accessors exported from `app/src/main/realitylib_vr.c`
    (`GetXrInstance()`, `GetXrSession()`, `GetXrStageSpace()`, `GetPredictedDisplayTime()`, `IsVRSessionRunning()`).

### 4) Deferred Renderer Component

- **Location**: `app/src/main/realitylib_vr.c`
- **Runtime state**:
  - `static DrawCommand drawCommands[]` + `drawCommandCount`
  - GL shader/program + geometry: `shaderProgram`, `cubeVAO`, etc.
- **Responsibilities**:
  - Record drawing requests during the frame (`DrawVRCuboid()`, `DrawVRLine3D()`, etc. call `AddDrawCommand()`).
  - Replay draw commands once per eye in `RenderEye()` during `EndVRMode()`.

### 5) External Platform Components

- **Android OS / NativeActivity**: lifecycle, input queue + looper, process termination signals.
- **OpenXR Loader (`libopenxr_loader.so`)**: dispatch layer for `xr*` entry points.
- **OpenXR Runtime (system)**: tracking, action bindings, swapchain allocation, composition, session state machine.
- **Hardware**: HMD tracking sensors, controllers/hands, display, and haptic actuators.

## Connectors (Interaction Pathways)

### A) In-process C API Calls

- **App -> RealityLib**: `InitApp`, `BeginVRMode`, `SyncControllers`, drawing calls, `EndVRMode`, `CloseApp`.
- **App -> Hand tracking**: `InitHandTracking`, `UpdateHandTracking`, `GetHand`, `IsHandTracked`, gesture queries.

### B) Android Event Loop Connector

- **RealityLib -> Android OS** via `ALooper_pollOnce()` (in `AppShouldClose()`):
  - Drives `android_native_app_glue` sources (`source->process(app, source)`).
  - Detects shutdown (`app->destroyRequested`) and sets `vrState.shouldExit`.

### C) OpenXR API Connector

- **RealityLib Core -> OpenXR** (through the loader and runtime):
  - Startup: `xrCreateInstance`, `xrGetSystem`, `xrCreateSession`, reference spaces, action sets, swapchains
  - Runtime: `xrPollEvent` (session state), `xrWaitFrame`/`xrBeginFrame`/`xrEndFrame`,
    `xrLocateViews`, `xrSyncActions`, `xrLocateSpace`, `xrGetActionState*`, `xrApplyHapticFeedback`

### D) Hand Tracking Extension Connector

- **Hand tracking -> OpenXR runtime** using dynamically loaded EXT entry points:
  - `xrCreateHandTrackerEXT`, `xrLocateHandJointsEXT`, `xrDestroyHandTrackerEXT`

### E) Graphics Pipeline Connector (EGL/GLES + Swapchains)

- **RealityLib Core/Renderer -> EGL/GLES**:
  - Create context (`eglCreateContext`) and make current (`eglMakeCurrent`)
  - Render into OpenXR swapchain images:
    `xrAcquireSwapchainImage` -> `xrWaitSwapchainImage` -> GL draw -> `xrReleaseSwapchainImage`

## Attachments (Component <-> Connector Graph)

- App/Game Loop attaches to:
  - In-process C API Calls (to RealityLib core and hand tracking)
- RealityLib Core attaches to:
  - Android Event Loop Connector
  - OpenXR API Connector
  - Graphics Pipeline Connector
  - In-process C API Calls (as callee)
- Hand Tracking attaches to:
  - In-process C API Calls (as callee)
  - Hand Tracking Extension Connector
  - Shared-state accessor calls to RealityLib Core
- Deferred Renderer attaches to:
  - In-process drawing calls (recording)
  - Graphics Pipeline Connector (rendering into swapchains)
- OpenXR runtime + hardware attach to:
  - OpenXR API Connector / Hand Tracking Extension Connector (services provided back to app)

## Key Runtime Scenarios

### Startup

1. `android_main()` calls `InitApp(app)`:
   - `InitializeEGL()` creates an ES3 context.
   - `InitializeOpenXR()` initializes loader (Android init info), creates `XrInstance`, selects system, reads view configs.
   - `CreateSession()` binds the EGL context into `XrSession`, creates spaces, actions, and swapchains.
2. `android_main()` optionally calls `InitHandTracking()`:
   - Checks `XR_EXT_hand_tracking` and creates `XrHandTrackerEXT` for left/right hands.

### Per-frame Loop

The main loop in `app/src/main/main.c` is:

1. `AppShouldClose(app)`:
   - Processes Android events (`ALooper_pollOnce`) and OpenXR events (`PollXREvents`).
2. `BeginVRMode()`:
   - `xrWaitFrame` / `xrBeginFrame`
   - `xrLocateViews` to update eye poses and projection/view matrices.
   - Clears the per-frame draw command buffer.
3. `SyncControllers()`:
   - `xrSyncActions`, `xrLocateSpace`, `xrGetActionState*` populate controller + head state.
4. `inLoop(app)`:
   - Game logic reads input/headset state, optionally calls `UpdateHandTracking()`,
     and enqueues drawing via `DrawVR*()` calls.
5. `EndVRMode()`:
   - For each eye: acquire + wait swapchain image, render (replay command buffer), release image.
   - `xrEndFrame` submits the projection layer for composition.

### Shutdown

1. When `AppShouldClose()` returns true, the loop ends.
2. App calls `ShutdownHandTracking()` (if enabled).
3. App calls `CloseApp(app)`:
   - Destroys swapchains, spaces, action set, session, instance, then EGL context/surface/display.

## Constraints & Invariants (Project-Specific)

- `InitApp(app)` must succeed before any VR calls that depend on EGL/OpenXR state.
- Rendering and input functions are effectively gated by `vrState.sessionRunning`:
  - `BeginVRMode()` / `EndVRMode()` / input updates no-op if the session is not running.
- `UpdateHandTracking()` requires:
  - Hand tracking initialized, `IsVRSessionRunning() == true`, a non-null stage space, and a valid `predictedDisplayTime`.
  - Practically, this means calling it during the active frame loop (after `BeginVRMode()` has set `predictedDisplayTime`).
- Draw calls (`DrawVRCuboid`, `DrawVRLine3D`, etc.) record into a per-frame buffer; callers must not assume immediate rendering.

## Using This View

- **Show how the system works** at runtime: one Android process driving an OpenXR session with a frame loop and deferred draw recording.
- **Guide development**: clarifies where to add runtime features (new actions, new draw command types, new subsystems).
- **Reason about quality attributes**:
  - Performance/latency: frame loop is driven by `xrWaitFrame` and swapchain-based rendering; draw calls are buffered and replayed per eye.
  - Availability: hand tracking is optional and can be disabled when the extension is unavailable.
