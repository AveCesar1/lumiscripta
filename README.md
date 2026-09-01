# Lumiscripta

> *A minimal, fast, and beautiful markdown viewer — from light to script.*

Lumiscripta (*lumen* + *scripta* — "illuminated writings") is a lightweight Markdown viewer designed for clarity. No toolbars, no sidebars, no noise. Just your document, rendered beautifully. Toggle seamlessly between raw Markdown source and a styled preview. Built with C++ and Dear ImGui.

---

## ✨ Features

- **Single-pane toggle** — switch between editor and preview, never both at once. Clean and focused.
- **Warm light & deep dark themes** — carefully tuned palettes, not generic white/black.
- **Zero chrome** — the content is the interface. No ribbons, no sidebars, no clutter.
- **Cross-platform** — Linux, macOS, and Windows.
- **Fast & lean** — immediate-mode rendering, minimal memory footprint.

---

## 📦 Dependencies

All third-party code is included in this repository under `third_party/`:


| Library | Purpose | License |
|---|---|---|
| [Dear ImGui](https://github.com/ocornut/imgui) | Immediate-mode GUI | MIT |
| [Hello ImGui](https://github.com/pthom/hello_imgui) | Cross-platform bootstrap | MIT |
| [imgui_md](https://github.com/mekhontsev/imgui_md) | Markdown rendering | MIT |
| [md4c](https://github.com/mity/md4c) | Markdown parser | MIT |
| [ImGuiColorTextEdit](https://github.com/BalazsJako/ImGuiColorTextEdit) | Syntax-highlighted editor | MIT |

The only **system dependency** you need to install is **GLFW** (window and input handling).

### Linux
```bash
# Ubuntu / Debian
sudo apt install libglfw3-dev libglew-dev

# Fedora / RHEL
sudo dnf install glfw-devel glew-devel

# Arch
sudo pacman -S glfw glew
```

### macOS
```bash
# MacPorts
sudo port install glfw

# Homebrew
brew install glfw glew
```

### Windows
Download GLFW binaries from [glfw.org](https://www.glfw.org/download.html) or install via vcpkg:
```bash
vcpkg install glfw3 glew
```

---

## 🚀 Quick Start

No CMake. Just `make`.

```bash
git clone https://github.com/yourusername/lumiscripta.git
cd lumiscripta
make -j$(nproc)
```

The Makefile auto-detects your platform (Linux, macOS, Windows/MinGW) and links the correct libraries.

```bash
# Run
make run
# or
./lumiscripta
```

---

## 🎹 Controls

| Action | Shortcut |
|---|---|
| Open file | `Ctrl/Cmd + O` |
| Toggle editor / preview | `Ctrl/Cmd + E` |
| Toggle light / dark theme | `Ctrl/Cmd + Shift + T` |
| Increase font size | `Ctrl/Cmd + +` |
| Decrease font size | `Ctrl/Cmd + -` |
| Paste from clipboard | `Ctrl/Cmd + V` |

---

## 🏗️ Architecture

```
lumiscripta/
├── include/lumiscripta/    # Public headers
│   ├── App.h
│   ├── File.h
│   ├── Graphics.h
│   └── Helpers.h
├── src/                     # Implementation
│   ├── main.cpp
│   ├── App.cpp
│   ├── File.cpp
│   └── Graphics.cpp
├── third_party/             # External dependencies
│   ├── hello_imgui/         # (git submodule)
│   ├── imgui_md/
│   ├── md4c/
│   └── ImGuiColorTextEdit/
├── assets/                  # Fonts, icons
└── CMakeLists.txt
```

---

## 📝 License

Lumiscripta is released under the [MIT License](LICENSE).

---

## 🙏 Acknowledgements

Built with gratitude for the incredible Dear ImGui ecosystem and the open-source community that makes projects like this possible.