# Contributing to Lumiscripta

Lumiscripta is a lightweight, focused Markdown viewer and editor. Contributions should respect the project's minimalist philosophy, keeping complexity and dependencies low.

## Guiding Principles

1. **Content is the Interface**: Resist adding ribbons, sidebars, floating inspectors, or unnecessary chrome. The visual space belongs to the document.
2. **Single-Pane Focus**: Do not turn the application into a dual-pane side-by-side editor. The alternation between reading (Preview) and writing (Code) is an intentional design constraint.
3. **Zero Superfluous Dependencies**: External dependencies should be avoided whenever possible. If an OS feature can be triggered via existing system tools or standard platform headers (e.g. `open`, `xdg-open`, `explorer`), prefer that over linking new libraries.
4. **Clarity and Maintainability**: Prefer direct, transparent code over clever C++ abstractions. Comments explaining "why" a particular workaround or structure was chosen are actively encouraged.

## Development Workflow

### 1. Build and Run Locally

Ensure system requirements (GLFW, FreeType) are installed (see [Building Lumiscripta](building.md)).

```bash
# Clean build
make clean
make -j$(nproc)       # Linux
make -j$(sysctl -n hw.ncpu)  # macOS

# Run tests or preview documents
make run
# or run with a target Markdown file:
./lumiscripta README.md
```

### 2. Verify Across Views and Themes

When modifying UI, rendering, or input logic, always manually verify behavior across:
- **Welcome view**: launch without arguments (`./lumiscripta`) to verify button layout, centering, and initial window size.
- **Preview view**: headings, code blocks, tables, lists, inline code, colour emoji, and images (`![alt](path)`).
- **Editor view**: monospaced text, caret visibility, and line-number gutter synchronization during word wrapping and vertical scrolling.
- **Theme toggling**: test both Light (Warm Pebble) and Dark (Slate Almond) modes using `Ctrl/Cmd + Shift + T`.
- **Zoom scaling**: verify that `Ctrl/Cmd + '+'` and `'-'` scale typography and UI proportionally.
- **Link navigation**: verify browser confirmations for web URLs and document-switching prompts for relative `.md` files.

### 3. Preserving Load-Bearing Systems

Certain subsystems in Lumiscripta have subtle, load-bearing requirements that fail silently if altered casually:
- **Font Atlas Merging**: Emoji fonts must be merged into *all* text fonts with `DstFont` explicitly specified and `kIconExcludeRanges` configured (see [Font System](../architecture/font-system.md)).
- **Image Texture Lifetime**: Textures must be cleared and deleted while the OpenGL context is active (handled in `clearImageCache()` and `shutdown()`).
- **Editor Scroll Synchronisation**: Multiline input word-wrap state and line-number rendering rely on ImGui internal child window lookups (documented in `src/graphics.cpp`).

### 4. Updating Documentation

If your changes modify user-facing behavior, architectural structure, or make targets:
- Update the relevant documents under `docs/architecture/` or `docs/development/`.
- Ensure decisions are documented in `docs/architecture/decisions.md` with explicit rationale tags.
- Cross-link related documentation files to maintain navigational integrity.

## Submitting Pull Requests

- Keep pull requests focused on a single feature, bug fix, or documentation enhancement.
- Follow existing formatting and naming conventions (see [Coding Guidelines](coding-guidelines.md)).
- Verify that `make clean && make` succeeds without introducing new compiler warnings.
- Clean up any temporary diagnostic code before committing.

## Related Documents

- [Building Lumiscripta](building.md) — build system details and prerequisites
- [Coding Guidelines](coding-guidelines.md) — code style and language practices
- [Architecture Overview](../architecture/overview.md) — high-level design breakdown