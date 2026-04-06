/*
MIT License

Copyright (c) 2025-2026 Stephane Cuillerdier (aka Aiekick)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#pragma once

#define IMGENIE_VERSION "0.1.0"
#define IMGENIE_VERSION_NUM 00100

// ImGenie
// macOS-style Genie effect and Wobbly windows for Dear ImGui

// See Documentation.md for usage, integration examples (OpenGL, Vulkan), params reference and C API.

#ifndef IMGENIE_API
#define IMGENIE_API
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Enums (ImGui-style: typedef int + enum, C and C++ compatible)
typedef int ImGenieSide;
enum ImGenieSide_ {
    ImGenieSide_Auto = 0,
    ImGenieSide_Top,
    ImGenieSide_Bottom,
    ImGenieSide_Left,
    ImGenieSide_Right,
};

typedef int ImGenieAnimMode;
enum ImGenieAnimMode_ {
    ImGenieAnimMode_Compress = 0,
    ImGenieAnimMode_Sliding,
};

typedef int ImGenieTransitionMode;
enum ImGenieTransitionMode_ {
    ImGenieTransitionMode_None = 0,
    ImGenieTransitionMode_Genie,
    ImGenieTransitionMode_PageCurl,
    ImGenieTransitionMode_Fade,
    ImGenieTransitionMode_Scale,
    ImGenieTransitionMode_Slide,
};

typedef int ImGeniePageCurlOrigin;
enum ImGeniePageCurlOrigin_ {
    ImGeniePageCurlOrigin_TopLeft = 0,
    ImGeniePageCurlOrigin_Top,
    ImGeniePageCurlOrigin_TopRight,
    ImGeniePageCurlOrigin_Right,
    ImGeniePageCurlOrigin_BottomRight,
    ImGeniePageCurlOrigin_Bottom,
    ImGeniePageCurlOrigin_BottomLeft,
    ImGeniePageCurlOrigin_Left,
};

typedef int ImGenieEffectMode;
enum ImGenieEffectMode_ {
    ImGenieEffectMode_None = 0,
    ImGenieEffectMode_Wobbly,
};

// C-compatible rect struct
typedef struct ImGenie_Rect {
    float minX;
    float minY;
    float maxX;
    float maxY;
} ImGenie_Rect;

// Params structs — C and C++ compatible
// In C, use ImGenie_DefaultParams() to get a properly initialized instance.
typedef struct ImGenieGenieParams {
    int32_t cellsV;
    int32_t cellsH;
    ImGenieSide side;
    ImGenieAnimMode animMode;
    ImGenie_Rect destRect;  // Target rect the window animates to/from (update every frame)
#ifdef __cplusplus
    ImGenieGenieParams() : cellsV(20), cellsH(1), side(ImGenieSide_Auto), animMode(ImGenieAnimMode_Compress), destRect{} {}
#endif
} ImGenieGenieParams;

typedef struct ImGeniePageCurlParams {
    int32_t cellsH;
    int32_t cellsV;
    ImGeniePageCurlOrigin origin;
#ifdef __cplusplus
    ImGeniePageCurlParams() : cellsH(30), cellsV(30), origin(ImGeniePageCurlOrigin_BottomLeft) {}
#endif
} ImGeniePageCurlParams;

typedef int ImGenieSlideDir;
enum ImGenieSlideDir_ {
    ImGenieSlideDir_Auto = 0,       // Auto-detect closest edge or corner
    ImGenieSlideDir_Left,
    ImGenieSlideDir_Right,
    ImGenieSlideDir_Top,
    ImGenieSlideDir_Bottom,
    ImGenieSlideDir_TopLeft,
    ImGenieSlideDir_TopRight,
    ImGenieSlideDir_BottomLeft,
    ImGenieSlideDir_BottomRight,
};

typedef struct ImGenieSpringParams {
    float stiffness;
    float damping;
    int32_t substeps;
    int32_t cellsH;
    int32_t cellsV;
#ifdef __cplusplus
    ImGenieSpringParams() : stiffness(120.0f), damping(8.0f), substeps(8), cellsH(20), cellsV(20) {}
#endif
} ImGenieSpringParams;

typedef struct ImGenieSlideParams {
    ImGenieSlideDir dir;
    float autoCornerRatio;    // Corner zone ratio for Auto detection (0..0.5, default 0.25 = 1/4 of each edge)
    bool wobbly;              // Trailing edge follows via spring physics (rubber stretch)
    ImGenieSpringParams spring;
#ifdef __cplusplus
    ImGenieSlideParams() : dir(ImGenieSlideDir_Auto), autoCornerRatio(0.15f), wobbly(false), spring() {}
#endif
} ImGenieSlideParams;

typedef struct ImGenieWobblyParams {
    ImGenieSpringParams spring;
    float maxStiffness;       // Overrides spring.stiffness at grab point (varies with distance)
    float settleDuration;
#ifdef __cplusplus
    ImGenieWobblyParams() : spring(), maxStiffness(200.0f), settleDuration(0.15f) {}
#endif
} ImGenieWobblyParams;

typedef struct ImGenieTransitions {
    ImGenieTransitionMode transitionMode;
    float animDuration;  // Common to all transitions
    ImGenieGenieParams genie;
    ImGeniePageCurlParams pageCurl;
    ImGenieSlideParams slide;
#ifdef __cplusplus
    ImGenieTransitions() : transitionMode(ImGenieTransitionMode_Genie), animDuration(0.4f), genie(), pageCurl(), slide() {}
#endif
} ImGenieTransitions;

typedef struct ImGenieEffects {
    ImGenieEffectMode effectMode;
    ImGenieWobblyParams wobbly;
#ifdef __cplusplus
    ImGenieEffects() : effectMode(ImGenieEffectMode_Wobbly), wobbly() {}
#endif
} ImGenieEffects;

typedef struct ImGenieParams {
    bool drawDebug;
    ImGenieTransitions transitions;
    ImGenieEffects effects;
#ifdef __cplusplus
    ImGenieParams() : drawDebug(false), transitions(), effects() {}
#endif
} ImGenieParams;

#ifdef __cplusplus

#include <functional>
#include <unordered_map>

#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif  // IMGUI_DEFINE_MATH_OPERATORS

#ifdef IMGUI_INCLUDE
#include IMGUI_INCLUDE
#else  // IMGUI_INCLUDE
#include <imgui_internal.h>
#endif  // IMGUI_INCLUDE

typedef std::function<ImTextureRef(int32_t aWidth, int32_t aHeight, ImDrawData* apDrawData)> CreateCaptureFunctor;
typedef std::function<void(const ImTextureRef&)> DestroyCaptureFunctor;

// POD effect struct (ImGui-style, public members)
struct ImGenieEffect {
    enum class State {
        PendingCapture,      // Disappearance: waiting for backbuffer capture
        Captured,            // Disappearance: captured, ready to animate
        Animating,           // Disappearance: animation in progress
        AppearingCapture,    // Appearance: waiting for FBO capture
        AppearingAnimating,  // Appearance: reverse animation in progress
        MovingCapture,       // Moving: waiting for FBO capture
        MovingActive,        // Moving: drag in progress, wobbly lattice
        MovingSettle         // Moving: drag ended, springs settling
    };
    State state{State::PendingCapture};
    ImRect sourceRect{};
    ImRect destRect{};
    ImTextureRef capturedTex{};
    ImVec2 capturedSize{};
    float animT{0.0f};
    // Wobbly spring corners (TL, TR, BL, BR)
    struct SpringCorner {
        ImVec2 current{};
        ImVec2 velocity{};
    } springs[4]{};
    float springWeights[4]{};
    ImVec2 grabUV{};
    ImVec2 settleStart[4]{};
    // Resolved side (computed once at effect creation, never changes)
    ImGenieSide resolvedSide{ImGenieSide_Top};
    // Per-effect params snapshot
    ImGenieParams params{};
};

// POD context struct (ImGui-style, public members)
struct ImGenieContext {
    ImGenieParams defaultParams{};
    ImGenieParams globalParams{};
    std::unordered_map<ImGuiID, ImGenieEffect> effects{};
    std::unordered_map<ImGuiID, bool> openStates{};
    std::unordered_map<ImGuiID, const char*> effectNames{};
    std::unordered_map<ImGuiID, ImVec2> dragStartPos{};
    CreateCaptureFunctor createCaptureFunc{};
    DestroyCaptureFunctor destroyCaptureFunc{};
    bool captureFlipV{false};  // OpenGL needs true (Y-axis inverted), Vulkan/Metal/DX12 = false
};

// Check that version and struct sizes match between compiled ImGenie code and caller.
#define IMGENIE_CHECKVERSION() ImGenie::DebugCheckVersion(IMGENIE_VERSION, sizeof(ImGenieParams), sizeof(ImGenieEffect), sizeof(ImGenieContext))

// ImGenie API
namespace ImGenie {

// Version check
IMGENIE_API bool DebugCheckVersion(const char* aVersion, size_t aParamsSize, size_t aEffectSize, size_t aContextSize);

// Contexts functions
IMGENIE_API ImGenieContext* CreateContext();
IMGENIE_API void DestroyContext(ImGenieContext* apCtx = nullptr);
IMGENIE_API ImGenieContext* GetCurrentContext();
IMGENIE_API void SetCurrentContext(ImGenieContext* apCtx);

// Create/Destroy Capture functions
IMGENIE_API void SetCreateCaptureFunc(const CreateCaptureFunctor& arFunc);
IMGENIE_API void SetDestroyCaptureFunc(const DestroyCaptureFunctor& arFunc);
IMGENIE_API void SetCaptureFlipV(bool aFlipV);  // Set true for OpenGL (Y-axis inverted)

IMGENIE_API void Capture();  // Capture the windows textures. Call AFTER ImGui::Render() and BEFORE RenderDrawData().

// Returns true if at least one genie effect is active (animating, capturing, moving, etc.)
IMGENIE_API bool HasActiveEffects();

// Return true if an effect is active on this aWindowName
IMGENIE_API bool IsEffectActive(const char* aWindowName);

// Show the demo window
IMGENIE_API void ShowDemoWindow(bool* apoOpen = nullptr, ImGenieParams* apoParams = nullptr, ImGenieParams* apoDefaultParams = nullptr);

// to Call every frame UNCONDITIONALLY, BEFORE the if(show) guard.
// Return true if you can show your window normally.
// Return false if ImGenie is handling the window (animating).
IMGENIE_API bool Allow(const char* aWindowName, bool* apoOpen, const ImGenieParams* apParams = nullptr);

// Wrapper combining Allow + ImGui::Begin/End for simpler usage.
// IMPORTANT: Unlike ImGui::Begin/End, End() must only be called if Begin() returned true.
// Calling End() when Begin() returned false will trigger an ImGui assert.
// Usage:
//   if (ImGenie::Begin("Window", &show)) {
//       /* content */
//       ImGenie::End();
//   }
IMGENIE_API bool Begin(const char* aWindowName, bool* apoOpen = NULL, ImGuiWindowFlags flags = 0, const ImGenieParams* apParams = nullptr);
IMGENIE_API void End();

}  // namespace ImGenie

