# ImGenie

macOS-style **Genie effect** and Linux/Gnome-style **Wobbly windows** for [Dear ImGui](https://github.com/ocornut/imgui).

ImGenie adds animated window transitions to ImGui:
- windows shrink into a dock icon when closed (genie effect), expand back when opened,
- wobble like jelly when dragged (spring physics). Inspired by MacOS and Linux/Gnome window animations.

## Features

- **Genie effect (appear/disappear)** -- Windows smoothly morph into/from a target rectangle (dock icon) using a deformed Coons patch mesh with cubic Bezier S-curves.
- **Wobbly window drag** -- When dragging a window, it deforms elastically with spring physics on 4 corners. Corners near the grab point are stiffer, distant corners lag behind. On release, springs settle with an easing animation.
- **4-side support** -- Genie animation works from Top, Bottom, Left, or Right (auto-detected from relative position, or forced).
- **Enable/disable per effect** -- `enableGenieEffect` and `enableWobblyMove` flags let you use only genie, only wobbly, or both.
- **Per-window settings** -- Each window can have its own animation parameters, or use global defaults.
- **Debug mesh overlay** -- Visualize the deformed mesh for debugging.
- **Built-in demo window** -- `ShowDemoWindow()` lets you tweak all settings at runtime, with save/reset defaults.
- **Backend-agnostic capture** -- You provide the FBO capture/destroy callbacks. Works with any renderer (OpenGL example included).
- **C API** -- All functions available as plain C wrappers for binding generators (cimgui, etc.).
- **DLL-ready** -- `IMGENIE_API` macro for `__declspec(dllexport)` support.

**Genie effect (appear/disappear)**

![Demo](https://github.com/aiekick/ImEffects/blob/master/doc/ImGenie_genie.gif)

**Wobbly window drag**

![Demo](https://github.com/aiekick/ImEffects/blob/master/doc/ImGenie_wobbly.gif)

## Demo App

A full demo is available in the [ImEffects](https://github.com/aiekick/ImEffects) repository or with this [Emscripten](https://aiekick.github.io/ImEffects/) demo

## Limitations (current)

- **Not compatible with Docking** -- ImGenie does not yet support ImGui's docking feature (`ImGuiConfigFlags_DockingEnable`). Docked windows cannot be captured or animated correctly.
- **Not compatible with Viewports** -- ImGenie does not support ImGui's multi-viewport feature (`ImGuiConfigFlags_ViewportsEnable`). Windows rendered in OS-level windows outside the main viewport are not handled.

## Roadmap

- Docking support
- Menu items and popups
- Modal windows
- Viewports (external OS windows) -- long-term goal

## How it works

1. **Capture** -- When a transition starts, ImGenie captures the window's DrawList into an offscreen texture (via your callback).
2. **Hide** -- The real window's DrawList is removed from ImDrawData so the backend doesn't render it.
3. **Animate** -- The captured texture is drawn on a deformed mesh (Coons patch) on the ForegroundDrawList. The mesh vertices are animated over time (genie) or driven by spring physics (wobbly).
4. **Restore** -- When animation completes, the effect is removed and the real window reappears.

## Integration

### 1. Add files

Copy `ImGenie.h` and `imGenie.cpp` into your project. Include `ImGenie.h` after `imgui.h` / `imgui_internal.h`.

### 2. Setup

```cpp
// After ImGui context creation
IMGUI_CHECKVERSION();
ImGui::CreateContext();
ImGenie::CreateContext();
IMGENIE_CHECKVERSION();

// Register your capture callback (renders an ImDrawData to an offscreen texture)
ImGenie::SetCreateCaptureFunc([](int32_t aWidth, int32_t aHeight, ImDrawData* apDrawData) -> ImTextureRef {
    // 1. Create render target at aWidth x aHeight (FBO for OpenGL, render pass for Vulkan, etc.)
    // 2. Render: ImGui_ImplOpenGL3_RenderDrawData(apDrawData)
    // 3. Return the texture (no readback needed -- keep the render target texture directly)
    // See ImGenie.h header comment for a complete OpenGL example
});
ImGenie::SetCaptureFlipV(true);  // OpenGL Y-axis is inverted (set false for Vulkan/Metal/DX12)

// Register your texture cleanup callback
ImGenie::SetDestroyCaptureFunc([](const ImTextureRef& aTex) {
    GLuint texID = static_cast<GLuint>(static_cast<uintptr_t>(aTex._TexID));
    glDeleteTextures(1, &texID);
});
```

### 3. Per-window usage

**Option A: `ImGenie::Begin/End` wrapper (simplest)**

> **Important:** Unlike `ImGui::Begin/End`, `ImGenie::End()` must only be called when `ImGenie::Begin()` returned `true`. Calling `End()` when `Begin()` returned `false` will trigger an ImGui assert, because no `ImGui::Begin()` was called internally.

```cpp
// Wraps Allow + ImGui::Begin/End in a single call
if (ImGenie::Begin("My Window", buttonRect, &show)) {
    // ... window content ...
    ImGenie::End();  // only call End if Begin returned true
}
```

**Option B: `ImGenie::Allow` (manual control)**

Call `Allow()` every frame, unconditionally, before your `if (show)` guard.
This gives you full control over the ImGui::Begin/End calls:

```cpp
// Returns true if you can show the window, false if ImGenie is animating
if (ImGenie::Allow("My Window", buttonRect, &show)) {
    if (show) {
        if (ImGui::Begin("My Window", &show)) {
            // ... window content ...
        }
        ImGui::End();
    }
}
```

### 4. Render loop

```cpp
// CPU: prepare frame
ImGui::NewFrame();
// ... your UI code with Allow() calls ...
ImGui::Render();

// Capture step: AFTER Render(), BEFORE RenderDrawData()
ImGenie::Capture();

// GPU: render
ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
```

### 5. Cleanup

```cpp
ImGenie::DestroyContext();
ImGui::DestroyContext();
```

## API

```cpp
namespace ImGenie {
    // Context
    ImGenieContext* CreateContext();
    void DestroyContext(ImGenieContext* apCtx = nullptr);
    ImGenieContext* GetCurrentContext();
    void SetCurrentContext(ImGenieContext* apCtx);

    // Capture callbacks
    void SetCreateCaptureFunc(const CreateCaptureFunctor& arFunc);
    void SetDestroyCaptureFunc(const DestroyCaptureFunctor& arFunc);
    void SetCaptureFlipV(bool aFlipV);  // true for OpenGL, false for Vulkan/Metal/DX12
    void Capture();  // Call AFTER ImGui::Render() and BEFORE RenderDrawData()

    // Per-window control
    bool Allow(const char* aWindowName, const ImRect& arDstRect,
               bool* apoOpen, const ImGenieSettings* apSettings = nullptr);
    bool Begin(const char* aWindowName, const ImRect& arDstRect, bool* apoOpen,
               ImGuiWindowFlags aFlags = 0, const ImGenieSettings* apSettings = nullptr);
    void End();

    // Query
    bool HasActiveEffects();
    bool IsEffectActive(const char* aWindowName);

    // Demo
    void ShowDemoWindow(bool* apoOpen = nullptr, ImGenieSettings* apoSettings = nullptr,
                        ImGenieSettings* apoDefaultSettings = nullptr);

    // Version
    bool DebugCheckVersion(const char* aVersion, size_t aSettingsSize, size_t aEffectSize, size_t aContextSize);
}
```

## Settings

`ImGenieSettings` controls all animation parameters:

| Parameter | Default | Description |
|---|---|---|
| `drawDebugMesh` | `false` | Show wireframe mesh overlay |
| `enableGenieEffect` | `true` | Enable/disable genie appear/disappear animation |
| `enableWobblyMove` | `true` | Enable/disable wobbly window drag |
| `genieCellsV` | `20` | Vertical subdivisions for genie mesh |
| `genieCellsH` | `1` | Horizontal subdivisions for genie mesh |
| `genieAnimDuration` | `0.5` | Genie animation duration (seconds) |
| `genieSide` | `Auto` | Target side: Auto, Top, Bottom, Left, Right |
| `animMode` | `Compress` | UV mode: Compress (texture shrinks) or Sliding (texture scrolls) |
| `wobblyCellsV` | `20` | Vertical subdivisions for wobbly mesh |
| `wobblyCellsH` | `20` | Horizontal subdivisions for wobbly mesh |
| `wobblyMaxStiffness` | `200` | Spring stiffness near grab point |
| `wobblyMinStiffness` | `50` | Spring stiffness far from grab point |
| `wobblyDamping` | `10` | Spring damping factor |
| `wobblySubsteps` | `8` | Physics substeps per frame |
| `wobblySettleDuration` | `0.15` | Settle easing duration after drag release (seconds) |

Pass per-window settings via the last argument of `Allow()` or `Begin()`, or modify `ImGenie::GetCurrentContext()->globalSettings` for global defaults. Use `ShowDemoWindow()` to tweak settings at runtime.

## C API

All functions are available as C wrappers prefixed with `ImGenie_`:

```c
ImGenieContext* ImGenie_CreateContext(void);
void ImGenie_DestroyContext(ImGenieContext* apCtx);
void ImGenie_SetCreateCaptureFunc(ImGenie_CreateCaptureFunc aFunc);
void ImGenie_SetDestroyCaptureFunc(ImGenie_DestroyCaptureFunc aFunc);
void ImGenie_SetCaptureFlipV(bool aFlipV);
void ImGenie_Capture(void);
bool ImGenie_Allow(const char* aWindowName, float aDstRectMinX, float aDstRectMinY,
                   float aDstRectMaxX, float aDstRectMaxY, bool* apoOpen);
bool ImGenie_Begin(const char* aWindowName, float aDstRectMinX, float aDstRectMinY,
                   float aDstRectMaxX, float aDstRectMaxY, bool* apoOpen, int aFlags);
void ImGenie_End(void);
bool ImGenie_HasActiveEffects(void);
bool ImGenie_IsEffectActive(const char* aWindowName);
void ImGenie_ShowDemoWindow(bool* apoOpen, void* apoSettings, void* apoDefaultSettings);
const char* ImGenie_GetVersion(void);
int ImGenie_GetVersionNum(void);
```

## License

MIT License -- Copyright (c) 2025-2026 Stephane Cuillerdier (aka Aiekick)
