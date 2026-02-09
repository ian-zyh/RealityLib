# RealityLib Allocation View (3-Layer)

This allocation view shows how the repository maps onto three runtime layers:

1) **Android API layer** (process + lifecycle + windowing + graphics)
2) **RealityLib / Raylib layer** (your native game/app code + rendering integration)
3) **OpenXR inputs layer** (tracking, controllers, hands, actions, haptics via OpenXR)

---

## Layer 1 — Android API (Platform / Runtime)

**What it provides**
- Process lifecycle and native entry via **NativeActivity**
- Event loop / looper (`ALooper_pollOnce`)
- Native windowing + EGL/GLES context + GPU presentation
- Packaging/launch via **APK + Manifest**

**Where it shows up**
- `app/src/main/main.c` (`android_main`)
- `app/src/main/AndroidManifest.xml` (capabilities, permissions, NativeActivity config)

---

## Layer 2 — RealityLib / Raylib (App + Rendering)

**What it provides**
- Your app/game loop and high-level behavior
- VR runtime glue code (EGL/GLES + frame loop + swapchain usage)
- Renderer and draw-command recording (Raylib-facing layer / app-facing API)

**Where it shows up**
- `app/src/main/main.c` (game loop / app logic calling into RealityLib)
- `app/src/main/realitylib_vr.c` (VR core: EGL/GLES + swapchains + frame loop)
- `app/src/main/realitylib_hands.c` (hand tracking state + convenience accessors)

**Key responsibility split**
- Android gives you a running native process + GL surface.
- RealityLib/Raylib owns “what to draw” and “how to submit frames” (through OpenXR swapchains).

---

## Layer 3 — OpenXR Inputs (Runtime + Sensors)

**What it provides**
- OpenXR session + frame timing (`xrWaitFrame` / `xrBeginFrame` / `xrEndFrame`)
- Head pose tracking, controller inputs, action system
- Hand tracking extension (`XR_EXT_hand_tracking`) if supported
- Haptics/output routed through the runtime

**Where it shows up**
- `realitylib_vr.c`: OpenXR core usage (`xr*` session/swapchains/actions/locate/submit)
- `realitylib_hands.c`: `xrCreateHandTrackerEXT`, `xrLocateHandJointsEXT`
- Runtime side: OpenXR loader + vendor runtime (Quest system implementation)

---

## Primary Allocations (Software → Layer)

| Software unit | Allocated to layer | What it depends on |
|---|---|---|
| `android_main()` + app loop (`main.c`) | Android API + RealityLib | NativeActivity lifecycle + calls into RealityLib |
| RealityLib VR core (`realitylib_vr.c`) | RealityLib + OpenXR inputs + Android API | EGL/GLES + OpenXR session/swapchains/actions |
| Hand tracking (`realitylib_hands.c`) | OpenXR inputs | `XR_EXT_hand_tracking` extension support |
| Renderer / draw submission | RealityLib + Android API | GLES rendering into OpenXR swapchain images |

---

## Build & Packaging (Additional)

(Everything Gradle/CMake-related is treated as “additional” build plumbing.)

- **Gradle / Android Gradle Plugin**: orchestrates build + packages APK
- **CMake + Android NDK**: compiles and links native code into `librealitylib.so`
- **Dependency staging**: brings in the OpenXR loader (`libopenxr_loader.so`) and headers as needed
- Outputs: APK containing `librealitylib.so` (+ OpenXR loader .so for the target ABI)

---
