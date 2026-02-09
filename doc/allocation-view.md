# RealityLib Allocation View

Allocation views describe how software units map to elements of the environment in which the software is **developed** and **executed**.

## View Definition (Allocation)

- **Elements**
  - **Software elements**: code units / build units / runtime artifacts with *requirements* on their environment.
  - **Environmental elements**: platforms, tools, hardware, OS/runtime services providing properties/capabilities to software.
- **Relation**
  - **Allocated-to**: a software element is mapped (allocated) to an environmental element.
- **Constraints**
  - Vary per view; see “Constraints & Assumptions”.
- **Usage**
  - Reason about performance, availability, security/safety, dev workflow, and installation mechanisms.

## Scope (This Repository)

This view is grounded in the current Android/OpenXR native stack:

- Build orchestration: `app/build.gradle`, `build.gradle`, `settings.gradle`
- Native build: `app/src/main/CMakeLists.txt`
- Runtime entrypoint: `app/src/main/main.c` (`android_main`)
- Runtime implementation: `app/src/main/realitylib_vr.c`, `app/src/main/realitylib_hands.c`
- Packaging/manifest: `app/src/main/AndroidManifest.xml`
- Dependency setup: `scripts/setup_deps.sh`

## Environmental Elements

### Development / Build Environment

- **Dev workstation**
  - Provides: filesystem, shell, CPU, memory.
  - Must provide: Java 11+, Android SDK, Android NDK.
- **Gradle (Android Gradle Plugin)**
  - Provides: dependency resolution, APK packaging, native build orchestration.
- **Android SDK (API 33 used for compile; platform tools)**
  - Provides: build tools, `adb` for install/debug.
- **Android NDK (pinned)**
  - Required: NDK `29.0.14206865` (per `app/build.gradle`).
  - Provides: toolchain, `android_native_app_glue`.
- **CMake**
  - Required: CMake `3.22.1` (per `app/build.gradle`).
  - Provides: native build graph, linking config.
- **Maven repositories**
  - Provides: `org.khronos.openxr:openxr_loader_for_android:1.1.51` AAR (resolved by Gradle).
- **OpenXR headers source**
  - Provided via `scripts/setup_deps.sh` (downloads Khronos OpenXR-SDK headers into `app/src/main/deps/OpenXR-SDK/include`).

### Execution Environment (Meta Quest)

- **Android OS runtime (Quest)**
  - Provides: process lifecycle, native activity hosting, looper, native windowing.
- **NativeActivity**
  - Provides: entry into native code; hands over `struct android_app*`.
- **OpenXR loader + OpenXR runtime**
  - Provides: OpenXR API entry points and the system implementation for tracking, actions, swapchains, composition, etc.
- **Graphics stack (EGL + OpenGL ES)**
  - Provides: EGL context/surface management, GLES3 rendering, GPU driver.
- **Hardware**
  - Provides: HMD pose tracking, controllers, optional hand tracking sensors, haptics, GPU/display.

## Software Elements

### Source Modules

- **User/game code**: `app/src/main/main.c`
  - Owns “game loop” (`inLoop`) and high-level app behavior.
- **RealityLib VR core**: `app/src/main/realitylib_vr.c` / `app/src/main/realitylib_vr.h`
  - Owns OpenXR + EGL/GLES integration, actions/input, swapchains, frame loop plumbing.
- **RealityLib hand tracking**: `app/src/main/realitylib_hands.c` / `app/src/main/realitylib_hands.h`
  - Owns XR_EXT_hand_tracking integration and gesture derivation.
- **Build glue**: `app/src/main/CMakeLists.txt`, `app/build.gradle`
  - Owns how native code is compiled/linked and how dependencies are staged.
- **App configuration**: `app/src/main/AndroidManifest.xml`
  - Declares runtime expectations and packaging entry configuration.

### Runtime Artifacts

- **APK**: `app/build/outputs/apk/<variant>/app-<variant>.apk`
  - Container for native shared libraries and manifest metadata.
- **Native shared library**: `librealitylib.so`
  - Loaded by `NativeActivity` via `android.app.lib_name = realitylib`.
- **OpenXR loader shared library**: `libopenxr_loader.so`
  - Resolved by Gradle (`openxr_loader_for_android`) and staged for CMake linking under `app/src/main/deps/OpenXR-SDK/libs/<abi>/`.

