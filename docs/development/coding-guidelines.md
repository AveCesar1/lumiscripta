# Coding Guidelines

This guide summarizes the programming conventions and architectural practices observed across the Lumiscripta codebase.

## Language Standard & Dialect

- **C++17**: Code must strictly conform to the C++17 ISO standard (`-std=c++17`).
- **No C++20 features**: Do not use concepts, coroutines, ranges, or formatting library additions.
- **No RTTI / Dynamic Casts**: Code avoids runtime type information (`dynamic_cast`, `typeid`).
- **No C++ Exceptions**: Functions signal failure by returning `bool` (or optional/pointer types where appropriate) and log diagnostics to `std::cerr`. Do not `throw` or use `try`/`catch`.

## Scope & Namespaces

- **Global Namespace**: The core classes (`LumiscriptaApp`, `File`, `Graphics`, `MarkdownRenderer`) reside directly in the global namespace.
- **No Namespaces for App Types**: Do not wrap project headers in a `namespace lumiscripta { ... }` block; keep symbols aligned with the existing global architecture.
- **Using Directives**: Header files use selective type aliases where needed (`using std::string;`, `using std::unique_ptr;`). Avoid broad `using namespace std;` in headers.

## Naming Conventions

- **Classes and Structs**: `PascalCase` (`LumiscriptaApp`, `CachedImage`, `MarkdownRenderer`).
- **Member Variables**: `m_` prefix with camelCase (`m_window`, `m_file`, `m_graphics`, `m_running`, `m_viewMode`).
- **Methods and Member Functions**: `camelCase` (`loadFile()`, `renderUI()`, `enterMainUI()`, `applyTheme()`).
- **Free and Utility Functions**: `camelCase` (`parentDirectory()`, `resolveAsset()`, `formatBytes()`).
- **Constants**:
  - Global or file-scope constants: `k` prefix with PascalCase (`kWelcomeWindowWidth`, `kLinkColor`, `kEmojiLoaderFlags`).
  - Preprocessor macros: `ALL_CAPS_WITH_UNDERSCORES` (`APP_H`, `ICON_MIN_FA`).
- **Enums**: `enum class` scoped enumerations using `PascalCase` for both enum name and values (`enum class ViewMode { Editor, Preview, Welcome };`).

## Memory & Ownership Rules

- **Exclusive Heap Ownership**: Use `std::unique_ptr` for owned heap allocations inside classes (e.g. `m_file`, `m_graphics` in `LumiscriptaApp`).
- **Non-Owning References**: Use raw pointers for observers or externally-managed resources:
  - `GLFWwindow* m_window` (lifecycle owned by GLFW / App).
  - `Graphics* m_graphics` inside `MarkdownRenderer` (back-reference to owning graphics manager).
  - `ImFont*` global font handles.
- **No Shared Pointers**: Do not use `std::shared_ptr` unless shared ownership semantics are strictly necessary (none currently exist).

## Headers, Includes & Forward Declarations

- **Header Guards**: Classic `#ifndef HEADER_NAME_H` `#define HEADER_NAME_H` guards.
- **Include Quoting**:
  - Quoted paths for project headers: `#include "lumiscripta/app.h"` or `#include "App.h"`.
  - Angle brackets for system and C++ standard library headers: `#include <vector>`, `#include <GLFW/glfw3.h>`.
- **Forward Declarations**: Forward-declare structs and classes in headers whenever possible (e.g., `struct GLFWwindow;`, `class File;`, `class Graphics;`) to reduce header dependencies and compilation overhead.

## Third-Party Library Isolation

- **`stb_image` Rule**: The implementation definition `#define STB_IMAGE_IMPLEMENTATION` must exist in **exactly one** translation unit (`src/graphics.cpp`). All other files must include `stb/stb_image.h` without this macro to avoid duplicate symbol linker errors.
- **md4c Integration**: `md4c.c` is pure ANSI C and is compiled with `$(CC)` rather than `$(CXX)`.
- **ImGui Internal APIs**: Where ImGui internal APIs are necessary (e.g., `findMultilineTextWindow()`, `predictMultilineWrapWidth()` for word-wrapped line gutters), clearly document the rationale, edge cases prevented, and downstream upgrade considerations.
- **FreeType Emoji Configuration**: Font-merging flags (`ImGuiFreeTypeLoaderFlags_LoadColor | ImGuiFreeTypeLoaderFlags_Bitmap`) and global defines (`-DIMGUI_ENABLE_FREETYPE -DIMGUI_USE_WCHAR32`) are mandatory. Never remove or alter them casually.

## Error Handling & Degradation

- **Graceful Asset Fallbacks**: Missing assets must never crash or assert-abort the application:
  - Missing fonts fall back to ImGui's embedded default font (`io.Fonts->AddFontDefault()`).
  - Missing window icons run the window unadorned.
  - Broken Markdown images render centered italic placeholder labels (`Image file not found` or `Image file is unreadable`) instead of halting rendering.
- **Font Validation**: Pre-verify magic bytes via `looksLikeFontFile()` before feeding files to FreeType/ImGui to prevent assertion panics on corrupted font data.

## Related Documents

- [Architecture Overview](../architecture/overview.md) — high-level unit responsibilities
- [Font System](../architecture/font-system.md) — font atlas rules and load-bearing constraints
- [Architectural Decisions](../architecture/decisions.md) — verified rationale for design patterns