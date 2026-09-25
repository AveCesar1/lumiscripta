# Architectural Decisions

Design decisions visible in the current implementation, with their rationale where it can be verified. Confidence is marked on each rationale:

- **[code]** — stated in a source comment or directly observable behavior
- **[docs]** — stated in the project's own documentation (`README.md`, `DESCRIPTION.md`, `RESUME.md`)
- **(inferred)** — reasonable but not explicitly documented; treat as uncertain

## 1. Lightweight four-unit architecture

**Decision:** The whole app is `App`, `Graphics`, `File`, and header-only `Utils` in the global namespace, with `bool`-returns + `std::cerr` instead of exceptions, no RTTI, and C++17 only.

**Rationale:** The `main.cpp` header states the project is a personal learning exercise that "intentionally prioritizes clarity over cleverness"; `RESUME.md` codifies the conventions. The structure keeps every subsystem readable in one file. **[docs/code]**

## 2. Dear ImGui + GLFW + OpenGL 3.3 stack

**Decision:** The UI is immediate-mode Dear ImGui on GLFW with the OpenGL3 backend (GLSL `#version 330`), compiled in by the Makefile.

**Rationale:** `DESCRIPTION.md` states the visual style is defined entirely in code — every color and radius — with no native OS controls and no platform-specific look, so the two themes are literal palettes in `setupStyleLight/Dark()`. Immediate mode also means "redrawn every frame" with no retained widget tree. **[docs/code]**

## 3. Hand-rolled Makefile, no CMake

**Decision:** A single `Makefile` with `uname -s` platform detection builds sources, ImGui, md4c (as C), and imgui_md; system deps are discovered via `pkg-config` with MacPorts/Homebrew fallbacks; targets include `run`, `install`, and `macos-bundle`.

**Rationale:** The README states it explicitly — *"No CMake. Just `make`."* Deeper motivations (e.g. avoiding generated build files) are implied by the docs but not argued. **[docs] (deeper rationale inferred)**

## 4. Single-pane UI as a state machine

**Decision:** `ViewMode { Welcome, Editor, Preview }` is exclusive; the top bar is a segmented Code/Preview control, never two panes at once.

**Rationale:** `DESCRIPTION.md`: the alternation "forces focus. You read, or you edit. Never both." — the exclusivity *is* the product idea, not an implementation shortcut. **[docs]**

## 5. Stock `InputTextMultiline` editor (no third-party editor widget)

**Decision:** The editor is ImGui's own multiline input with a hand-drawn 36 px line-number gutter, word-wrap-aware row stepping, and two ImGui-internal workarounds for scroll/wrap state. No syntax highlighting is compiled in — the Makefile builds neither `ImGuiColorTextEdit` nor Hello ImGui, despite both appearing in the root `README.md` dependency table.

**Rationale:** The choice itself is observable **[code]**; `RESUME.md` lists syntax highlighting as an open decision ("raw `InputTextMultiline` or integrating `ImGuiColorTextEdit`").

## 6. Local-only images

**Decision:** Any Markdown image reference containing `://` is rejected as not-found; there is no HTTP client.

**Rationale:** Stated in `graphics.cpp`: the app has no HTTP client, "and a viewer should never block on the network anyway" — the UI thread must not stall. **[code]**

## 7. Image caching is mandatory

**Decision:** Textures are cached in `unordered_map<string, CachedImage>` keyed by *resolved path*, cleared on every file load and at shutdown.

**Rationale:** `get_image()` executes every frame for every image, so decoding without a cache would run stb_image in the render loop; `RESUME.md`: "Caching is mandatory, not an optimization." Clearing per document prevents GPU texture leaks from the previous file. **[code/docs]**

## 8. Always decode to RGBA; one stb_image implementation unit

**Decision:** `stbi_load(..., req_comp = 4)` everywhere; `STB_IMAGE_IMPLEMENTATION` is defined exactly once, in `graphics.cpp`.

**Rationale:** ImGui uploads four components — grayscale PNGs or palette GIFs would otherwise arrive with the wrong stride; the single-implementation rule is the stb linkage contract. **[code]**

## 9. Broken images become visible text, not gaps or alt text

**Decision:** `MarkdownRenderer::get_image()` draws one of two centered italic messages (`not found` vs `unreadable (corrupted?)`) and returns `false` on failure.

**Rationale:** `imgui_md` suppresses alt text while rendering an image, so `get_image()` is the only hook where a failure can surface — and distinguishing "missing" from "undecodable" requires checking existence *before* decoding. **[code]**