## Allocations

### A) Development / Build Allocation (Source -> Toolchain)

| Software element | Allocated-to (environment) | Required properties | Provided properties |
|---|---|---|---|
| `app/src/main/*.c` / `*.h` | Android NDK toolchain | arm64 cross-compile; NDK `29.0.14206865` | Clang, sysroot, `android_native_app_glue` |
| `app/build.gradle` | Gradle + AGP 8.2.0 | Maven access; Android SDK; CMake/NDK installed | Builds APK; orchestrates CMake; packages .so files |
| `scripts/setup_deps.sh` | Dev workstation (shell + curl + unzip) | Network + zip tools | Populates `deps/OpenXR-SDK/include/openxr` |
| `org.khronos.openxr:openxr_loader_for_android:1.1.51` | Maven repositories | Network / repo access | Provides `libopenxr_loader.so` in an AAR |

### B) Packaging Allocation (Build outputs -> APK structure)

| Software element | Allocated-to (environment) | Required properties | Provided properties |
|---|---|---|---|
| `librealitylib.so` | Android APK native libs (`lib/arm64-v8a/`) | ABI `arm64-v8a` | Loadable by the Android dynamic linker |
| `libopenxr_loader.so` | Android APK native libs (`lib/arm64-v8a/`) | Staged for link (`extractOpenXRLoader` task) | OpenXR API dispatch layer available at runtime |
| `AndroidManifest.xml` | APK manifest | Correct `NativeActivity` config and intent categories | OS can launch the app as OpenXR immersive HMD |

### C) Runtime Allocation (Software units -> Runtime/Hardware services)

| Software element | Allocated-to (environment) | Required properties | Provided properties |
|---|---|---|---|
| `android_main()` (`main.c`) | Android `NativeActivity` process | `android:hasCode="false"` + `android.app.lib_name` points to `realitylib` | Process creation; lifecycle callbacks; `android_app` handle |
| `realitylib_vr.c` (EGL) | EGL + GLES3 stack | OpenGL ES 3.x support (manifest declares ES 3.1) | GL context and rendering execution on GPU |
| `realitylib_vr.c` (OpenXR core) | OpenXR loader + runtime | Valid `XrInstance`/`XrSession`; runtime support for primary stereo view | Tracking, swapchains, composition, actions, haptics |
| `realitylib_hands.c` | OpenXR runtime (XR_EXT_hand_tracking) | XR_EXT_hand_tracking supported and enabled | Skeletal joints and derived gesture state |
| `AppShouldClose()` | Android looper | Event queue + `ALooper_pollOnce` | Responsive lifecycle shutdown and event processing |

## Constraints & Assumptions (Current Codebase)

- **ABI**: `arm64-v8a` only (per `abiFilters` in `app/build.gradle`).
- **Android API level**: `minSdk 26`, `targetSdk 33`, `compileSdk 33` (per `app/build.gradle`).
- **Graphics**: requires OpenGL ES 3.1 capability (`android:glEsVersion="0x00030001"` in `app/src/main/AndroidManifest.xml`).
- **VR feature**: requires `android.hardware.vr.headtracking` (manifest `required="true"`).
- **Hand tracking**:
  - Declared optional capability (`oculus.software.handtracking` required="false").
  - Still requests `com.oculus.permission.HAND_TRACKING` in the manifest; feature availability is checked at runtime.
- **OpenXR loader acquisition**:
  - Gradle resolves and stages `libopenxr_loader.so` via the `extractOpenXRLoader` task in `app/build.gradle`.
  - OpenXR headers are expected under `app/src/main/deps/OpenXR-SDK/include` (populated by `scripts/setup_deps.sh`).

## Usage Notes (Why This View Is Useful)

- **Performance**: clarifies that rendering is native (EGL/GLES) and frame pacing is tied to OpenXR (`xrWaitFrame`/swapchains); ABI and GLES requirements are explicit.
- **Availability**: hand tracking is allocated as an optional dependency; the app can fall back to controllers when XR_EXT_hand_tracking is unavailable.
- **Security/Safety**: the manifest declares the critical permissions and required device capabilities; the app is a native-only `NativeActivity` (`android:hasCode="false"`).
- **Installation mechanism**: build produces an APK which is installed via `adb` (documented in `README.md` and `scripts/setup_deps.sh`).
