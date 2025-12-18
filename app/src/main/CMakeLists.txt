# Set the minimum required version of CMake
cmake_minimum_required(VERSION 3.22.1)

# Set standards for C and C++
set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 11)

# Set the project name
project(realitylib)

# Manual OpenXR setup (instead of find_package)
set(OPENXR_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/deps/OpenXR-SDK/include")
set(OPENXR_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/deps/OpenXR-SDK/libs/${ANDROID_ABI}")

# Check if OpenXR exists
if(NOT EXISTS ${OPENXR_INCLUDE_DIR})
    message(FATAL_ERROR "OpenXR headers not found at: ${OPENXR_INCLUDE_DIR}")
endif()

# Configure raylib for Android
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/deps/raylib/CMakeLists.txt")
    set(PLATFORM "Android" CACHE STRING "" FORCE)
    set(GRAPHICS "GRAPHICS_API_OPENGL_ES2" CACHE STRING "" FORCE)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(USE_EXTERNAL_GLFW OFF CACHE BOOL "" FORCE)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/deps/raylib)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/deps/raymob/CMakeLists.txt")
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/deps/raymob)
endif()

# if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/deps/Samples/SampleXrFramework/CMakeLists.txt")
#     add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/deps/Samples/SampleXrFramework)
# endif()

# Fetch all source files recursively, excluding 'deps' source files
file(GLOB_RECURSE SOURCES 
    "${CMAKE_CURRENT_SOURCE_DIR}/*.c" 
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)
list(FILTER SOURCES EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/deps/.*")

# Include headers for android_native_app_glue.c
include_directories(${ANDROID_NDK}/sources/android/native_app_glue/)

# Add android_native_app_glue.c to source files
list(APPEND SOURCES ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c)

# Create a shared library
add_library(${PROJECT_NAME} SHARED ${SOURCES})

# Define compiler macros
target_compile_definitions(${PROJECT_NAME} PRIVATE PLATFORM_ANDROID)

# Set compile flags based on build type
if(CMAKE_BUILD_TYPE MATCHES "Debug")
    target_compile_definitions(${PROJECT_NAME} PRIVATE _DEBUG DEBUG)
    set(CMAKE_CXX_FLAGS_DEBUG "-g -fno-limit-debug-info")
    set(CMAKE_C_FLAGS_DEBUG "-g -fno-limit-debug-info")
elseif(CMAKE_BUILD_TYPE MATCHES "Release")
    target_compile_definitions(${PROJECT_NAME} PRIVATE NDEBUG)
    set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")
    set(CMAKE_C_FLAGS_RELEASE "-O2 -DNDEBUG")
endif()

# Include dependency headers
target_include_directories(${PROJECT_NAME} PRIVATE 
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${OPENXR_INCLUDE_DIR}"
)

# Conditionally include other deps if they exist
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/deps/raylib/src")
    target_include_directories(${PROJECT_NAME} PRIVATE 
        "${CMAKE_CURRENT_SOURCE_DIR}/deps/raylib/src"
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/deps/raymob")
    target_include_directories(${PROJECT_NAME} PRIVATE 
        "${CMAKE_CURRENT_SOURCE_DIR}/deps/raymob"
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/deps/Samples/SampleXrFramework/include")
    target_include_directories(${PROJECT_NAME} PRIVATE 
        "${CMAKE_CURRENT_SOURCE_DIR}/deps/Samples/SampleXrFramework/include"
    )
endif()

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE 
    android
    log
    EGL
    GLESv3
)

# Link OpenXR if library exists
if(EXISTS ${OPENXR_LIB_DIR})
    target_link_libraries(${PROJECT_NAME} PRIVATE 
        ${OPENXR_LIB_DIR}/libopenxr_loader.so
    )
endif()

# Conditionally link other libraries
if(TARGET raylib)
    target_link_libraries(${PROJECT_NAME} PRIVATE raylib)
endif()

if(TARGET raymob)
    target_link_libraries(${PROJECT_NAME} PRIVATE raymob)
endif()

# if(TARGET native_activity_framework)
#     target_link_libraries(${PROJECT_NAME} PRIVATE native_activity_framework)
# endif()

# Required for NativeActivity
set_target_properties(${PROJECT_NAME} PROPERTIES 
    LINK_FLAGS "-u ANativeActivity_onCreate"
)