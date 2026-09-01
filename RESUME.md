# Lumiscripta — Internal Development Guide

This document is for the AI (or any future developer) implementing Lumiscripta. It describes architecture, library APIs, coding conventions, and implementation rules.

---

## Project Structure

```
lumiscripta/
├── include/lumiscripta/
│   ├── App.h
│   ├── File.h
│   ├── Graphics.h
│   └── Helpers.h
├── src/
│   ├── main.cpp
│   ├── App.cpp
│   ├── File.cpp
│   └── Graphics.cpp
├── third_party/
│   ├── imgui/              <- Dear ImGui core + backends
│   ├── md4c/               <- md4c.h, md4c.c
│   ├── imgui_md/           <- imgui_md.h, imgui_md.cpp
│   ├── stb/                <- stb_image.h (for window icon)
│   └── IconsFontAwesome/   <- IconsFontAwesome7.h, IconsFontAwesome7Brands.h
├── assets/
│   ├── fonts/
│   └── icon.png
└── Makefile
```

**No CMake.** Build is handled by a single Makefile with platform detection (Linux, macOS, Windows/MinGW).

---

## Architecture

Four units. No namespaces. Global scope for classes.

### App
- Owns `File*` and `Graphics*`.
- Creates the GLFW window (`1280x800`, resizable, no native decorations if possible).
- Runs the main loop: `processInput()` -> `renderUI()`.
- Holds `ViewMode` (Editor / Preview) and delegates rendering to `Graphics`.
- Handles file open dialog via native OS calls (platform-specific, or via `tinyfiledialogs` if needed later).

### File
- Simple I/O wrapper.
- `load(path)` reads entire file into `std::string`.
- `setContent(str)` replaces buffer (used for clipboard paste).
- `hasUnsavedChanges()` compares current buffer against original.
- All filesystem operations are blocking and synchronous (files are small).

### Graphics
- Initializes Dear ImGui + GLFW + OpenGL3 backends.
- Loads fonts from `assets/fonts/` at init time.
- Applies color themes (Light / Dark) by mutating `ImGuiStyle` directly.
- `renderEditor(std::string& content)` uses `ImGui::InputTextMultiline` or `ImGuiColorTextEdit` for syntax highlighting.
- `renderPreview(const std::string& content)` passes the string to `imgui_md::render()`.
- `beginFrame()` / `endFrame()` wrap `ImGui::NewFrame()` / `ImGui::Render()` / `ImGui_ImplOpenGL3_RenderDrawData()`.

### Helpers
- Header-only utility functions: `trim()`, `split()`, `fileExtension()`, `formatBytes()`.
- Pure functions. No state. No class.

---

## Library APIs & Usage Patterns

### Dear ImGui
- Include order matters:
  ```cpp
  #include "imgui.h"
  #include "imgui_impl_glfw.h"
  #include "imgui_impl_opengl3.h"
  ```
- Font loading happens **before** `ImGui::NewFrame()` in init:
  ```cpp
  ImGuiIO& io = ImGui::GetIO();
  io.Fonts->AddFontFromFileTTF("assets/fonts/Inter-Regular.ttf", 16.0f);
  ```
- For merged icon fonts (FontAwesome 7):
  ```cpp
  ImFontConfig cfg;
  cfg.MergeMode = true;
  cfg.GlyphMinAdvanceX = 16.0f;
  static const ImWchar ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
  io.Fonts->AddFontFromFileTTF("assets/fonts/fa-solid-900.ttf", 16.0f, &cfg, ranges);
  ```
  Use `IconsFontAwesome7.h` macros like `ICON_FA_FILE` or `ICON_FA_MOON`.
- Style is applied by mutating `ImGui::GetStyle()`:
  ```cpp
  ImGuiStyle& style = ImGui::GetStyle();
  style.WindowRounding = 0.0f;
  style.FrameRounding = 6.0f;
  style.ScrollbarSize = 4.0f;
  style.Colors[ImGuiCol_WindowBg] = ImVec4(0.98f, 0.97f, 0.95f, 1.0f); // #FAF7F2
  ```

