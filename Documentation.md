# ImGenie Documentation

macOS-style Genie effect, Page Curl transition and Wobbly windows for [Dear ImGui](https://github.com/ocornut/imgui).

## Table of Contents

- [Features](#features)
- [Integration](#integration)
  - [Setup (OpenGL example)](#setup-opengl-example)
  - [Render loop](#render-loop)
  - [Shutdown](#shutdown)
- [Usage](#usage)
  - [Simple usage (Begin/End)](#simple-usage-beginend)
  - [Advanced usage (Allow)](#advanced-usage-allow)
- [Settings](#settings)
  - [ImGenieParams](#imgeniesettings)
  - [Transitions](#transitions)
  - [Effects](#effects)
- [Enums](#enums)
- [Windows without bool\* p\_open](#windows-without-bool-p_open)
- [Mid-animation reversal](#mid-animation-reversal)
- [C API](#c-api)
- [Defines](#defines)

## Features

- **Genie transition**: macOS-style minimize/restore animation toward a target rect (dock icon, button, etc.)
- **Page Curl transition**: Page curl/unroll appear/disappear animation with configurable origin (corners and edges)
- **Fade transition**: Alpha fade in/out
- **Scale transition**: Zoom in/out from window center
- **Slide transition**: Slide off-screen toward edges or corners, with optional wobbly spring-based elastic stretch
- **Wobbly windows effect**: Spring-based window deformation when dragging
- **Mid-animation reversal**: Toggling a window open/close during an animation instantly reverses its direction
- **Windows without `bool* p_open`**: Full animation support via `ImGenie::Close()` / `ImGenie::Open()` API

Transitions and effects are independent: transitions control appear/disappear animations, effects control runtime behavior (e.g. wobbly drag).

## Integration

see  Backend Capture section at the end of this file for a sample fucntions for Opengl3 and Vulkan

> For Vulkan, Metal or DirectX 12, adapt the capture/destroy functions to your rendering backend and set `SetCaptureFlipV(false)` (default).

### Creation

```cpp
IMGUI_CHECKVERSION();
ImGui::CreateContext();

IMGENIE_CHECKVERSION();
ImGenie::CreateContext();

ImGenie::SetCreateCaptureFunc([](int32_t aWidth, int32_t aHeight, ImDrawData* apDrawData) {
    ImTextureRef ret{};
	// your code here for create frame buffer with size aWidth and aHeight ...
    ImGui_Impl<Your_Backend>_RenderDrawData(apDrawData); // render the drawList onto the Buffer 
	// your code here for delete the buffer ...
    return ret; // return the texture
});
ImGenie::SetCaptureFlipV(true);  // true for OpenGL (Y-axis is inverted), false for Vulkan by ex

ImGenie::SetDestroyCaptureFunc([](const ImTextureRef& aTex) {
	// your code here for detroy your texture ...
});
```

### Render loop

```cpp
// CPU: prepare draw data
ImGui::Render();

// Capture windows to offscreen textures (AFTER Render, BEFORE RenderDrawData)
ImGenie::Capture();

// GPU: render
glfwMakeContextCurrent(window);
glViewport(0, 0, displayW, displayH);
glClear(GL_COLOR_BUFFER_BIT);
ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
```

### Shutdown

```cpp
ImGenie::DestroyContext();
ImGui::DestroyContext();
```

## Usage

### Simple usage (Begin/End)

`ImGenie::Begin` / `ImGenie::End` wraps `ImGui::Begin` / `ImGui::End` and handles the effect automatically.

**Important**: unlike `ImGui::Begin`/`End`, you must only call `ImGenie::End()` when `ImGenie::Begin()` returned `true`. `ImGenie::Begin()` handles collapsed windows internally (calls `ImGui::End()` when the window is collapsed), so you don't need to worry about Begin/End stack balance.

```cpp
// For genie transition: set the target rect the window animates to/from
ImGenieParams params;  // or use global params via context
params.transitions.genie.destRect = {buttonPos.x, buttonPos.y,
                                     buttonPos.x + buttonSize.x,
                                     buttonPos.y + buttonSize.y};

// show is a bool controlling the window visibility
if (ImGenie::Begin("My Window", &show, ImGuiWindowFlags_None, &params)) {
    ImGui::Text("Hello!");
    ImGenie::End();
}
```

### Advanced usage (Allow)

`ImGenie::Allow` gives you full control over `ImGui::Begin`/`End` while ImGenie handles the animation.

Call `Allow` **every frame**, **unconditionally**, **before** your `if (show)` guard.

```cpp
ImGenieParams params;
params.transitions.genie.destRect = {buttonPos.x, buttonPos.y,
                                     buttonPos.x + buttonSize.x,
                                     buttonPos.y + buttonSize.y};

// Allow returns true when you should show your window normally.
// Returns false when ImGenie is handling the animation.
if (ImGenie::Allow("My Window", &show, &params)) {
    if (show) {
        ImGui::Begin("My Window", &show);
        ImGui::Text("Hello!");
        ImGui::End();
    }
}
```

### Windows without bool* p_open

By default, ImGui's `bool* p_open` parameter controls the close button in the title bar. ImGenie can animate windows even without a `bool*` — it manages an internal open/close state per window.

Use `ImGenie::Close()` and `ImGenie::Open()` to trigger transitions:

```cpp
// Simple window without bool* p_open
if (ImGenie::Begin("My Window")) {
    ImGui::Text("Hello!");
    ImGenie::End();
}

// Trigger close animation from a button, menu, etc.
if (ImGui::Button("Hide Window")) {
    ImGenie::Close("My Window");
}

// Trigger open animation
if (ImGui::Button("Show Window")) {
    ImGenie::Open("My Window");
}
```

With `Allow`:

```cpp
if (ImGenie::Allow("My Window", nullptr, &params)) {
    ImGui::Begin("My Window");
    ImGui::Text("Hello!");
    ImGui::End();
}

// Control visibility externally
ImGenie::Close("My Window");  // triggers disappear animation
ImGenie::Open("My Window");   // triggers appear animation
```

The internal state defaults to `true` (open). When `Close()` is called, the next frame detects the transition and starts the disappear animation. `Open()` does the reverse.

### Mid-animation reversal

If the user toggles a window's visibility while an animation is still playing, ImGenie instantly reverses the animation direction. The animation continues smoothly from the current position — no need to wait for it to complete.

This works both with `bool* p_open` (toggling the bool) and with `Close()` / `Open()` (calling the opposite function mid-animation).

## Settings

Settings can be passed per-window via the `apSettings` parameter of `Allow` / `Begin`, or set globally via `ImGenie::GetCurrentContext()->globalSettings`.

### ImGenieParams

| Field | Type | Description |
|---|---|---|
| `drawDebug` | `bool` | Draw the deformation mesh wireframe (debug) |
| `transitions` | `ImGenieTransitions` | Appear/disappear transition settings |
| `effects` | `ImGenieEffects` | Runtime effect settings |

### Transitions

Controlled by `settings.transitions.transitionMode`:

| Mode | Description |
|---|---|
| `ImGenieTransitionMode_None` | No transition, instant show/hide |
| `ImGenieTransitionMode_Genie` | macOS-style genie animation toward a target rect |
| `ImGenieTransitionMode_PageCurl` | Page curl animation from a configurable origin |
| `ImGenieTransitionMode_Fade` | Alpha fade in/out |
| `ImGenieTransitionMode_Scale` | Zoom in/out from window center |
| `ImGenieTransitionMode_Slide` | Slide off-screen in a configurable direction |

#### Common parameters (`settings.transitions`)

| Field | Type | Default | Description |
|---|---|---|---|
| `animDuration` | `float` | 0.4s | Animation duration in seconds (shared by all transitions) |

#### Genie parameters (`settings.transitions.genie`)

| Field | Type | Default | Description |
|---|---|---|---|
| `cellsV` | `int32_t` | 20 | Vertical mesh subdivisions |
| `cellsH` | `int32_t` | 1 | Horizontal mesh subdivisions |
| `side` | `ImGenieSide` | Auto | Side the window collapses toward |
| `animMode` | `ImGenieAnimMode` | Compress | UV mapping mode (Compress or Sliding) |
| `destRect` | `ImGenie_Rect` | {0,0,0,0} | Target rect the window animates to/from (update every frame) |

#### Page Curl parameters (`settings.transitions.pageCurl`)

| Field | Type | Default | Description |
|---|---|---|---|
| `cellsH` | `int32_t` | 30 | Horizontal mesh subdivisions |
| `cellsV` | `int32_t` | 30 | Vertical mesh subdivisions |
| `origin` | `ImGeniePageCurlOrigin` | BottomLeft | Corner or edge the curl starts from |

#### Fade parameters

No specific parameters. Uses common `animDuration`.

#### Scale parameters

No specific parameters. Uses common `animDuration`. Zooms from/to the window center.

#### Slide parameters (`settings.transitions.slide`)

| Field | Type | Default | Description |
|---|---|---|---|
| `dir` | `ImGenieSlideDir` | Auto | Direction the window slides to when disappearing |
| `wobbly` | `bool` | false | Enable spring-based elastic stretch during slide (leading corner(s) move, trailing follow via springs) |
| `spring.stiffness` | `float` | 120.0 | Spring stiffness (wobbly slide only) |
| `spring.damping` | `float` | 8.0 | Spring damping (wobbly slide only) |
| `spring.substeps` | `int32_t` | 8 | Physics substeps per frame (wobbly slide only) |
| `spring.cellsH` | `int32_t` | 20 | Horizontal mesh subdivisions (wobbly slide only) |
| `spring.cellsV` | `int32_t` | 20 | Vertical mesh subdivisions (wobbly slide only) |

### Effects

Controlled by `settings.effects.effectMode`:

| Mode | Description |
|---|---|
| `ImGenieEffectMode_None` | No runtime effect |
| `ImGenieEffectMode_Wobbly` | Spring-based deformation when dragging |

#### Wobbly parameters (`settings.effects.wobbly`)

| Field | Type | Default | Description |
|---|---|---|---|
| `spring.cellsH` | `int32_t` | 20 | Horizontal mesh subdivisions |
| `spring.cellsV` | `int32_t` | 20 | Vertical mesh subdivisions |
| `spring.stiffness` | `float` | 120.0 | Minimum spring stiffness (far from grab point) |
| `spring.damping` | `float` | 8.0 | Spring damping factor |
| `spring.substeps` | `int32_t` | 8 | Physics substeps per frame |
| `maxStiffness` | `float` | 200.0 | Maximum spring stiffness (at grab point) |
| `settleDuration` | `float` | 0.15s | Duration of settle animation after drag ends |

## Enums

### ImGenieSide

Target side for the Genie transition.

| Value | Description |
|---|---|
| `ImGenieSide_Auto` | Auto-detect closest side |
| `ImGenieSide_Top` | Collapse toward top |
| `ImGenieSide_Bottom` | Collapse toward bottom |
| `ImGenieSide_Left` | Collapse toward left |
| `ImGenieSide_Right` | Collapse toward right |

### ImGenieAnimMode

UV mapping mode for the Genie transition.

| Value | Description |
|---|---|
| `ImGenieAnimMode_Compress` | Texture shrinks with the mesh |
| `ImGenieAnimMode_Sliding` | Texture scrolls through the mesh |

### ImGeniePageCurlOrigin

Origin corner or edge for the Page Curl transition.

| Value | Description |
|---|---|
| `ImGeniePageCurlOrigin_TopLeft` | Curl from top-left corner |
| `ImGeniePageCurlOrigin_Top` | Curl from top edge |
| `ImGeniePageCurlOrigin_TopRight` | Curl from top-right corner |
| `ImGeniePageCurlOrigin_Right` | Curl from right edge |
| `ImGeniePageCurlOrigin_BottomRight` | Curl from bottom-right corner |
| `ImGeniePageCurlOrigin_Bottom` | Curl from bottom edge |
| `ImGeniePageCurlOrigin_BottomLeft` | Curl from bottom-left corner |
| `ImGeniePageCurlOrigin_Left` | Curl from left edge |

### ImGenieSlideDir

Direction for the Slide transition.

| Value | Description |
|---|---|
| `ImGenieSlideDir_Auto` | Auto-detect closest viewport edge or corner |
| `ImGenieSlideDir_Left` | Slide toward left edge |
| `ImGenieSlideDir_Right` | Slide toward right edge |
| `ImGenieSlideDir_Top` | Slide toward top edge |
| `ImGenieSlideDir_Bottom` | Slide toward bottom edge |
| `ImGenieSlideDir_TopLeft` | Slide toward top-left corner |
| `ImGenieSlideDir_TopRight` | Slide toward top-right corner |
| `ImGenieSlideDir_BottomLeft` | Slide toward bottom-left corner |
| `ImGenieSlideDir_BottomRight` | Slide toward bottom-right corner |

For edge directions, 2 corners are pinned (the leading edge). For corner directions, only 1 corner is pinned — the other 3 follow via springs when wobbly is enabled.

### ImGenieTransitionMode

| Value | Description |
|---|---|
| `ImGenieTransitionMode_None` | No transition, instant show/hide |
| `ImGenieTransitionMode_Genie` | macOS-style genie animation toward a target rect |
| `ImGenieTransitionMode_PageCurl` | Page curl animation from a configurable origin |
| `ImGenieTransitionMode_Fade` | Alpha fade in/out |
| `ImGenieTransitionMode_Scale` | Zoom in/out from window center |
| `ImGenieTransitionMode_Slide` | Slide off-screen in a configurable direction |

### ImGenieEffectMode

| Value | Description |
|---|---|
| `ImGenieEffectMode_None` | No runtime effect |
| `ImGenieEffectMode_Wobbly` | Spring-based deformation when dragging |

## C API

All functions are available as C-compatible functions prefixed with `ImGenie_`. Settings structs are fully accessible in C.

Use `ImGenie_DefaultParams()` to get a pointer to a properly initialized `ImGenieParams` instance.

```c
ImGenieContext* ctx = ImGenie_CreateContext();
ImGenieParams* params = ImGenie_DefaultParams();
params->transitions.transitionMode = ImGenieTransitionMode_PageCurl;
params->transitions.pageCurl.origin = ImGeniePageCurlOrigin_TopRight;
```

| C++ | C |
|---|---|
| `ImGenie::CreateContext()` | `ImGenie_CreateContext()` |
| `ImGenie::DestroyContext()` | `ImGenie_DestroyContext(ctx)` |
| `ImGenie::SetCaptureFlipV(v)` | `ImGenie_SetCaptureFlipV(v)` |
| `ImGenie::Capture()` | `ImGenie_Capture()` |
| `ImGenie::HasActiveEffects()` | `ImGenie_HasActiveEffects()` |
| `ImGenie::IsEffectActive(name)` | `ImGenie_IsEffectActive(name)` |
| `ImGenie::Close(name)` | `ImGenie_Close(name)` |
| `ImGenie::Open(name)` | `ImGenie_Open(name)` |
| `ImGenie::ShowDemoWindow(...)` | `ImGenie_ShowDemoWindow(...)` |
| `ImGenie::Allow(...)` | `ImGenie_Allow(...)` |
| `ImGenie::Begin(...)` / `End()` | `ImGenie_Begin(...)` / `ImGenie_End()` |

## Defines

| Define | Description |
|---|---|
| `IMGENIE_API` | DLL export/import macro (empty by default) |
| `IMGUI_INCLUDE` | Override the imgui include path (defaults to `<imgui_internal.h>`) |
| `IMGENIE_VERSION` | Version string (e.g. `"0.1.0"`) |
| `IMGENIE_VERSION_NUM` | Version number (e.g. `00100`) |
| `IMGENIE_CHECKVERSION()` | Macro to verify struct size/version compatibility |

## Backend Capture Functions

ImGenie needs two backend-specific functions to capture and destroy window textures. Below are ready-to-use implementations.

### OpenGL 3+

```cpp
ImGenie::SetCreateCaptureFunc([](int32_t aWidth, int32_t aHeight, ImDrawData* apDrawData) {
    ImTextureRef ret{};

    GLuint fboTex{};
    glGenTextures(1, &fboTex);
    glBindTexture(GL_TEXTURE_2D, fboTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, aWidth, aHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

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

    GLint prevViewport[4];
    glGetIntegerv(GL_VIEWPORT, prevViewport);
    glViewport(0, 0, aWidth, aHeight);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(apDrawData);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]);

    glDeleteFramebuffers(1, &fbo);
    ret._TexID = fboTex;
    return ret;
});

ImGenie::SetCaptureFlipV(true);  // OpenGL Y-axis is inverted

ImGenie::SetDestroyCaptureFunc([](const ImTextureRef& aTex) {
    GLuint texID = static_cast<GLuint>(static_cast<uintptr_t>(aTex._TexID));
    glDeleteTextures(1, &texID);
});
```

### Vulkan

```cpp
// Assumes you have access to:
//   VkDevice device, VkPhysicalDevice physicalDevice, VkQueue queue,
//   VkCommandPool commandPool, VkRenderPass renderPass
// and a helper to find a suitable memory type.

ImGenie::SetCreateCaptureFunc([&](int32_t aWidth, int32_t aHeight, ImDrawData* apDrawData) {
    ImTextureRef ret{};

    // Create image
    VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM; // same a simgui backend 
    imageInfo.extent = {(uint32_t)aWidth, (uint32_t)aHeight, 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    VkImage image;
    vkCreateImage(device, &imageInfo, nullptr, &image);

    // Allocate and bind memory
    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(device, image, &memReqs);
    VkMemoryAllocateInfo allocInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VkDeviceMemory memory;
    vkAllocateMemory(device, &allocInfo, nullptr, &memory);
    vkBindImageMemory(device, image, memory, 0);

    // Create image view
    VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    VkImageView imageView;
    vkCreateImageView(device, &viewInfo, nullptr, &imageView);

    // Create framebuffer
    VkFramebufferCreateInfo fbInfo{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    fbInfo.renderPass = renderPass;
    fbInfo.attachmentCount = 1;
    fbInfo.pAttachments = &imageView;
    fbInfo.width = (uint32_t)aWidth;
    fbInfo.height = (uint32_t)aHeight;
    fbInfo.layers = 1;
    VkFramebuffer framebuffer;
    vkCreateFramebuffer(device, &fbInfo, nullptr, &framebuffer);

    // Record and submit render commands
    VkCommandBufferAllocateInfo cmdInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    cmdInfo.commandPool = commandPool;
    cmdInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdInfo.commandBufferCount = 1;
    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(device, &cmdInfo, &cmd);

    VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &beginInfo);

    // Transition image layout to color attachment
    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.image = image;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0,
                         0, nullptr, 0, nullptr, 1, &barrier);

    VkRenderPassBeginInfo rpBegin{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    rpBegin.renderPass = renderPass;
    rpBegin.framebuffer = framebuffer;
    rpBegin.renderArea.extent = {(uint32_t)aWidth, (uint32_t)aHeight};
    VkClearValue clearValue{};
    rpBegin.clearValueCount = 1;
    rpBegin.pClearValues = &clearValue;
    vkCmdBeginRenderPass(cmd, &rpBegin, VK_SUBPASS_CONTENTS_INLINE);

    ImGui_ImplVulkan_RenderDrawData(apDrawData, cmd);

    vkCmdEndRenderPass(cmd);

    // Transition to shader read for sampling
    barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
                         0, nullptr, 0, nullptr, 1, &barrier);

    vkEndCommandBuffer(cmd);

    VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;
    vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue);

    vkFreeCommandBuffers(device, commandPool, 1, &cmd);
    vkDestroyFramebuffer(device, framebuffer, nullptr);

    // Create descriptor set for ImGui sampling (ImGui_ImplVulkan_AddTexture)
    VkSampler sampler = /* your linear sampler */;
    VkDescriptorSet descSet = ImGui_ImplVulkan_AddTexture(sampler, imageView,
                                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    ret._TexID = descSet;
    // Store image, memory, imageView somewhere for cleanup (e.g. in a side map)
    return ret;
});

ImGenie::SetCaptureFlipV(false);  // Vulkan Y-axis is not inverted

ImGenie::SetDestroyCaptureFunc([&](const ImTextureRef& aTex) {
    VkDescriptorSet descSet = static_cast<VkDescriptorSet>(aTex._TexID);
    ImGui_ImplVulkan_RemoveTexture(descSet);
    // Also destroy image, imageView, memory from your side map
});
```

> **Note**: The Vulkan example is a simplified skeleton. In production you would use your existing resource management (allocator, descriptor pools, etc.) and avoid `vkQueueWaitIdle` by using fences or timeline semaphores.
