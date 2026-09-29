#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <string>
#include <algorithm>
#include <unordered_map>
#include <vector>

using std::string;

/*  THEORY:
    * GLFW and ImGui provide their own headers, but we don't want to include them here because...
    *   I want to teach you how to use forward declarations. 
    *   It reduces compile times and dependencies by not including the full header files here.
    * 'forward declaration'? It's when you tell the compiler "Hey, there's a type called X,
    *   just search for it later, I promise it's real." 
    * And, in our Makefile, we need to make sure to link against the GLFW and ImGui libraries 
    *   when we compile.
    *   Basically, add the headers that define these types in the .cpp file, not here in the .h file.
*/
struct GLFWwindow;
#include "imgui.h"
#include "imgui_md/imgui_md.h"

// Font handles provided by the implementation (defined in src/graphics.cpp)
extern ImFont* g_font_regular;
extern ImFont* g_font_bold;
extern ImFont* g_font_bold_large;
extern ImFont* g_font_mono;
extern ImFont* g_font_mono_large;

enum class Theme {
    Light,
    Dark
};

// Why an image failed to load, so the preview can say which one happened:
// the file simply isn't there, or it is there but we couldn't decode it.
enum class ImageError {
    None,        // loaded fine
    NotFound,    // no file at that path
    Unreadable   // file exists but stb_image refused it (corrupt/unsupported)
};

// Content metrics for the columns of one Markdown table, measured from the
// document before it is rendered (see MarkdownRenderer::print). Both are in
// unscaled pixels; the zoom factor is applied when the columns are laid out.
struct TableColumnMetrics {
    std::vector<float> minWidth;        // widest unbreakable run in the column
    std::vector<float> preferredWidth;  // widest cell if it never wrapped
};

// The markdown renderer asks Graphics for image textures (and Graphics owns the
// cache), so Graphics is forward-declared here and its definition comes later.
class Graphics;

class MarkdownRenderer : public imgui_md {
public:
        MarkdownRenderer()
                : m_code_block(false), m_code_draw_list(nullptr), m_code_start(0.0f, 0.0f),
                    m_code_width(0.0f), m_table_width(0.0f), m_table_start(0.0f),
                    m_table_indent_adjust(0.0f),
                    m_document_hash(0), m_document_length(0), m_table_index(0),
                    m_graphics(nullptr) {}

    // The renderer is a member of Graphics, but imgui_md calls into it without
    // ever telling it who owns it — this is how it learns where to look for
    // images (defined in graphics.cpp).
    void setGraphics(Graphics* graphics) { m_graphics = graphics; }

    ImFont* get_font() const override {
        if (m_is_code) {
            return g_font_mono;
        }
        if (m_is_table_header) {
            return g_font_bold;
        }
        switch (m_hlevel) {
            case 0:  return m_is_strong ? g_font_bold : g_font_regular;
            case 1:  return g_font_bold_large;
            default: return g_font_bold;
        }
    }

    // Hyperlink text in the preview. imgui_md would colour links with
    // ImGuiCol_ButtonHovered (a washed-out tan on the light palette, nearly
    // invisible on the dark one); links use ImGuiCol_TextLink instead — the same
    // neon purple in both themes (kLinkColor in graphics.cpp).
    ImVec4 get_color() const override {
        if (!m_href.empty()) {
            return ImGui::GetStyle().Colors[ImGuiCol_TextLink];
        }
        return imgui_md::get_color();
    }

