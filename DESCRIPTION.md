# Lumiscripta

> *Light upon the written word.*

Lumiscripta is a minimal, single-purpose markdown viewer. It does not edit. It does not manage projects. It opens a document, renders it beautifully, and gets out of the way.

---

## Design Philosophy

The interface is the content. Every pixel that is not text is a mistake. There are no sidebars, no ribbons, no floating panels. A single bar at the top holds only what is strictly necessary: the document name, a view toggle, and a theme switch. Everything else is whitespace.

The app alternates between two states — **Editor** and **Preview** — never both at once. This forces focus. You read, or you edit. Never both.

---

## Color Palette

### Light Mode — Warm Pebble

| Token | Hex | Usage |
|---|---|---|
| Background | `#FAF7F2` | Main canvas |
| Surface | `#E3DDD5` | Code blocks, blockquotes, tables |
| Text Primary | `#2D2A28` | Body text, headings |
| Text Secondary | `#7A6F66` | Metadata, timestamps, hints |
| Accent / Border | `#B8AEA4` | Dividers, scrollbars, subtle outlines |

### Dark Mode — Slate Almond

| Token | Hex | Usage |
|---|---|---|
| Background | `#1B1D20` | Main canvas |
| Surface | `#2A2D33` | Code blocks, blockquotes, tables |
| Text Primary | `#E9EDF1` | Body text, headings |
| Text Secondary | `#8A9199` | Metadata, timestamps, hints |
| Accent / Border | `#4A4F56` | Dividers, scrollbars, subtle outlines |

No pure black. No pure white. The palette carries temperature in both directions.

---

## Typography

| Role | Family | Weight | Size |
|---|---|---|---|
| Body & UI | Inter | 400 / 500 | 16px |
| Headings | Inter | 500 / 700 | 20px / 24px / 32px |
| Code (inline & blocks) | JetBrains Mono | 400 / 700 | 14px |
| Icons | FontAwesome 7 Free Solid | — | 16px |

Line height for body text is `1.6`. Code blocks use `1.5`. Headings are tight at `1.2`.

---

## Interface Structure

```
┌─────────────────────────────────────────────┐
│  Lumiscripta    [Editor] [Preview]    [☀︎]  │  <- top bar (32px)
├─────────────────────────────────────────────┤
│                                             │
│                                             │
│              Content Area                     │
│         (editor OR preview)                   │
│                                             │
│                                             │
└─────────────────────────────────────────────┘
```

### Top Bar
- **Left:** App name or current filename (truncated if long).
- **Center:** Segmented control toggling `Editor` / `Preview`. The active segment is filled; the inactive one is ghosted.
- **Right:** Theme toggle button (sun / moon icon). No text label.

### Editor View
- A single monospaced text area spanning the full window.
- No line numbers by default (they add chrome).
- Syntax highlighting for markdown via `ImGuiColorTextEdit`.
- Scrollbar is 4px wide, no track, thumb at 15% opacity of text color.

### Preview View
- Rendered markdown using `imgui_md`.
- Headings have generous top margin, tight bottom margin.
- Code blocks sit inside a rounded rectangle (`radius: 6px`) with the Surface color.
- Blockquotes use a 2px left border in Accent color.
- Links are underlined and colored in Text Secondary.

### Scrollbars
- Custom styled via ImGui: 4px width, no background track, thumb is Text Primary at 12% opacity.
- Hovering the thumb raises opacity to 25%.

---

## Interaction Model

| Action | Shortcut |
|---|---|
| Open file | `Ctrl/Cmd + O` |
| Toggle Editor / Preview | `Ctrl/Cmd + E` |
| Toggle Light / Dark | `Ctrl/Cmd + Shift + T` |
| Zoom in | `Ctrl/Cmd + +` |
| Zoom out | `Ctrl/Cmd + -` |

Mouse is secondary. Keyboard is primary. The app is built for people who do not want to reach for the mouse.

---

## Technical Notes

Lumiscripta is built on **Dear ImGui**, an immediate-mode GUI library. This means the UI is redrawn every frame. There are no retained widget trees, no native OS controls, and no platform-specific look. The visual style is defined entirely in code — every color, every radius, every pixel is deliberate.

The backend is **GLFW** + **OpenGL 3**. The app compiles on Linux, macOS, and Windows from a single Makefile. No CMake. No generated build files.

---

## Assets

All visual assets live under `assets/`:

```
assets/
├── fonts/
│   ├── Inter-Regular.ttf
│   ├── Inter-Medium.ttf
│   ├── Inter-Bold.ttf
│   ├── JetBrainsMono-Regular.ttf
│   ├── JetBrainsMono-Bold.ttf
│   └── fa-solid-900.ttf
└── icon.png          <- 256x256, transparent background
```

The window icon is loaded at startup via `glfwSetWindowIcon`. It appears in the OS taskbar, window title bar, and alt-tab switcher.