### imgui_md + md4c
- `imgui_md` is a single `.cpp`/`.h` pair that wraps `md4c`.
- Usage:
  ```cpp
  #include "imgui_md.h"
  imgui_md::render("markdown string here");
  ```
- It renders directly into the current ImGui window.
- It respects the current font and style colors.
- For code blocks, it uses the current monospaced font if configured.
- `md4c` is compiled as C (`md4c.c`), not C++. Use `$(CC)` in the Makefile, not `$(CXX)`.

### GLFW
- Window creation:
  ```cpp
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  #ifdef __APPLE__
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
  #endif
  GLFWwindow* window = glfwCreateWindow(1280, 800, "Lumiscripta", NULL, NULL);
  ```
- Window icon:
  ```cpp
  #define STB_IMAGE_IMPLEMENTATION
  #include "stb_image.h"
  // ...
  int w, h, ch;
  unsigned char* px = stbi_load("assets/icon.png", &w, &h, &ch, 4);
  if (px) {
      GLFWimage img = { w, h, px };
      glfwSetWindowIcon(window, 1, &img);
      stbi_image_free(px);
  }
  ```
- VSync: `glfwSwapInterval(1);`

### FontAwesome 7
- The user downloaded FA7 and `IconsFontAwesome7.h`.
- Ensure the TTF file matches the header version. If the header defines `ICON_MIN_FA` / `ICON_MAX_FA`, use those ranges. If not, use the default range `0xE000-0xF8FF` or whatever the header specifies.
- Icons are rendered as UTF-8 text inside ImGui widgets:
  ```cpp
  ImGui::Button(ICON_FA_MOON " Dark");
  ```

---

## Coding Conventions

- **No namespaces.** All classes and free functions live in the global namespace.
- **Member prefix:** `m_` for all class members (`m_window`, `m_content`).
- **Pointer ownership:** Use raw pointers only for non-owning references (e.g., `GLFWwindow*`). Use `std::unique_ptr` for owned heap objects inside `App`.
- **Includes:** Use quoted paths for project headers (`#include "App.h"`). Use angle brackets only for system headers (`#include <vector>`).
- **No exceptions.** Return `bool` for error signaling. Log to `std::cerr`.
- **No RTTI / no dynamic_cast.**
- **C++17 only.** No C++20 features.

---

## Implementation Rules

1. **App::init()** must create the GLFW window first, then call `Graphics::init(window)`, then load fonts.
2. **App::run()** is the while-loop: `glfwPollEvents()` -> `Graphics::beginFrame()` -> `renderUI()` -> `Graphics::endFrame()` -> `glfwSwapBuffers()`.
3. **renderUI()** draws the top bar first (using `ImGui::BeginMainMenuBar()` or a manual `ImGui::SetCursorPos` bar), then the content area below it.
4. **Editor view** uses the full remaining window height. Preview view does the same.
5. **Theme switching** must happen instantly without restart. Mutate `ImGuiStyle` colors and call `ImGui::GetIO().Fonts->Build()` only if fonts change (they don't).
6. **File loading** is synchronous. Show a subtle status message in the top bar if loading fails (no modal dialogs).
7. **Window icon** is set once at startup. If `assets/icon.png` is missing, the app starts without an icon — no crash.
8. **Asset paths** are relative to the working directory. Do not hardcode absolute paths.

---

## Build Commands

```bash
make -j$(nproc)     # Linux
make -j$(sysctl -n hw.ncpu)   # macOS
make                # Windows (MinGW)
make run            # build and execute
make clean          # remove build/ and binary
```

---

## Known Gaps (to be filled during implementation)

- Native file open dialog on each platform (currently can use `glfw` + platform APIs, or a simple `std::cin` fallback for MVP).
- Clipboard integration for paste (`glfwGetClipboardString`).
- Syntax highlighting in editor view (decide between raw `InputTextMultiline` or integrating `ImGuiColorTextEdit`).
- Zoom: implemented by rebuilding the font atlas with different sizes, or scaling `ImGuiIO::FontGlobalScale`.