    void BLOCK_CODE(const MD_BLOCK_CODE_DETAIL*, bool e) override {
        if (e) {
            m_is_code = true;
            ImGui::NewLine();
            const float avail = ImGui::GetContentRegionAvail().x;
            const float target = std::min(avail * 0.9f, 900.0f);
            const float x = ImGui::GetCursorScreenPos().x;
            ImDrawList* draw = ImGui::GetWindowDrawList();
            draw->ChannelsSplit(2);
            draw->ChannelsSetCurrent(1);
            m_code_draw_list = draw;
            m_code_start = ImVec2(x, ImGui::GetCursorScreenPos().y);
            m_code_width = target;
            ImGui::PushFont(g_font_mono);
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, 4.0f));
            ImGui::SetCursorScreenPos(ImVec2(x, m_code_start.y + 10.0f));
            m_code_block = true;
        } else {
            if (m_code_block) {
                const float bottom = ImGui::GetCursorScreenPos().y + 6.0f;
                m_code_draw_list->ChannelsSetCurrent(0);
                m_code_draw_list->AddRectFilled(
                    m_code_start,
                    ImVec2(m_code_start.x + m_code_width, bottom),
                        ImGui::GetColorU32(ImVec4(
                            ImGui::GetStyle().Colors[ImGuiCol_FrameBg].x * 0.55f +
                                ImGui::GetStyle().Colors[ImGuiCol_WindowBg].x * 0.45f,
                            ImGui::GetStyle().Colors[ImGuiCol_FrameBg].y * 0.55f +
                                ImGui::GetStyle().Colors[ImGuiCol_WindowBg].y * 0.45f,
                            ImGui::GetStyle().Colors[ImGuiCol_FrameBg].z * 0.55f +
                                ImGui::GetStyle().Colors[ImGuiCol_WindowBg].z * 0.45f,
                            1.0f)),
                    8.0f);
                m_code_draw_list->ChannelsMerge();
                ImGui::PopStyleVar();
                ImGui::PopFont();
                m_code_block = false;
            }
            m_is_code = false;
            ImGui::NewLine();
        }
    }

    void SPAN_CODE(bool e) override {
        if (e) {
            ImGui::PushFont(g_font_mono);
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_Text));
        } else {
            ImGui::PopStyleColor();
            ImGui::PopFont();
        }
    }

    // Table blocks, cells and rows all live in graphics.cpp, next to the column
    // metrics they share.
    //
    // BLOCK_TABLE() gives the table its fixed total width and lays the columns
    // out from the content metrics gathered by print(): every column gets at
    // least the width of its longest word, is allowed to grow towards the width
    // its widest cell would need on one line, and shares what is left over in
    // proportion to how much room each column asked for.
    //
    // BLOCK_TD() re-bases ImGui's indent on where the first line really is:
    // imgui_md starts a cell with SetCursorPos() + SameLine(), so the first line
    // sits ItemSpacing.x right of the indent, while the indent is what positions
    // every wrapped line that follows. It also clears IsSameLine first (see the
    // implementation) so every cell starts on the row top instead of one line
    // below the previous cell.
    //
    // BLOCK_TR() centres each cell's content vertically once the row height is
    // known, which is only at the end of the row.
    void BLOCK_TABLE(const MD_BLOCK_TABLE_DETAIL* d, bool e) override;
    void BLOCK_TD(const MD_BLOCK_TD_DETAIL* d, bool e) override;
    void BLOCK_TR(bool e) override;

    // Render a document. Shadows imgui_md::print() so the renderer can measure
    // the document's table columns *before* laying them out: an immediate-mode
    // render cannot know how wide a column should be until it has seen the
    // content, and it cannot go back once the content has been drawn. Defined in
    // graphics.cpp.
    int print(const char* str, const char* str_end);

    // A click on a link in the preview. Deciding what to do with the target —
    // browser, another document, or the file manager — belongs to the app, which
    // runs outside this render pass, so the target is queued on Graphics and
    // picked up by LumiscriptaApp on a later frame. Defined in graphics.cpp,
    // where Graphics is complete (only forward-declared here).
    void open_url() const override;

    bool get_image(image_info& nfo) const override;

