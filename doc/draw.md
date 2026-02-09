| Software unit | Allocated to layer | What it depends on |
|---|---|---|
| `android_main()` + app loop (`main.c`) | Android API + RealityLib | NativeActivity lifecycle + calls into RealityLib |
| RealityLib VR core (`realitylib_vr.c`) | RealityLib + OpenXR inputs + Android API | EGL/GLES + OpenXR session/swapchains/actions |
| Hand tracking (`realitylib_hands.c`) | OpenXR inputs | `XR_EXT_hand_tracking` extension support |
| Renderer / draw submission | RealityLib + Android API | GLES rendering into OpenXR swapchain images |