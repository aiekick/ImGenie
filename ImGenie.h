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

// ImGenie, v0.1.0
// macOS-style Genie effect and Wobbly windows for Dear ImGui

#define IMGENIE_VERSION "0.1.0"
#define IMGENIE_VERSION_NUM 00100

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

// C-compatible rect struct
typedef struct ImGenie_Rect {
    float minX;
    float minY;
    float maxX;
    float maxY;
} ImGenie_Rect;

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

/*
Code sample for opengl

==== Creation : ====

IMGUI_CHECKVERSION();
ImGui::CreateContext();
ImGenie::CreateContext();
ImGenie::SetCreateCaptureFunc([](int32_t aWidth, int32_t aHeight, ImDrawData* apDrawData) {
    ImTextureRef ret{};

    // Create texture for FBO color attachment
    GLuint fboTex{};
    glGenTextures(1, &fboTex);
    glBindTexture(GL_TEXTURE_2D, fboTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, aWidth, aHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    // Create FBO and attach texture
    GLuint fbo{};
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fboTex, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteFramebuffers(1, &fbo);
        glDeleteTextures(1, &fboTex);
        return ret;
    }

    // Save GL state, render to FBO, restore
    GLint prevViewport[4];
    glGetIntegerv(GL_VIEWPORT, prevViewport);
    glViewport(0, 0, aWidth, aHeight);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(apDrawData);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]);

    // Delete FBO but keep the texture (it contains the rendered snapshot)
    glDeleteFramebuffers(1, &fbo);

    ret._TexID = fboTex;
    return ret;
});
ImGenie::SetCaptureFlipV(true);  // OpenGL Y-axis is inverted

ImGenie::SetDestroyCaptureFunc([](const ImTextureRef& aTex) {
    GLuint texID = static_cast<GLuint>(static_cast<uintptr_t>(aTex._TexID));
    glDeleteTextures(1, &texID);
});

==== render loop : ====

// Cpu Zone : prepare
ImGui::Render();

// Capture windows to FBO before main render
ImGenie::Capture();

// GPU Zone : Rendering
glfwMakeContextCurrent(window);

glViewport(0, 0, displayW, displayH);
glClear(GL_COLOR_BUFFER_BIT);
ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

Destruction :

ImGenie::DestroyContext();
ImGui::DestroyContext();
*/

typedef std::function<ImTextureRef(int32_t aWidth, int32_t aHeight, ImDrawData* apDrawData)> CreateCaptureFunctor;
typedef std::function<void(const ImTextureRef&)> DestroyCaptureFunctor;

// POD settings struct (ImGui-style, public members, C++ defaults)
struct ImGenieSettings {
    bool drawDebugMesh{false};
    // Enable/disable effects
    bool enableGenieEffect{true};   // Genie appear/disappear animation
    bool enableWobblyMove{true};    // Wobbly window move (spring deformation)
    // Genie effect (appear/disappear)
    int32_t genieCellsV{20};
    int32_t genieCellsH{1};
    float genieAnimDuration{0.5f};
    ImGenieSide genieSide{ImGenieSide_Auto};
    ImGenieAnimMode animMode{ImGenieAnimMode_Compress};
    // Wobbly move
    int32_t wobblyCellsV{20};
    int32_t wobblyCellsH{20};
    float wobblyMaxStiffness{200.0f};
    float wobblyMinStiffness{50.0f};
    float wobblyDamping{10.0f};
    int32_t wobblySubsteps{8};
    float wobblySettleDuration{0.15f};
};

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
    // Per-effect settings snapshot
    ImGenieSettings settings{};
};

// POD context struct (ImGui-style, public members)
struct ImGenieContext {
    ImGenieSettings defaultSettings{};
    ImGenieSettings globalSettings{};
    std::unordered_map<ImGuiID, ImGenieEffect> effects{};
    std::unordered_map<ImGuiID, bool> openStates{};
    std::unordered_map<ImGuiID, const char*> effectNames{};
    std::unordered_map<ImGuiID, ImVec2> dragStartPos{};
    CreateCaptureFunctor createCaptureFunc{};
    DestroyCaptureFunctor destroyCaptureFunc{};
    bool captureFlipV{false};  // OpenGL needs true (Y-axis inverted), Vulkan/Metal/DX12 = false
};

// Check that version and struct sizes match between compiled ImGenie code and caller.
#define IMGENIE_CHECKVERSION() ImGenie::DebugCheckVersion(IMGENIE_VERSION, sizeof(ImGenieSettings), sizeof(ImGenieEffect), sizeof(ImGenieContext))

// ImGenie API
namespace ImGenie {

// Version check
IMGENIE_API bool DebugCheckVersion(const char* aVersion, size_t aSettingsSize, size_t aEffectSize, size_t aContextSize);

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
IMGENIE_API void ShowDemoWindow(bool* apoOpen = nullptr, ImGenieSettings* apoSettings = nullptr, ImGenieSettings* apoDefaultSettings = nullptr);

// to Call every frame UNCONDITIONALLY, BEFORE the if(show) guard.
// Return true if you can show your window normally.
// Return false if ImGenie is handling the window (animating).
IMGENIE_API bool Allow(const char* aWindowName, const ImRect& arDstRect, bool* apoOpen, const ImGenieSettings* apSettings = nullptr);

// Wrapper combining Allow + ImGui::Begin/End for simpler usage.
// IMPORTANT: Unlike ImGui::Begin/End, End() must only be called if Begin() returned true.
// Calling End() when Begin() returned false will trigger an ImGui assert.
// Usage:
//   if (ImGenie::Begin("Window", buttonRect, &show)) {
//       /* content */
//       ImGenie::End();
//   }
IMGENIE_API bool Begin(const char* aWindowName, const ImRect& arDstRect, bool* apoOpen = NULL, ImGuiWindowFlags flags = 0, const ImGenieSettings* apSettings = nullptr);
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
typedef struct ImGenieSettings ImGenieSettings;
#define IMGENIE_C_API
#endif  // __cplusplus

// C API callback types — use void* for opaque ImGui types (ImTextureRef, ImDrawData)
// The wrapper binding is responsible for casting to/from the correct types.
typedef void* (*ImGenie_CreateCaptureFunc)(int32_t aWidth, int32_t aHeight, void* apDrawData);
typedef void (*ImGenie_DestroyCaptureFunc)(void* apTex);

IMGENIE_C_API const char* ImGenie_GetVersion(void);
IMGENIE_C_API int ImGenie_GetVersionNum(void);
IMGENIE_C_API bool ImGenie_DebugCheckVersion(const char* aVersion, size_t aSettingsSize, size_t aEffectSize, size_t aContextSize);
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
IMGENIE_C_API ImGenieSettings* ImGenie_DefaultSettings(void);
IMGENIE_C_API void ImGenie_ShowDemoWindow(bool* apoOpen, ImGenieSettings* apoSettings, ImGenieSettings* apoDefaultSettings);
IMGENIE_C_API bool ImGenie_Allow(const char* aWindowName, const ImGenie_Rect* apDstRect, bool* apoOpen, const ImGenieSettings* apSettings);
IMGENIE_C_API bool ImGenie_Begin(const char* aWindowName, const ImGenie_Rect* apDstRect, bool* apoOpen, int aFlags, const ImGenieSettings* apSettings);
IMGENIE_C_API void ImGenie_End(void);
