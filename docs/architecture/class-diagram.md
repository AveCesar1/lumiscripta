# Class Diagram

Principal components of Lumiscripta, derived from the headers in [`include/lumiscripta/`](../../include/lumiscripta/). Only load-bearing members and public methods are shown; private helpers are omitted for readability.

```mermaid
classDiagram
    class LumiscriptaApp {
        -unique_ptr~File~ m_file
        -unique_ptr~Graphics~ m_graphics
        -GLFWwindow* m_window
        -ViewMode m_viewMode
        -bool m_running
        +init() bool
        +run() void
        +shutdown() void
        +loadFile(path: string) bool
        +saveFile(path: string) bool
        +toggleView() void
        +toggleTheme() void
        +getViewMode() ViewMode
        -processInput() void
        -renderUI() void
        -renderMenuBar() void
        -renderWelcome() void
        -enterMainUI(mode: ViewMode) void
    }

    class File {
        -string m_path
        -string m_content
        -string m_originalContent
        -bool m_modified
        +load(path: string) bool
        +save(path: string) bool
        +getContent() string
        +setContent(content: string) void
        +getPath() string
        +hasUnsavedChanges() bool
    }

    class Graphics {
        -ImGuiContext* m_ctx
        -GLFWwindow* m_window
        -Theme m_theme
        -bool m_initialized
        -unordered_map~CachedImage~ m_images
        -string m_baseDir
        -string m_wordmarkLightPath
        -string m_wordmarkDarkPath
        +init(window: GLFWwindow*) bool
        +shutdown() void
        +beginFrame() void
        +endFrame() void
        +applyTheme(theme: Theme) void
        +getTheme() Theme
        +renderEditor(content: string) void
        +renderPreview(content: string) void
        +setBaseDirectory(dir: string) void
        +clearImageCache() void
        +getImageTexture(src: string, texture: ImTextureID, size: ImVec2, error: ImageError) bool
        +getWordmarkTexture(theme: Theme, texture: ImTextureID, size: ImVec2) bool
        -loadTexture(path: string, texture: ImTextureID, size: ImVec2, error: ImageError) bool
        -setupStyleLight() void
        -setupStyleDark() void
    }

    class CachedImage {
        +unsigned int id
        +float width
        +float height
    }

    class MarkdownRenderer {
        -bool m_code_block
        -ImDrawList* m_code_draw_list
        -ImVec2 m_code_start
        -float m_code_width
        -float m_table_width
        -ImVec2 m_table_start
        -Graphics* m_graphics
        +setGraphics(graphics: Graphics*) void
        +get_font() ImFont*
        +BLOCK_CODE(detail: MD_BLOCK_CODE_DETAIL, enter: bool) void
        +SPAN_CODE(enter: bool) void
        +BLOCK_TABLE(detail: MD_BLOCK_TABLE_DETAIL, enter: bool) void
        +open_url() void
        +get_image(nfo: image_info) bool
    }

    class imgui_md {
        <<interface>>
        +print(md: char, md_end: char) void
    }

    class GLFWwindow {
        <<external>>
    }

    class ViewMode {
        <<enumeration>>
        Editor
        Preview
        Welcome
    }

    class Theme {
        <<enumeration>>
        Light
        Dark
    }

    class ImageError {
        <<enumeration>>
        None
        NotFound
        Unreadable
    }

    LumiscriptaApp "1" *-- "1" File : owns (unique_ptr)
    LumiscriptaApp "1" *-- "1" Graphics : owns (unique_ptr)
    LumiscriptaApp --> GLFWwindow : creates and destroys (non-owning ref)
    LumiscriptaApp ..> ViewMode : state machine
    Graphics "1" *-- "1" CachedImage : m_images (per resolved path)
    Graphics ..> MarkdownRenderer : instantiated in renderPreview
    Graphics ..> Theme : applyTheme / getTheme
    Graphics ..> ImageError : reports decode failures
    MarkdownRenderer --|> imgui_md : overrides rendering callbacks
    MarkdownRenderer --> Graphics : back-reference (non-owning)
```

## Notes on the Diagram

- **`LumiscriptaApp` is the composition root.** It is the only class that owns heap objects (`unique_ptr`) and the only one that touches GLFW window creation/destruction. `File` and `Graphics` never reference each other; App mediates (e.g. `loadFile()` tells `Graphics` to clear its image cache and re-base relative image paths).
- **`MarkdownRenderer` is declared in `graphics.h` but instantiated as a function-local `static` inside `Graphics::renderPreview()`.** `imgui_md` never tells the renderer who owns it, so `setGraphics(this)` is the hook that lets `get_image()` reach the texture cache. The `Graphics*` it holds is a non-owning back-reference.
- **`CachedImage` is a private nested struct of `Graphics`** (shown standalone only for readability); in the header it is `std::unordered_map<string, CachedImage>` keyed by resolved path. It stores the GL texture name as `unsigned int` so the public header does not need OpenGL headers.
- **Font handles are globals, not members:** `g_font_regular`, `g_font_bold`, `g_font_bold_large`, `g_font_mono`, `g_font_mono_large` are `extern ImFont*` declared in `graphics.h`, defined in `graphics.cpp`; a file-local `g_font_italic` serves image error messages. They are shared by the editor, preview, and welcome screen.
- **`File` is intentionally dumb** — a buffer, a path, and a last-saved snapshot. Path logic (image base directories, asset lookup) lives in `App`/`Graphics`/`Utils`.
- Error signaling everywhere is `bool` (no exceptions); enums encode *why* an operation failed where callers need to distinguish causes (`ImageError`).
- Signatures are abridged: `renderEditor` takes `string&` (in-place editing) and `renderPreview` takes `const string&`; pointer/out parameters are shown without `*` where noted by type name (`ImTextureID`, `ImageError`).

## Related Documents

- [Overview](overview.md) · [Application flow](application-flow.md) · [Rendering pipeline](rendering-pipeline.md) · [Decisions](decisions.md)
- [Documentation index](../README.md)