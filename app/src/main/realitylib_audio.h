/**
 * RealityLib Audio - Simple 3D Audio Engine (Android/Quest)
 *
 * C-facing API designed to match RealityLib's "just edit main.c" workflow.
 */
 
 #ifndef REALITYLIB_AUDIO_H
 #define REALITYLIB_AUDIO_H
 
 #include <stdbool.h>
 #include <stdint.h>
 
 #include "realitylib_vr.h" // Vector3, Quaternion
 
 #ifdef __cplusplus
 extern "C" {
 #endif
 
 // =============================================================================
 // Handles and enums
 // =============================================================================
 
 typedef uint32_t AudioClipHandle;
 typedef uint32_t AudioEmitterHandle;
 typedef uint32_t AudioVoiceHandle;
 
 enum {
     AUDIO_INVALID_HANDLE = 0
 };
 
 typedef enum AudioSpatializerMode {
     AUDIO_SPATIALIZER_SIMPLE = 0,
     AUDIO_SPATIALIZER_HRTF   = 1,  // reserved for future implementation
 } AudioSpatializerMode;
 
 typedef enum AudioClipLoadFlags {
     AUDIOCLIP_DECODE_TO_PCM = 1 << 0, // load fully into memory (SFX)
     AUDIOCLIP_STREAM        = 1 << 1, // streaming (music) - may be ignored in v1
 } AudioClipLoadFlags;
 
 typedef struct AudioPlayParams {
     float gain;       // 0..?
     float pitch;      // 1.0 = normal
     bool loop;
     float fadeInSeconds;
 } AudioPlayParams;
 
 // =============================================================================
 // Engine lifecycle
 // =============================================================================
 
 // Initialize audio engine. If sampleRate or framesPerBurst are 0, sensible defaults are used.
 bool InitAudioEngine(int sampleRate, int framesPerBurst);
 void ShutdownAudioEngine(void);
 void UpdateAudioEngine(float dt);
 
 // =============================================================================
 // Android integration helpers
 // =============================================================================
 
 // Provide Android AssetManager for loading audio from APK assets.
 // Call once after InitApp(app) (or at least before LoadAudioClip).
 void SetAudioAssetManager(void* assetManager /* AAssetManager* */);
 
 // =============================================================================
 // Listener controls
 // =============================================================================
 
 void SetAudioListenerPose(Vector3 position, Quaternion orientation);
 void SetAudioMasterVolume(float volume);
 void SetAudioSpatializer(AudioSpatializerMode mode);
 
 // =============================================================================
 // Assets
 // =============================================================================
 
 // Load an audio clip from Android assets. Example: "sfx/jump.wav"
 AudioClipHandle LoadAudioClip(const char* assetPath, AudioClipLoadFlags flags);
 void UnloadAudioClip(AudioClipHandle clip);
 
 // =============================================================================
 // Emitters / voices
 // =============================================================================
 
 AudioEmitterHandle CreateAudioEmitter(void);
 void DestroyAudioEmitter(AudioEmitterHandle e);
 
 void SetAudioEmitterPose(AudioEmitterHandle e, Vector3 pos, Vector3 vel, Quaternion ori);
 void SetAudioEmitterParams(
     AudioEmitterHandle e,
     float gain,
     float pitch,
     float minDist,
     float maxDist,
     float rolloff,
     float coneInnerDeg,
     float coneOuterDeg,
     float coneOuterGain
 );
 
 // Convenience: play a 3D one-shot without creating an emitter.
 AudioVoiceHandle PlayOneShot3D(AudioClipHandle clip, Vector3 pos, float gain);
 
 // Play a clip attached to an emitter.
 AudioVoiceHandle PlayOnEmitter(AudioEmitterHandle e, AudioClipHandle clip, AudioPlayParams params);
 
 // Stop a voice (optional fade out).
 void StopVoice(AudioVoiceHandle v, float fadeOutSeconds);
 
 #ifdef __cplusplus
 }
 #endif
 
 #endif // REALITYLIB_AUDIO_H
