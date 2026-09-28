# Project Structure

This document outlines the organization of the Lumiscripta repository, mapping directories and critical files to their operational roles.

```
lumiscripta/
├── include/                  # Public and modular C++ headers
│   └── lumiscripta/
│       ├── app.h             # LumiscriptaApp application coordinator
│       ├── file.h            # File synchronous buffer wrapper
│       ├── graphics.h        # Graphics manager and MarkdownRenderer interface
│       └── utils.h           # Pure utility helpers, path resolution, and asset discovery
├── src/                      # Implementation sources
│   ├── app.cpp               # Window management, main loop, modal dialog, and input
│   ├── file.cpp              # Disk I/O implementation
│   ├── graphics.cpp          # ImGui backend setup, themes, fonts, image cache, rendering
│   └── main.cpp              # Entry point: command-line argument handling and app lifecycle
├── assets/                   # Static runtime resources
│   ├── branding/             # Application logos, dark/light wordmarks
│   └── fonts/                # Inter, JetBrains Mono, FontAwesome, Twemoji Mozilla
├── third_party/              # Vendored external dependencies (no submodules required)
│   ├── imgui/                # Dear ImGui core, backends, FreeType font loader
│   ├── imgui_md/             # Immediate-mode Markdown rendering bridge
│   ├── md4c/                 # CommonMark-compliant Markdown parser in C
│   ├── stb/                  # stb_image single-header decoder
│   └── IconsFontAwesome/     # FontAwesome 7 icon glyph definition headers
├── docs/                     # Project technical documentation
│   ├── README.md             # Documentation index
│   ├── architecture/         # High-level architecture, pipeline, and design records
│   ├── development/          # Building, repository structure, coding, and contributing guides
│   └── screenshots/          # Interface preview images referenced in documentation
├── Makefile                  # Build specification with cross-platform detection
├── DESCRIPTION.md            # Visual design philosophy and interface palette definitions
├── RESUME.md                 # Original internal developer guide
└── README.md                 # Public repository overview
```

## Core Source Code (`src/` and `include/lumiscripta/`)

The repository adopts a strict separation between public interface definitions in `include/lumiscripta/` and implementation details in `src/`.

### 1. `App` (`include/lumiscripta/app.h`, `src/app.cpp`)
- Creates and manages the GLFW window.
- Manages top-level application state via `ViewMode` (`Welcome`, `Editor`, `Preview`).
- Orchestrates the event loop: `processInput()`, `renderUI()`, `renderMenuBar()`, `renderWelcome()`.

### 2. `Graphics` (`include/lumiscripta/graphics.h`, `src/graphics.cpp`)
- Manages the Dear ImGui context and GLFW/OpenGL3 backend bindings.
- Assembles the merged font atlas (Inter, JetBrains Mono, Font Awesome, and Twemoji).
- Implements theme color configuration for Light (Warm Pebble) and Dark (Slate Almond) modes.
- Contains the `MarkdownRenderer` subclass deriving from `imgui_md`.
- Maintains the OpenGL image texture cache (`m_images`) decoded using `stb_image`.
- Implements the monospaced editor window with custom word-wrapped line numbering in `renderEditor()`.

### 3. `File` (`include/lumiscripta/file.h`, `src/file.cpp`)
- Encapsulates synchronous reading (`load()`) and writing (`save()`) of file buffers.
- Tracks unsaved modifications by comparing active buffer contents against `m_originalContent`.

### 4. `Utils` (`include/lumiscripta/utils.h`)
- Header-only collection of pure free functions.
- String utilities (`trim`, `split`, `fileExtension`, `formatBytes`).
- Filesystem path utilities (`parentDirectory`, `joinPath`, `isAbsolutePath`, `pathExists`).
- Executable directory detection (`executableDir()`) across macOS (`_NSGetExecutablePath`), Linux (`/proc/self/exe`), and Windows (`GetModuleFileNameA`).
- Multi-tier asset resolution search chain (`resolveAsset()`).

## Assets (`assets/`)

Runtime assets loaded through `resolveAsset()`:
- `assets/branding/`: Contains `logo-light.png`, `wordmark-light.png`, and `wordmark-dark.png` used for window icons, the welcome screen, and the custom menu bar.
- `assets/fonts/`:
  - `Inter-Regular.ttf`, `Inter-Bold.ttf`, `Inter-Medium.ttf`: UI and Markdown body typography.
  - `JetBrainsMono-Regular.ttf`, `JetBrainsMono-Bold.ttf`: Monospaced editor, inline code, and code blocks.
  - `fa-solid-900.otf`: FontAwesome 7 icon set.
  - `TwemojiMozilla.ttf`: COLRv0 vector color emoji font rendered via FreeType.

## Third-Party Libraries (`third_party/`)

All third-party code is vendored in-tree to provide self-contained, reproducible builds:
- **`imgui/`**: Dear ImGui source files (`imgui.cpp`, `imgui_draw.cpp`, `imgui_widgets.cpp`, `imgui_tables.cpp`), OpenGL3 and GLFW backends, and `misc/freetype/imgui_freetype.cpp`.
- **`md4c/`**: Fast SAX-style Markdown parser written in standard C (`md4c.c`). Compiled via `$(CC)` using `-Wall -O2`.
- **`imgui_md/`**: C++ bridge transforming `md4c` parse events into Dear ImGui rendering calls.
- **`stb/`**: Header-only image loader (`stb_image.h`). Compiled into `src/graphics.cpp` via `#define STB_IMAGE_IMPLEMENTATION`.
- **`IconsFontAwesome/`**: C++ header definitions mapping FontAwesome unicode codepoints.

## Documentation (`docs/`)

Technical documentation organized into focused topic directories:
- `docs/README.md`: Central documentation catalog and cross-reference index.
- `docs/architecture/`: Deep dives into system design, flowcharts, class structure, font rendering, image caching, and architectural decision records.
- `docs/development/`: Guides covering building, repository structure, coding, and contributing guides.

## Related Documents

- [Building Lumiscripta](building.md) — build system usage and compilation instructions
- [Architecture Overview](../architecture/overview.md) — high-level conceptual design
- [Coding Guidelines](coding-guidelines.md) — conventions across the codebase

- Implements the confirmation modal dialog (`renderLinkDialog()`) for web and document navigation.
- Handles platform execution routines (browser launch, file manager reveal, macOS Dock icon via Cocoa).