# RealityLib - VR Framework for Meta Quest
# CMakeLists.txt

cmake_minimum_required(VERSION 3.22.1)

# Project configuration
project(realitylib C)
set(CMAKE_C_STANDARD 99)

# =============================================================================
# OpenXR Setup
# =============================================================================

set(OPENXR_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/deps/OpenXR-SDK/include")
set(OPENXR_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/deps/OpenXR-SDK/libs/${ANDROID_ABI}")

# Check if OpenXR headers exist
if(NOT EXISTS "${OPENXR_INCLUDE_DIR}/openxr/openxr.h")
    message(WARNING "OpenXR headers not found at: ${OPENXR_INCLUDE_DIR}")
    message(WARNING "Run scripts/setup_deps.sh to download dependencies")
endif()

# =============================================================================
# Source Files
# =============================================================================

# Main RealityLib sources
set(REALITYLIB_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/main.c
    ${CMAKE_CURRENT_SOURCE_DIR}/realitylib_vr.c
)

# Add android_native_app_glue
include_directories(${ANDROID_NDK}/sources/android/native_app_glue)
list(APPEND REALITYLIB_SOURCES 
    ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c
)

# =============================================================================
# Create Shared Library
# =============================================================================

add_library(${PROJECT_NAME} SHARED ${REALITYLIB_SOURCES})

# =============================================================================
# Compiler Configuration
# =============================================================================

target_compile_definitions(${PROJECT_NAME} PRIVATE
    PLATFORM_ANDROID
    __ANDROID__
    XR_USE_GRAPHICS_API_OPENGL_ES
    XR_USE_PLATFORM_ANDROID
)

# Build type specific flags
if(CMAKE_BUILD_TYPE MATCHES "Debug")
    target_compile_definitions(${PROJECT_NAME} PRIVATE _DEBUG DEBUG)
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g -fno-limit-debug-info")
elseif(CMAKE_BUILD_TYPE MATCHES "Release")
    target_compile_definitions(${PROJECT_NAME} PRIVATE NDEBUG)
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O2 -DNDEBUG")
endif()

# Architecture specific flags
if(CMAKE_ANDROID_ARCH_ABI STREQUAL "arm64-v8a")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mfix-cortex-a53-835769")
endif()

# =============================================================================
# Include Directories
# =============================================================================

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${OPENXR_INCLUDE_DIR}
    ${ANDROID_NDK}/sources/android/native_app_glue
)

# =============================================================================
# Link Libraries
# =============================================================================

# Core Android libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    android
    log
    EGL
    GLESv3
    m
)

# Find the OpenXR loader (extracted from AAR by Gradle)
set(OPENXR_LOADER "${OPENXR_LIB_DIR}/libopenxr_loader.so")

if(EXISTS ${OPENXR_LOADER})
    message(STATUS "Found OpenXR loader: ${OPENXR_LOADER}")
    target_link_libraries(${PROJECT_NAME} PRIVATE ${OPENXR_LOADER})
else()
    # The loader will be extracted by Gradle before linking
    # Create an imported target for it
    message(STATUS "OpenXR loader will be provided by Gradle dependency")
    message(STATUS "Expected location: ${OPENXR_LOADER}")
    
    # Still try to link - Gradle should extract it before the link step
    target_link_libraries(${PROJECT_NAME} PRIVATE ${OPENXR_LOADER})
endif()

# =============================================================================
# Native Activity Configuration
# =============================================================================

set_target_properties(${PROJECT_NAME} PROPERTIES
    LINK_FLAGS "-u ANativeActivity_onCreate"
)

# =============================================================================
# Status Output
# =============================================================================

message(STATUS "")
message(STATUS "=== RealityLib Build Configuration ===")
message(STATUS "Android ABI: ${ANDROID_ABI}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "OpenXR Include: ${OPENXR_INCLUDE_DIR}")
message(STATUS "OpenXR Lib: ${OPENXR_LIB_DIR}")
message(STATUS "======================================")
message(STATUS "")
