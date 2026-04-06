# ImGenie

ImGenie is a lib for [Dear ImGui](https://github.com/ocornut/imgui) who add cool effects and transition for windows

Transitions :
* transition Genie (macOS-style Genie effect)
* transition Page-Curl
* transition Fade
* transition Scale
* transition Slide

Effects :
* effect Wobbly (Linux/Gnome-style Wobbly windows).

See [Documentation.md](Documentation.md) for integration, usage, settings reference and C API.

A full demo is available in the [ImEffects](https://github.com/aiekick/ImEffects) repository or with this [Emscripten](https://aiekick.github.io/ImEffects/) demo.

# Transitions 

**Transition Genie (macOS-style Genie effect)**

![Demo](https://github.com/aiekick/ImEffects/blob/master/doc/ImGenie_transition_genie.gif)

**Transition Page-Curl**

![Demo](https://github.com/aiekick/ImEffects/blob/master/doc/ImGenie_transition_page-curl.gif)

**Transition Fade**

![Demo](https://github.com/aiekick/ImEffects/blob/master/doc/ImGenie_transition_fade.gif)

**Transition Scale**

![Demo](https://github.com/aiekick/ImEffects/blob/master/doc/ImGenie_transition_scale.gif)

**Transition Slide**

![Demo](https://github.com/aiekick/ImEffects/blob/master/doc/ImGenie_transition_slide.gif)

# Effects 

**Wobbly window (Linux/Gnome-style Wobbly windows)**

![Demo](https://github.com/aiekick/ImEffects/blob/master/doc/ImGenie_effect_wobbly.gif)

## Limitations

- **Not compatible with Docking** -- ImGenie does not yet support ImGui's docking feature.
- **Not compatible with Viewports** -- Multi-viewport is not handled.

## License

MIT License -- Copyright (c) 2025-2026 Stephane Cuillerdier (aka Aiekick)