private:
    bool m_code_block;
    ImDrawList* m_code_draw_list;
    ImVec2 m_code_start;
    float m_code_width;
    float m_table_width;
    float m_table_start;
    float m_table_indent_adjust;   // per-cell indent correction, removed at cell exit
    struct CellSpan {
        int vtxStart;
        int vtxEnd;
    };
    std::vector<CellSpan> m_table_row_cells; // vertex ranges for each cell in current row

    // Content metrics for every table in the document: measured by print() and
    // consumed by BLOCK_TABLE() in render order (md4c visits both in the same
    // order, so index N is the same table in both).
    std::vector<TableColumnMetrics> m_table_metrics;
    size_t m_document_hash;     // identity of the document the metrics came from
    size_t m_document_length;
    int    m_table_index;       // table being rendered, indexing m_table_metrics

    Graphics* m_graphics;   // owner; provides the image texture cache
};

class Graphics {
public:
    Graphics();
    ~Graphics();

    // Initialize ImGui + backends. Call after GLFW window exists.
    bool init(GLFWwindow* window);

    // Cleanup all ImGui resources.
    void shutdown();

    // Switch color theme in-place.
    void applyTheme(Theme theme);

    // Call at the start and end of every frame.
    void beginFrame();
    void endFrame();

    // Render the raw markdown editor (editable text area) and the rendered preview (HTML-like).
    void renderEditor(string& content); // not 'const' because we will modify it in-place
    void renderPreview(const string& content);

    // Current active theme.
    Theme getTheme() const;

    // ------------------------------------------------------------------
    // Images (markdown ![alt](path) in the preview)
    // ------------------------------------------------------------------
    // Directory that relative image paths are resolved against. App sets this
    // to the directory of the loaded markdown file.
    void setBaseDirectory(const string& dir);

    // Drop every cached image texture. Called when a new file is loaded so
    // textures belonging to the previous document don't linger on the GPU.
    void clearImageCache();

    // Decode 'src' and upload it as an OpenGL texture (cached by path).
    // Returns false when the file is missing, unreadable or not an image, so a
    // broken reference renders as nothing instead of a broken image. 'error' (if
    // given) tells the two failure modes apart for the fallback message.
    bool getImageTexture(const string& src, ImTextureID& texture, ImVec2& size,
                         ImageError* error = nullptr);

    // Branding wordmark for the app's own chrome (welcome screen and top bar).
    // Returns the natural pixel size; the caller scales it and keeps the aspect
    // ratio. 'theme' picks the version that reads on the current background
    // (light art for the light theme, dark art for the dark theme).
    bool getWordmarkTexture(Theme theme, ImTextureID& texture, ImVec2& size);

    // ------------------------------------------------------------------
    // Hyperlinks (clicks in the preview)
    // ------------------------------------------------------------------
    // Queue the target of a clicked link. MarkdownRenderer::open_url() calls
    // this while a document is being rendered; the app consumes the request on a
    // later frame, because the answer to it is a confirmation dialog.
    void requestOpenLink(const string& href);

    // Hand over the queued href, if any, and clear it. Returns false when no link
    // was clicked since the previous call.
    bool takePendingLink(string& href);

private:
    ImGuiContext* m_ctx;
    GLFWwindow* m_window;
    Theme m_theme;
    bool m_initialized;

    // Loaded images: resolved path -> GL texture name + natural size.
    struct CachedImage {
        unsigned int id;    // GLuint; kept as unsigned int so this header
        float width;        // does not have to drag in the OpenGL headers.
        float height;
    };
    std::unordered_map<string, CachedImage> m_images;
    string m_baseDir;
    string m_pendingLink;   // target of a link clicked in the preview, if any

    // Branding art paths, resolved once at init (absolute or cwd-relative, never
    // joined to the document directory).
    string m_wordmarkLightPath;
    string m_wordmarkDarkPath;

    // Cache + decode + upload, with 'path' used exactly as given. Shared by the
    // markdown images (which resolve against the document first) and the
    // branding art (which is already resolved).
    bool loadTexture(const string& path, ImTextureID& texture, ImVec2& size, ImageError* error);

    void setupStyleLight();
    void setupStyleDark();
};

#endif /* GRAPHICS_H */