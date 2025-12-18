## Dependencies & Versions

### Java/Gradle Dependencies

| Dependency | Version | Purpose |
|---|---|---|
| Android Gradle Plugin | 8.2.0 | Android build system |
| AndroidX AppCompat | 1.6.1 | Android compatibility library |

### Android SDK Configuration

| Configuration | Version |
|---|---|
| Compile SDK | 33 |
| Min SDK | 26 (Android 8.0 Oreo) |
| Target SDK | 33 (Android 13) |
| NDK Version | 29.0.14206865 |
| CMake Version | 3.22.1 |

### Build Configuration

| Setting | Value |
|---|---|
| Java Source Compatibility | Java 11 |
| Java Target Compatibility | Java 11 |
| C Standard | C99 |
| C++ Standard | C++11 |
| STL Type | c++_shared |
| Target ABI | arm64-v8a |

### Native Libraries & Headers

| Library | Version | Location |
|---|---|---|
| OpenXR SDK | Latest (from deps) | `app/src/main/deps/OpenXR-SDK` |
| raylib | Latest (from deps) | `app/src/main/deps/raylib` |
| raymob | Latest (from deps) | `app/src/main/deps/raymob` |
| Android NDK | 29.0.14206865 | `$ANDROID_SDK_ROOT/ndk/29.0.14206865` |

## System Requirements

- **Android Studio**: Latest version (2024+)
- **JDK**: Java 11 or higher
- **Android SDK**: API 33+
- **Android NDK**: Version 29.0.14206865
- **CMake**: Version 3.22.1+
- **Windows/Mac/Linux**: Any OS with Android Studio support

## Setup Instructions

### Prerequisites

1. **Install Android Studio** from [developer.android.com](https://developer.android.com/studio)

2. **Install Required SDK Components** in Android Studio:
   - Go to `Tools` → `SDK Manager`
   - Under the `SDK Platforms` tab:
     - Check Android 13 (API level 33)
     - Check Android 8.0 (API level 26) for minimum SDK support
   - Under the `SDK Tools` tab:
     - Android NDK (Side by side): Install version **29.0.14206865**
     - CMake: Install version **3.22.1+**
     - Android SDK Platform-Tools
     - Android SDK Build-Tools (latest)

3. **Configure Local Properties**:
   - A `local.properties` file should be automatically created in the project root
   - Verify it contains paths to your Android SDK and NDK:
     ```properties
     sdk.dir=/path/to/Android/Sdk
     ndk.dir=/path/to/Android/Sdk/ndk/29.0.14206865
     ```

### Clone & Build

1. **Clone the Repository**:
   ```bash
   git clone https://github.com/ian-zyh/RealityLibTest.git
   cd RealityLibTest
   ```

2. **Initialize Gradle** (first build may take longer):
   ```bash
   # On Windows
   gradlew.bat build
   
   # On Mac/Linux
   ./gradlew build
   ```

## Building & Running in Android Studio

### Method 1: Using Android Studio UI (Recommended)

1. **Open the Project**:
   - Open Android Studio
   - Select `File` → `Open`
   - Navigate to the `RealityLibTest` project folder and click `OK`
   - Wait for Gradle sync to complete

2. **Connect an Android Device or Emulator**:
   - Physical Device: Enable USB Debugging and connect via USB
   - Emulator: Create an AVD through `Tools` → `AVD Manager` (select API 33)

3. **Build the Project**:
   - Click `Build` → `Make Project` (or press `Ctrl+F9`)
   - Wait for the build to complete

4. **Run the Application**:
   - Click `Run` → `Run 'app'` (or press `Shift+F10`)
   - Select your target device/emulator
   - Click `OK`

### Method 2: Using Gradle Command Line

```bash
# Navigate to project directory
cd RealityLibTest

# Debug Build
./gradlew installDebug

# Release Build
./gradlew installRelease

# Run on connected device
adb shell am start -n com.realitylib.app/.MainActivity
```

### Method 3: Build & Deploy APK

1. **Build Debug APK**:
   ```bash
   ./gradlew assembleDebug
   ```
   APK location: `app/build/outputs/apk/debug/app-debug.apk`

2. **Build Release APK** (requires signing):
   ```bash
   ./gradlew assembleRelease
   ```
   APK location: `app/build/outputs/apk/release/app-release.apk`

3. **Install on Device**:
   ```bash
   adb install app/build/outputs/apk/debug/app-debug.apk
   ```

## Build Troubleshooting

### Common Issues

1. **NDK not found**:
   - Verify NDK version 29.0.14206865 is installed
   - Update `local.properties` with correct NDK path

2. **CMake not found**:
   - Install CMake 3.22.1+ via SDK Manager
   - Ensure CMake is in your system PATH

3. **OpenXR headers not found**:
   - Ensure `app/src/main/deps/OpenXR-SDK` exists with headers in `include/`
   - Run: `git lfs pull` if using Git LFS for large binary files

4. **Gradle sync fails**:
   - Click `File` → `Sync Now` in Android Studio
   - Check that Gradle is using the correct JDK (Java 11+)

5. **Build takes too long**:
   - Parallel builds are enabled by default in `gradle.properties`
   - Use incremental builds by changing code in the IDE

## Project Structure

```
RealityLibTest/
├── app/
│   ├── src/main/
│   │   ├── AndroidManifest.xml
│   │   ├── CMakeLists.txt         # Native build configuration
│   │   ├── main.c                 # Native entry point
│   │   ├── deps/
│   │   │   ├── OpenXR-SDK/        # OpenXR headers & libs
│   │   │   ├── raylib/            # Graphics library
│   │   │   ├── raymob/            # Android bindings
│   │   │   └── Samples/           # Sample frameworks
│   │   ├── examples/              # Example implementations
│   │   └── res/                   # Android resources
│   └── build.gradle               # App build configuration
├── build.gradle                   # Root build configuration
├── gradle.properties              # Gradle & feature settings
├── local.properties               # SDK/NDK paths (local)
└── README.md                      # This file
```


