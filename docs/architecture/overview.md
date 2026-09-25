# Architecture Overview

Lumiscripta is a minimal, cross-platform Markdown viewer written in C++17. It opens a document, renders it, and lets the user switch between raw source and a styled preview — nothing else. This document describes the architecture conceptually; see [application-flow.md](application-flow.md) and [rendering-pipeline.md](rendering-pipeline.md) for the dynamic views, and [class-diagram.md](class-diagram.md) for the structural one.

## Design Philosophy

- **The content is the interface.** No sidebars, ribbons, or floating panels. A single 40 px top bar (wordmark, Code/Preview toggle, theme switch) and a content area.
- **One pane at a time.** The `ViewMode` enum is exclusive: `Editor` *or* `Preview`, never both. The welcome screen (`Welcome`) is a third, pre-document state.
- **Immediate-mode UI.** Everything visual — colors, rounding, spacing, the line-number gutter — is drawn in code every frame via Dear ImGui, so both themes are literal palettes in `Graphics::setupStyleLight()` / `setupStyleDark()` rather than stylesheets.
- **Clarity over cleverness.** The project header in `src/main.cpp` states the code is a learning exercise that deliberately prioritizes readability. Consequences visible in the code: no namespaces (global scope), no exceptions (functions return `bool` and log to `std::cerr`), no RTTI, C++17 only.
- **Never crash on missing assets.** Fonts, icons, branding art, and document images all degrade to a fallback or a message instead of aborting the process.

## The Four Units

| Unit | Files | Responsibility |
|---|---|---|
| **App** | `include/lumiscripta/app.h`, `src/app.cpp` | Application shell: creates the GLFW window, owns the other units, runs the main loop, handles keyboard input, and draws the UI chrome (top bar, welcome screen, content-window layout). Holds the `ViewMode` state machine. |
| **Graphics** | `include/lumiscripta/graphics.h`, `src/graphics.cpp` | All rendering infrastructure: ImGui context and backends, font loading/merging, theme application, the editor and preview render paths, and the image/branding texture cache. Also hosts `MarkdownRenderer`, the `imgui_md` subclass that customizes preview output. |
| **File** | `include/lumiscripta/file.h`, `src/file.cpp` | A synchronous, blocking I/O wrapper around one document: load, save, get/set content, and dirty tracking by comparing the buffer with the last-saved copy. |
| **Utils** | `include/lumiscripta/utils.h` (header-only) | Pure, stateless free functions: string helpers (`trim`, `split`, `fileExtension`, `formatBytes`), path helpers (`parentDirectory`, `joinPath`, `pathExists`), and executable-relative asset resolution (`executableDir`, `assetDirs`, `resolveAsset`). No class, no state. |

`src/main.cpp` is a thin entry point: construct `LumiscriptaApp`, `init()`, optionally
`loadFile(argv[1])`, `run()`, `shutdown()`.

## Ownership and Relationships

```
main.cpp
   └── LumiscriptaApp  (unique_ptr owns both units)
         ├── File            — document buffer + dirty state
         ├── Graphics        — owns ImGui context, styles, image cache
         │     └── MarkdownRenderer (static in renderPreview; non-owning Graphics* back-pointer)
         └── GLFWwindow*     — created/destroyed by App; raw pointer = non-owning reference
```

- `LumiscriptaApp` owns a `File` and a `Graphics` through `std::unique_ptr` (convention: `std::unique_ptr` for owned heap objects, raw pointers only for non-owning references such as `GLFWwindow*` and the `Graphics*` inside the renderer).
- `Graphics` owns the ImGui context (`ImGuiContext*`), the theme, and the image cache (`std::unordered_map<string, CachedImage>` keyed by resolved path).
- `MarkdownRenderer` instances never own anything; they reach back into their `Graphics` to fetch image textures.
- Font handles (`g_font_regular`, `g_font_bold`, `g_font_bold_large`, `g_font_mono`, `g_font_mono_large`) are global `ImFont*` defined in `graphics.cpp` and `extern`-declared in `graphics.h`, shared by both rendering paths.

## Major Dependencies

Compiled in by the `Makefile` (the authoritative list):

| Dependency | Role |
|---|---|
| **Dear ImGui** (+ GLFW and OpenGL3 backends, `imgui_stdlib`, FreeType loader) | The entire UI and rendering model |
| **GLFW** | Window, input, OpenGL context (system dependency) |
| **OpenGL 3.3 core** | Final rasterization; GLSL `#version 330` |
| **FreeType** (system dependency) | Font rasterization via ImGui's `misc/freetype` backend — required for colour emoji |
| **md4c** (compiled as C) | Markdown parser |
| **imgui_md** | Turns md4c parse events into ImGui draw calls; subclassed by `MarkdownRenderer` |
| **stb_image** | Decodes the window icon, document images, and branding wordmarks (implementation compiled exactly once, in `graphics.cpp`) |
| **Font Awesome 7 / Twemoji Mozilla** (assets) | Icon glyphs merged into the UI font; COLRv0 colour emoji merged into every text font |

Assets are located through `resolveAsset()` in `utils.h`, which searches relative to the executable (and `$LUMISCRIPTA_ASSETS`, system directories) rather than the working directory — a desktop launcher otherwise starts the app somewhere without `assets/`.

## Cross-Cutting Conventions

- **Error signaling:** `bool` returns + `std::cerr` logging; no exceptions. Missing assets and broken images degrade gracefully.
- **Frame ownership:** `App` drives the loop; `Graphics::beginFrame()`/`endFrame()` wrap `ImGui::NewFrame()` and `ImGui::Render()` + draw-data submission.
- **State flow:** the editor edits a *copy* of the document buffer each frame and writes it back through `File::setContent()`; the preview reads it `const`.

## Related Documents

- [Class diagram](class-diagram.md) — structural view of the same units
- [Application flow](application-flow.md) — lifecycle and dispatch
- [Rendering pipeline](rendering-pipeline.md), [Image pipeline](image-pipeline.md), [Font system](font-system.md) — subsystem deep dives
- [Decisions](decisions.md) — why the architecture looks this way
- [Documentation index](../README.md)