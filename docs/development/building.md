# Building Lumiscripta

Lumiscripta uses a single platform-detecting **Makefile** — there is no CMake, Meson, or generated build system. Build execution relies on standard GNU `make` and C++17 compilers.

## Supported Platforms & Compilers

- **macOS** (`Darwin`): Clang (`clang++`) / Apple Clang. macOS 10.13+ deployment target.
- **Linux** (`Linux`): GCC (`g++`) or Clang (`clang++`).
- **Windows** (`MINGW` / MSYS2): MinGW-w64 GCC.

The build mandates **C++17** (`-std=c++17`). C++20 features are not used.

## Prerequisites and System Dependencies

All third-party libraries (Dear ImGui, md4c, imgui_md, stb_image, Twemoji, FontAwesome) are bundled in `third_party/` or `assets/fonts/`. Only windowing, OpenGL, and font rasterization dependencies come from the system.

### Required Packages

| Platform | Package Manager / Distribution | Command |
|---|---|---|
| **macOS** | Homebrew | `brew install glfw freetype` |
| **macOS** | MacPorts | `sudo port install glfw freetype` |
| **Linux** (Debian/Ubuntu) | APT | `sudo apt install libglfw3-dev libglew-dev libfreetype6-dev` |
| **Linux** (Fedora/RHEL) | DNF | `sudo dnf install glfw-devel glew-devel freetype-devel` |
| **Linux** (Arch) | Pacman | `sudo pacman -S glfw glew freetype2` |
| **Windows** (MSYS2/MinGW) | Pacman | `pacman -S mingw-w64-x86_64-glfw mingw-w64-x86_64-glew mingw-w64-x86_64-freetype` |

### FreeType Discovery in the Makefile

The Makefile discovers FreeType compiler and linker flags using `pkg-config`:
```make
FREETYPE_CFLAGS := $(shell pkg-config --cflags freetype2 2>/dev/null)
FREETYPE_LIBS   := $(shell pkg-config --libs freetype2 2>/dev/null)
```
If `pkg-config` returns nothing, fallback search paths are tried:
- `-I/opt/local/include/freetype2 -I/opt/homebrew/include/freetype2 -I/usr/local/include/freetype2`
- `-L/opt/local/lib -L/opt/homebrew/lib -L/usr/local/lib -lfreetype`

## Compiler Flags & ODR Requirements

The Makefile passes global flags in `CXXFLAGS`:
```make
CXXFLAGS := -std=c++17 -Wall -Wextra -pedantic -O2 \
            -DIMGUI_ENABLE_FREETYPE -DIMGUI_USE_WCHAR32
```

- `-DIMGUI_ENABLE_FREETYPE`: Routes ImGui font rasterization through FreeType so COLRv0 colour emoji can render.
- `-DIMGUI_USE_WCHAR32`: Expands `ImWchar` to 32 bits so supplementary unicode planes (U+1F000+) are addressable.
- **Critical:** These defines must be identical across every translation unit including `imgui.h` to prevent One Definition Rule (ODR) violations.

Header dependency generation is handled by `-MMD -MP` into `.d` files included at the end of the Makefile (`-include $(DEPS)`).

## Make Targets

| Target | Description | Output |
|---|---|---|
| `make` / `make all` | Compiles all C/C++ objects and links the main executable | `./lumiscripta` |
| `make run` | Builds the executable if needed and runs it immediately | Runs `./lumiscripta` |
| `make clean` | Removes the build directory, binary, macOS bundle, and runtime ini file | Removes `build/`, `lumiscripta`, `Lumiscripta.app`, `lumiscripta.ini` |
| `make install` | Installs the binary, shared assets, and desktop entries to `$(PREFIX)` (default: `/usr/local`) | `$(PREFIX)/bin/lumiscripta`, `$(PREFIX)/share/lumiscripta/assets` |
| `make macos-bundle` | Packages the application into a standalone macOS `.app` bundle with `.icns` icon | `./Lumiscripta.app` |

### Parallel Compilation

Run compilation across multiple CPU cores:
```bash
# macOS
make -j$(sysctl -n hw.ncpu)

# Linux
make -j$(nproc)
```

## Platform-Specific Build Behavior

### macOS
- Links `-lglfw -framework OpenGL -framework Cocoa -framework IOKit -framework CoreFoundation $(FREETYPE_LIBS)`.
- Runtime Dock icon assignment uses Objective-C runtime messaging with `Cocoa` without requiring separate `.mm` files.
- `make macos-bundle` runs `sips` and `iconutil` to generate `Lumiscripta.app/Contents/Resources/lumiscripta.icns` from `assets/branding/logo-light.png`.

### Linux
- Links `-lglfw -lGLEW -lGL -ldl -lpthread $(FREETYPE_LIBS)`.
- `make install` deploys `assets/branding/logo-light.png` to `$(DESTDIR)$(PREFIX)/share/icons/hicolor/256x256/apps/lumiscripta.png` and installs `assets/lumiscripta.desktop` to `$(DESTDIR)$(PREFIX)/share/applications/`.

### Windows (MinGW)
- Links `-lglfw3 -lglew32 -lopengl32 -lgdi32 -limm32 $(FREETYPE_LIBS)`.

## Asset Requirements at Runtime

The executable relies on assets inside `assets/` (fonts, brand wordmarks, icon). Assets are resolved dynamically through `resolveAsset()` in `include/lumiscripta/utils.h` checking:
1. Environment variable `LUMISCRIPTA_ASSETS`
2. `exe_dir/assets`
3. `exe_dir/../share/lumiscripta/assets`
4. `exe_dir/../assets` and `exe_dir/../../assets`
5. `/usr/share/lumiscripta/assets` and `/usr/local/share/lumiscripta/assets`
6. Working directory `./assets`

For development, executing from the repository root guarantees `./assets` is found.

## Related Documents

- [Project Structure](project-structure.md) — directory and module layout
- [Architecture Overview](../architecture/overview.md) — system units and dependencies
- [Decisions: Build System](../architecture/decisions.md) — rationale for the single Makefile