#endif  // __cplusplus

/////////////////////////////////////////////////
////// C LANG API ///////////////////////////////
/////////////////////////////////////////////////

#ifdef __cplusplus
#define IMGENIE_C_API extern "C" IMGENIE_API
#else  // __cplusplus
typedef struct ImGenieContext ImGenieContext;
#define IMGENIE_C_API
#endif  // __cplusplus

// C API callback types — use void* for opaque ImGui types (ImTextureRef, ImDrawData)
// The wrapper binding is responsible for casting to/from the correct types.
typedef void* (*ImGenie_CreateCaptureFunc)(int32_t aWidth, int32_t aHeight, void* apDrawData);
typedef void (*ImGenie_DestroyCaptureFunc)(void* apTex);


IMGENIE_C_API const char* ImGenie_GetVersion(void);
IMGENIE_C_API int ImGenie_GetVersionNum(void);
IMGENIE_C_API bool ImGenie_DebugCheckVersion(const char* aVersion, size_t aParamsSize, size_t aEffectSize, size_t aContextSize);
IMGENIE_C_API ImGenieContext* ImGenie_CreateContext(void);
IMGENIE_C_API void ImGenie_DestroyContext(ImGenieContext* apCtx);
IMGENIE_C_API ImGenieContext* ImGenie_GetCurrentContext(void);
IMGENIE_C_API void ImGenie_SetCurrentContext(ImGenieContext* apCtx);
IMGENIE_C_API void ImGenie_SetCreateCaptureFunc(ImGenie_CreateCaptureFunc aFunc);
IMGENIE_C_API void ImGenie_SetDestroyCaptureFunc(ImGenie_DestroyCaptureFunc aFunc);
IMGENIE_C_API void ImGenie_SetCaptureFlipV(bool aFlipV);
IMGENIE_C_API void ImGenie_Capture(void);
IMGENIE_C_API bool ImGenie_HasActiveEffects(void);
IMGENIE_C_API bool ImGenie_IsEffectActive(const char* aWindowName);
IMGENIE_C_API ImGenieParams* ImGenie_DefaultParams(void);
IMGENIE_C_API void ImGenie_ShowDemoWindow(bool* apoOpen, ImGenieParams* apoParams, ImGenieParams* apoDefaultParams);
IMGENIE_C_API bool ImGenie_Allow(const char* aWindowName, bool* apoOpen, const ImGenieParams* apParams);
IMGENIE_C_API bool ImGenie_Begin(const char* aWindowName, bool* apoOpen, int aFlags, const ImGenieParams* apParams);
IMGENIE_C_API void ImGenie_End(void);