## 10. Font strategy: per-font merged sources, FreeType, COLRv0, pseudo-italic

**Decision:** One font atlas built once at startup: Inter/JetBrains Mono bases with Font Awesome confined to the UI font's PUA block and Twemoji Mozilla colour emoji merged into *every* text font, rasterized by ImGui's FreeType loader under `-DIMGUI_ENABLE_FREETYPE -DIMGUI_USE_WCHAR32`; the "italic" face is a FreeType-obliqued Inter rather than a shipped Italic cut.

**Rationale:** ImGui has no cross-font fallback and stb_truetype is greyscale-only; FreeType (uniquely among the supported loaders) rasterizes COLRv0, which is why Noto Color Emoji (COLRv1/SVG/CBDT) cannot be used. All constraints and failure modes are documented in detail in [font-system.md](font-system.md). **[code/docs]**

## 11. Synchronous, blocking file I/O

**Decision:** `File::load/save` read and write the whole document synchronously on the calling thread.

**Rationale:** Markdown documents are small, so no threading is justified (`RESUME.md` states files are small; the code is plainly synchronous). **[docs/code]**

## 12. Native open dialog via `popen()` + OS tools

**Decision:** File picking shells out to `osascript` (macOS), PowerShell `OpenFileDialog` (Windows), or `zenity`/`kdialog` (Linux) and reads the path from stdout.

**Rationale:** Not documented; observable benefit is that no dialog library is added to the dependency set. **(inferred)**

## 13. Small welcome window that grows into the editor

**Decision:** The app starts as a 440×300 window in `Welcome` mode; `enterMainUI()` resizes it to 1280×800 on first document.

**Rationale:** Not documented; an unobtrusive first-run window is the obvious reading of the behavior. **(inferred)**

## 14. No ImGui `.ini` persistence

**Decision:** `io.IniFilename = nullptr`.

**Rationale:** Code comment: "we don't want ImGui saving window positions." **[code]**

## 15. Zoom via `FontGlobalScale`

**Decision:** `Ctrl/Cmd + +/-` steps `io.FontGlobalScale` by 0.1 (clamped 0.5–2.5).

**Rationale:** One knob scales the entire UI — top bar, editor, gutter, and preview — without rebuilding the font atlas (fonts don't change on theme switch either). **[code/docs]**

## 16. Executable-relative asset resolution

**Decision:** All assets load through `resolveAsset()`: `$LUMISCRIPTA_ASSETS` → paths relative to the executable → system directories → working directory (last resort).

**Rationale:** Stated in `utils.h`: a desktop launcher or file manager decides the working directory (usually `/` or `$HOME`), so cwd-relative paths would fail outside the project root. **[code]**

## 17. The gutter uses ImGui internals on purpose

**Decision:** The editor reads ImGui's private state (`FindWindowByID`-style lookup by `ChildId`, `InputTextState.WrapWidth` seeding, scroll restoration) to keep line numbers and viewport stable.

**Rationale:** Comments in `graphics.cpp` document the exact bugs each workaround prevents (gutter reading stale scroll before first activation; the one-frame re-centering "flash" on activation). There is no public ImGui API for the input's inner child window. Reliance on internals means **these must be re-verified on ImGui upgrades**. **[code]**

## 18. Themes mutate `ImGuiStyle` in place

**Decision:** `applyTheme()` re-applies common styling plus a full palette directly to `ImGui::GetStyle()`; no stylesheet, no rebuild, no restart.

**Rationale:** Instant switching (a `RESUME.md` implementation rule) and, per `DESCRIPTION.md`, the palettes are hand-tuned hex values ("no pure black, no pure white"). **[docs/code]**

## Notable Omissions (verified against the code)

- **No save-as dialog** — `Ctrl/Cmd + S` only writes the existing path.
- **Links do not open** — `MarkdownRenderer::open_url()` is an empty no-op.
- **No syntax highlighting and no bundled editor widget** in the build (see decision 5).
- **No network access of any kind** (see decision 6).
- **Font Awesome is loaded but unused** — no `ICON_FA_*` macro appears in `src/` or `include/`; the merge machinery exists for future use.

## Related Documents

- [Overview](overview.md) · [Font system](font-system.md) · [Rendering pipeline](rendering-pipeline.md) · [Image pipeline](image-pipeline.md)
- [Documentation index](../README.md)