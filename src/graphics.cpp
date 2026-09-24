#include "lumiscripta/graphics.h"
#include "lumiscripta/utils.h"

#include <algorithm>
#include <vector>

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui/backends/imgui_impl_glfw.h"
#include "imgui/backends/imgui_impl_opengl3.h"
#include "imgui/misc/cpp/imgui_stdlib.h"
#include "imgui_md/imgui_md.h"
#include "IconsFontAwesome/IconsFontAwesome7.h"
#include "misc/freetype/imgui_freetype.h"   // ImGuiFreeTypeLoaderFlags_* (FreeType is the loader now)
#include <GLFW/glfw3.h>
#include <cfloat>
#include <fstream>
#include <iostream>
#include <sstream>

// stb_image is a single-header library split in two parts: the declarations and
// the implementation. Exactly one translation unit in the whole program may ask
// for the implementation, and this is that unit — every other file only needs
// the public API, so #define STB_IMAGE_IMPLEMENTATION lives here and nowhere
// else.
#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

// GL_CLAMP_TO_EDGE arrived in OpenGL 1.2. macOS and Linux headers define it,
// but the Windows SDK's GL/gl.h stops at 1.1 — define it ourselves so the
// texture setup below compiles everywhere.
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

static ImGuiWindow* findMultilineTextWindow(ImGuiID input_id) {
    // A multiline input hosts its text inside an inner child window created by
    // ImGui's InputTextEx(). Before the first click, GetInputTextState() returns
    // NULL (the state is not "ours" until activation), yet the child already
    // scrolls on its own — so to draw correct line numbers without user
    // interaction we read the scroll from that child window, not from the state.
    //
    // The child window's own ID is a name hash ("<parent>/<label>_%08X"), not
    // the input ID, so FindWindowByID(input_id) would not find it. ImGui does
    // store the input ID in child->ChildId, so we match on that.
    ImGuiContext& g = *GImGui;
    for (int n = 0; n < g.Windows.Size; ++n) {
        ImGuiWindow* w = g.Windows[n];
        if (w && w->ChildId == input_id)
            return w;
    }
    return nullptr;
}

// Mirror of the word-wrap width InputTextEx() computes for the multiline input's
// inner child window:
//
//   wrap_width = ImMax(1.0f, GetContentRegionAvail().x
//                             + (draw_window->ScrollbarY ? 0.0f : -style.ScrollbarSize));
//
// Inside that call the inner child has zero window padding and no decoration
// offsets (ImGui forces WindowPadding to (0,0) around BeginChildEx), and ImGui
// moves the child's cursor to Pos + FramePadding right after BeginChildEx, so
// GetContentRegionAvail().x there is:
//
//   child->ContentRegionRect.Max.x - (child->Pos.x + FramePadding.x)
//
// Keeping the input's recorded wrap width equal to this value while the field is
// inactive is what stops ImGui from re-centering the view on activation.
static float predictMultilineWrapWidth(ImGuiWindow* text_window,
    float frame_padding_x, float scrollbar_size) {
    if (text_window == nullptr) return 0.0f;
    const float cursor_pos_x = text_window->Pos.x + frame_padding_x;
    const float avail_x = text_window->ContentRegionRect.Max.x - cursor_pos_x;
    const float scrollbar_allowance = text_window->ScrollbarY ? 0.0f : -scrollbar_size;
    return ImMax(1.0f, avail_x + scrollbar_allowance);
}

static void drawEditorLineNumbers(const string& content, const ImVec2& inputMin,
    const ImVec2& inputMax, float scrollY, float wrapWidth) {
    const ImGuiStyle& style = ImGui::GetStyle();
    const float fontSize = ImGui::GetFontSize();
    const float lineHeight = ImGui::GetTextLineHeight();
    const float gutterWidth = 36.0f;
    const float textTop = inputMin.y + style.FramePadding.y - scrollY;
    const float numberX = inputMin.x + 7.0f;
    const float dividerX = inputMin.x + gutterWidth;
    const ImVec4 textColor = style.Colors[ImGuiCol_Text];
    const ImU32 numberColor = ImGui::GetColorU32(ImVec4(
        textColor.x, textColor.y, textColor.z, 0.38f));

    ImDrawList* draw = ImGui::GetForegroundDrawList();
    draw->PushClipRect(inputMin, inputMax, true);
    draw->AddLine(ImVec2(dividerX, inputMin.y),
        ImVec2(dividerX, inputMax.y),
        ImGui::GetColorU32(ImGuiCol_Border), 1.0f);

    const char* source = content.c_str();
    const char* end = source + content.size();
    int lineNumber = 1;
    float row = 0.0f;
    while (source <= end) {
        const char* lineEnd = source;
        while (lineEnd < end && *lineEnd != '\n') ++lineEnd;
        const char* visualStart = source;
        bool firstRow = true;

        do {
            const char* visualEnd = visualStart;
            if (visualStart < lineEnd) {
                visualEnd = g_font_mono->CalcWordWrapPosition(fontSize,
                    visualStart, lineEnd, wrapWidth);
                if (visualEnd <= visualStart) ++visualEnd;
            }
            if (firstRow) {
                const string label = std::to_string(lineNumber);
                draw->AddText(g_font_mono, fontSize * 0.72f,
                    ImVec2(numberX, textTop + row * lineHeight), numberColor,
                    label.c_str());
                firstRow = false;
            }
            ++row;
            visualStart = visualEnd;
        } while (visualStart < lineEnd);

        if (lineEnd == end) break;
        source = lineEnd + 1;
        ++lineNumber;
        if (source == end) {
            const string label = std::to_string(lineNumber);
            draw->AddText(g_font_mono, fontSize * 0.72f,
                ImVec2(numberX, textTop + row * lineHeight), numberColor,
                label.c_str());
            break;
        }
    }
    draw->PopClipRect();
}

// Font handles used by MarkdownRenderer.
ImFont* g_font_regular = nullptr;
ImFont* g_font_bold = nullptr;
ImFont* g_font_bold_large = nullptr;
ImFont* g_font_mono = nullptr;
ImFont* g_font_mono_large = nullptr;

// Pseudo-italic used only for the image error messages in the preview. It is
// Inter-Regular slanted by FreeType (ImGuiFreeTypeLoaderFlags_Oblique applies
// FT_GlyphSlot_Oblique to any outline face), so the project needs no separate
// Italic cut of the font. File-local because only graphics.cpp draws it.
static ImFont* g_font_italic = nullptr;

// Does this file at least look like a font? A truncated download or a text
// file renamed to .ttf would make ImGui assert() unconditionally on font data
// it cannot parse — a hard abort that ImFontFlags_NoLoadError does not cover —
// so such a file is treated exactly like a missing one.
static bool looksLikeFontFile(const string& path) {
	std::ifstream in(path.c_str(), std::ios::in | std::ios::binary);
	if (!in) return false;

	char header[4] = { 0, 0, 0, 0 };
	if (!in.read(header, 4)) return false;

	const unsigned char* h = reinterpret_cast<const unsigned char*>(header);
	if (h[0] == 0x00 && h[1] == 0x01 && h[2] == 0x00 && h[3] == 0x00) return true; // TrueType
	if (h[0] == 'O' && h[1] == 'T' && h[2] == 'T' && h[3] == 'O') return true;      // OpenType (CFF)
	if (h[0] == 't' && h[1] == 't' && h[2] == 'c' && h[3] == 'f') return true;      // TrueType collection
	if (h[0] == 't' && h[1] == 'r' && h[2] == 'u' && h[3] == 'e') return true;      // Apple TrueType
	if (h[0] == 't' && h[1] == 'y' && h[2] == 'p' && h[3] == '1') return true;      // PostScript Type 1
	return false;
}

// Load a font file, but never let a missing file abort the app. Fonts are
// optional by design: the fallback chain in Graphics::init() turns a NULL
// result into ImGui's built-in font, so an asset problem degrades the look
// instead of killing the process.
//
// Why the flag: ImGui reports a missing font through IM_ASSERT_USER_ERROR(),
// and io.ConfigErrorRecoveryEnableAssert defaults to true, so ImGui calls
// assert() — with -O2 and no -DNDEBUG that is a real abort(). NoLoadError
// tells ImGui that we check the return value ourselves (we also pre-check the
// path, so a font that disappears between the check and the load is still
// harmless).
static ImFont* loadFontFile(const string& path, float size_px,
	const ImFontConfig* cfg = nullptr, const ImWchar* ranges = nullptr) {
	if (path.empty()) return nullptr;
	if (!looksLikeFontFile(path)) return nullptr;

	ImFontConfig local;
	if (cfg) local = *cfg;
	local.Flags |= ImFontFlags_NoLoadError;

	return ImGui::GetIO().Fonts->AddFontFromFileTTF(path.c_str(), size_px, &local, ranges);
}

// ---------------------------------------------------------------------------
// Supplementary fonts (icon font, colour emoji font)
//
// ImGui resolves a codepoint by walking the merged sources of *the font being
// used*, in order (imgui_draw.cpp: "for (ImFontConfig* src : font->Sources)"),
// and the FreeType loader decides whether a source has a glyph from the face's
// real coverage (FT_Get_Char_Index). There is no cross-font fallback, so any
// font that should be able to show these glyphs needs its own merged source.
//
// DstFont must be set explicitly. Without it ImGui merges into whichever font
// was added last (imgui_draw.cpp: "font = font_cfg_in->DstFont ? ... : Fonts.back()"),
// which is not necessarily the font we mean.
// ---------------------------------------------------------------------------

// Codepoints the emoji face maps that we do *not* want it to own: some emoji
// fonts map a handful of ASCII codepoints (for keycap sequences), and Inter/FA
// must keep those. Without this, merging the emoji source would shadow ASCII.
//
// Note the lower bound is 0x0001, not 0x0000: exclusion lists are walked as
// "for (; exclude_list[0] != 0; exclude_list += 2)" in
// ImFontAtlasBuildAcceptCodepointForSource() (imgui_draw.cpp), so a range that
// *starts* at 0x0000 terminates the whole list and silently excludes nothing.
// U+0000 itself can never be rendered, so starting at 1 loses nothing.
static const ImWchar kEmojiExcludeAscii[] = { 0x0001, 0x00FF, 0, 0 };

// Everything *outside* Font Awesome's private-use block
// (ICON_MIN_FA..ICON_MAX_FA), i.e. what the icon source must never answer for.
//
// This list, not GlyphRanges, is what confines a merged source to its own
// codepoints. ImGui 1.92 loads glyphs lazily and ImFontBaked_BuildLoadGlyph()
// resolves a codepoint by asking the font's sources *in order*, taking the first
// one that reports the codepoint, and it honours GlyphExcludeRanges but never
// GlyphRanges (imgui_draw.cpp: "if (!src->GlyphExcludeRanges ||
// ImFontAtlasBuildAcceptCodepointForSource(src, codepoint))"). Passing the icon
// range as GlyphRanges therefore does not stop the icon font from answering for
// anything else.
//
// Font Awesome 7 is no longer a pure private-use font: fa-solid-900.otf also
// maps ~440 *real* Unicode codepoints as monochrome "alias" glyphs — ASCII
// letters and digits, currency signs, arrows and most of the emoji plane
// (U+1F4E6, U+1F4DD, U+1F525, U+1F600, …). Merged before the colour emoji
// source, FA wins those lookups and the emoji renders black and white. Hence:
// the icon source may only ever supply its PUA block.
//
// The ranges must not start at 0x0000 (see kEmojiExcludeAscii): a leading
// zero terminates the list, which is exactly how an earlier version of this
// list managed to exclude nothing at all.
static const ImWchar kIconExcludeRanges[] = {
    0x0001, ICON_MIN_FA - 1,     // everything below Font Awesome's PUA block
    ICON_MAX_FA + 1, 0x10FFFF,   // everything above it (IM_UNICODE_CODEPOINT_MAX)
    0, 0
};

// Emoji and pictographic ranges. With ImGuiBackendFlags_RendererHasTextures
// (which the OpenGL3 backend sets) glyphs are baked lazily, so this list is not
// preloaded; it documents intent and keeps the legacy non-texture path correct.
static const ImWchar kEmojiRanges[] = {
    0x2190, 0x21FF,     // arrows
    0x2300, 0x23FF,     // misc technical (⌚ ⏰ ⏳ …)
    0x2600, 0x27BF,     // misc symbols + dingbats ( ✨ ✔ ❤  …)
    0x2B00, 0x2BFF,     // misc symbols and arrows (⭐ ⬛ …)
    0x1F000, 0x1FAFF,   // emoji: faces, people, objects, symbols, flags
    0,
    0
};

// Loader settings for the colour emoji source.
// - LoadColor: rasterize the font's colour layers ('COLR' v0) as BGRA pixels.
// - Bitmap: imgui_freetype sets FT_LOAD_NO_BITMAP by default
//   ("if ((UserFlags & ImGuiFreeTypeLoaderFlags_Bitmap) == 0) LoadFlags |= FT_LOAD_NO_BITMAP;").
//   Keeping this clears that, which is what CBDT/PNG colour fonts need (colour
//   bitmaps are only handed out when the caller accepts bitmaps). Harmless for
//   COLR fonts, so it makes the emoji source work for both formats.
//
// Font-format constraint (why the emoji asset is Twemoji Mozilla and not Noto
// Color Emoji): FreeType itself can only rasterize 'COLR' v0 colour tables.
// FreeType's FT_LOAD_COLOR documentation states that for 'COLR' v1 there "is no
// rendering support", and SVG-in-OpenType glyphs additionally require an
// external renderer that imgui_freetype only wires up when built with
// IMGUI_ENABLE_FREETYPE_PLUTOSVG / _LUNASVG. Current Noto Color Emoji builds are
// COLRv1 (+SVG for older ones), so FreeType returns an empty outline for them and
// the glyph renders as a blank advance. Twemoji Mozilla is COLRv0 (vector colour
// layers), which FreeType rasterizes at any requested size -> crisp at 17px.
static const unsigned int kEmojiLoaderFlags =
    ImGuiFreeTypeLoaderFlags_LoadColor | ImGuiFreeTypeLoaderFlags_Bitmap;

// Merge a supplementary font into an already-loaded font.
// 'loader_flags' is per-source loader settings (e.g. LoadColor for emoji);
// pass 0 for plain glyphs like the icon font.
static bool mergeFontInto(ImFont* dst, const string& path, float size_px,
    unsigned int loader_flags, const ImWchar* exclude_ranges, const ImWchar* glyph_ranges) {
    if (dst == nullptr || path.empty()) return false;

    ImFontConfig cfg;
    cfg.MergeMode = true;
    cfg.DstFont = dst;
    cfg.FontLoaderFlags = loader_flags;
    cfg.GlyphExcludeRanges = exclude_ranges;

    return loadFontFile(path, size_px, &cfg, glyph_ranges) != nullptr;
}

Graphics::Graphics()
    : m_ctx(nullptr), m_window(nullptr), m_theme(Theme::Light), m_initialized(false) {}

Graphics::~Graphics() {}

bool Graphics::init(GLFWwindow* window) {
    if (m_initialized) return true;
    if (!window) return false;
    m_window = window;

    IMGUI_CHECKVERSION();
    m_ctx = ImGui::CreateContext();
    ImGui::SetCurrentContext(m_ctx);
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Disable .ini file — we don't want ImGui saving window positions.
    io.IniFilename = nullptr;

    if (!ImGui_ImplGlfw_InitForOpenGL(window, true)) {
        std::cerr << "ImGui_ImplGlfw_InitForOpenGL failed\n";
        ImGui::DestroyContext(m_ctx);   // don't leak the context we just created
        m_ctx = nullptr;
        m_window = nullptr;
        return false;
    }

    const char* glsl_version = "#version 330";
    if (!ImGui_ImplOpenGL3_Init(glsl_version)) {
        std::cerr << "ImGui_ImplOpenGL3_Init failed\n";
        ImGui_ImplGlfw_Shutdown();      // undo what did succeed before bailing out
        ImGui::DestroyContext(m_ctx);
        m_ctx = nullptr;
        m_window = nullptr;
        return false;
    }

    // Load fonts. The paths are resolved against the executable, not against
    // the working directory (see resolveAsset() in utils.h).
    {
        ImGuiIO& io2 = ImGui::GetIO();

        const string regular_path = resolveAsset("fonts/Inter-Regular.ttf");
        const string bold_path = resolveAsset("fonts/Inter-Bold.ttf");
        const string mono_path = resolveAsset("fonts/JetBrainsMono-Regular.ttf");
        const string icons_path = resolveAsset("fonts/fa-solid-900.otf");
        const string emoji_path = resolveAsset("fonts/TwemojiMozilla.ttf");

        // Build a merged font atlas: Inter base + FontAwesome icons.
        // We load Inter first, then merge FA into it.
        g_font_regular = loadFontFile(regular_path, 17.0f);
        g_font_bold = loadFontFile(bold_path, 17.0f);
        g_font_bold_large = loadFontFile(bold_path, 28.0f);
        g_font_mono = loadFontFile(mono_path, 14.0f);
        g_font_mono_large = loadFontFile(mono_path, 17.5f);

        // Merge FontAwesome 7 Solid icons into the regular font (the UI font).
        //
        // DstFont and GlyphExcludeRanges are both load-bearing here:
        // - Without DstFont ImGui merges into Fonts.back() — whichever font was
        //   added last, which at this point is the mono font of the editor, not
        //   the font we mean. Icons then live in the wrong font and, worse, the
        //   icon source shadows that font's other glyphs.
        // - Without GlyphExcludeRanges the icon source answers for every
        //   codepoint FA happens to map, including its monochrome Unicode
        //   aliases — see kIconExcludeRanges.
        bool icons_loaded = false;
        if (g_font_regular && !icons_path.empty()) {
            ImFontConfig cfg;
            cfg.MergeMode = true;
            cfg.DstFont = g_font_regular;
            cfg.GlyphMinAdvanceX = 17.0f;
            cfg.GlyphExcludeRanges = kIconExcludeRanges;
            static const ImWchar icon_ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
            icons_loaded = (loadFontFile(icons_path, 17.0f, &cfg, icon_ranges) != nullptr);
        }

        // Merge the colour emoji font into every font that renders text, so emoji
        // in body text, headings and code all render. kEmojiLoaderFlags asks
        // FreeType to rasterize the font's colour layers instead of a monochrome
        // outline; ImGui then packs them as RGBA in the atlas, which the OpenGL3
        // backend uploads unchanged. The emoji face is kept away from ASCII so
        // Inter keeps owning it.
        bool emoji_loaded = false;
        if (!emoji_path.empty()) {
            emoji_loaded |= mergeFontInto(g_font_regular, emoji_path, 17.0f,
                kEmojiLoaderFlags, kEmojiExcludeAscii, kEmojiRanges);
            emoji_loaded |= mergeFontInto(g_font_bold, emoji_path, 17.0f,
                kEmojiLoaderFlags, kEmojiExcludeAscii, kEmojiRanges);
            emoji_loaded |= mergeFontInto(g_font_bold_large, emoji_path, 28.0f,
                kEmojiLoaderFlags, kEmojiExcludeAscii, kEmojiRanges);
            emoji_loaded |= mergeFontInto(g_font_mono, emoji_path, 14.0f,
                kEmojiLoaderFlags, kEmojiExcludeAscii, kEmojiRanges);
            emoji_loaded |= mergeFontInto(g_font_mono_large, emoji_path, 17.5f,
                kEmojiLoaderFlags, kEmojiExcludeAscii, kEmojiRanges);
        }

        // Pseudo-italic for image error messages: FreeType slants the outline
        // (FT_GlyphSlot_Oblique) instead of needing an Italic cut of Inter.
        if (!regular_path.empty()) {
            ImFontConfig italicCfg;
            italicCfg.FontLoaderFlags = ImGuiFreeTypeLoaderFlags_Oblique;
            g_font_italic = loadFontFile(regular_path, 17.0f, &italicCfg);
        }

        // Fallback chain: assets are optional, so say what happened and carry on.
        if (!g_font_regular) {
            if (!regular_path.empty()) {
                std::cerr << "Graphics: not a readable font file: " << regular_path
                    << " — falling back to the built-in font.\n";
            } else {
                std::cerr
                    << "Graphics: font assets not found — falling back to the built-in font.\n"
                    << "          Set LUMISCRIPTA_ASSETS to the assets folder, or run from the project root. Searched:\n"
                    << assetSearchPaths();
            }
        } else if (!icons_loaded) {
            std::cerr << "Graphics: icon font unavailable — icons will not render.\n";
        }
        if (emoji_path.empty()) {
            std::cerr << "Graphics: colour emoji font not found (fonts/TwemojiMozilla.ttf) —"
                " emoji will not render.\n";
        } else if (!emoji_loaded) {
            std::cerr << "Graphics: could not load the colour emoji font — emoji will not render."
                " (FreeType only rasterizes 'COLR' v0 colour layers.)\n";
        }

        if (!g_font_regular) g_font_regular = io2.Fonts->AddFontDefault();
        if (!g_font_bold) g_font_bold = g_font_regular;
        if (!g_font_bold_large) g_font_bold_large = g_font_bold;
        if (!g_font_mono) g_font_mono = g_font_regular;
        if (!g_font_mono_large) g_font_mono_large = g_font_mono;

        io2.Fonts->Build();
    }

    // Branding art for the app's own chrome (welcome screen and top bar). The
    // textures themselves are decoded lazily by getWordmarkTexture(); only the
    // paths are resolved here.
    m_wordmarkLightPath = resolveAsset("branding/wordmark-light.png");
    m_wordmarkDarkPath = resolveAsset("branding/wordmark-dark.png");

    applyTheme(Theme::Light);
    m_initialized = true;
    return true;
}

void Graphics::shutdown() {
    if (!m_initialized) return;
    clearImageCache();   // textures must die while the GL context is alive
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext(m_ctx);
    m_ctx = nullptr;
    m_window = nullptr;
    m_initialized = false;
}

void Graphics::applyTheme(Theme theme) {
    m_theme = theme;
    if (theme == Theme::Light) setupStyleLight();
    else setupStyleDark();
}

void Graphics::beginFrame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void Graphics::endFrame() {
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void Graphics::renderEditor(string& content) {
    ImVec2 avail = ImGui::GetContentRegionAvail();
    ImVec4 canvas = ImGui::GetStyle().Colors[ImGuiCol_WindowBg];
    ImVec4 paper = ImGui::GetStyle().Colors[ImGuiCol_FrameBg];
    ImVec4 divider = ImGui::GetStyle().Colors[ImGuiCol_Border];
    ImVec4 editorText = ImGui::GetStyle().Colors[ImGuiCol_Text];

    ImGui::PushStyleColor(ImGuiCol_ChildBg, paper);
    ImGui::PushStyleColor(ImGuiCol_Border, divider);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
    ImGui::BeginChild("editor_surface", avail, true, ImGuiWindowFlags_NoScrollbar);

    const float gutterWidth = 36.0f;
    const float editorFramePadding = 8.0f;
    const float editorScrollbarSize = 8.0f;
    // Use a slightly larger monospace font in the editor (17.5px vs 14px)
    // so the code matches the preview's body text size. The line-number gutter
    // keeps the smaller mono font for readability.
    ImGui::PushFont(g_font_mono_large);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, canvas);
    ImGui::PushStyleColor(ImGuiCol_Text, editorText);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
        ImVec2(gutterWidth + editorFramePadding, editorFramePadding));
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, editorScrollbarSize);

    // ImGui always centers newly activated multiline text on its cursor. When it
    // reactivates an untouched field whose word-wrap state was never initialized,
    // InputTextEx treats the current width as a wrap-width change, sets its
    // CursorCenterY flag, and moves the inner child window on the activation
    // frame, even when the user scrolled elsewhere. Persist that inherited
    // scroll across that activation frame, clamped, so only the caret moves.
    const ImGuiID markdownInputId = ImGui::GetID("##markdown_source");
    ImGuiWindow* inactiveTextWindow = findMultilineTextWindow(markdownInputId);
    const float preActivationScrollY = inactiveTextWindow ? inactiveTextWindow->Scroll.y : 0.0f;
    const ImVec2 preActivationScrollMax = inactiveTextWindow ? inactiveTextWindow->ScrollMax : ImVec2(0.0f, 0.0f);
    ImGui::InputTextMultiline("##markdown_source", &content, ImGui::GetContentRegionAvail(),
        ImGuiInputTextFlags_AllowTabInput | ImGuiInputTextFlags_WordWrap);
    if (ImGui::IsItemActivated() && inactiveTextWindow != nullptr) {
        float inheritedScrollY = preActivationScrollY;
        if (inheritedScrollY < 0.0f)
            inheritedScrollY = 0.0f;
        if (inheritedScrollY > preActivationScrollMax.y)
            inheritedScrollY = preActivationScrollMax.y;
        inactiveTextWindow->Scroll.y = inheritedScrollY;
    }

    // Activation-time centering fix.
    //
    // While the field has never been activated, ImGuiInputTextState::WrapWidth is
    // still 0. On the activation frame InputTextEx() compares that against the
    // wrap width it just computed, concludes "the field was resized", and sets
    // CursorCenterY: it then re-centers the view on the caret, moves the inner
    // child window's scroll, recomputes the visible-line range and drops the
    // caret for that frame (imgui_widgets.cpp, "if (state->CursorCenterY)").
    // The frame is rendered at that new offset before we can undo the scroll,
    // which is the one-frame flash/"reload" seen on the first click.
    //
    // Feeding the state the wrap width ImGui is about to compute makes that
    // comparison a no-op, so activation only moves the caret. ImGui refreshes
    // this value itself while the field is active, so we only maintain it while
    // inactive (and only when the state is not owned by some other widget).
    ImGuiWindow* seedTextWindow = findMultilineTextWindow(markdownInputId);
    if (!ImGui::IsItemActive() && seedTextWindow != nullptr) {
        ImGuiContext& g = *ImGui::GetCurrentContext();
        if (g.InputTextState.ID == 0 || g.InputTextState.ID == markdownInputId) {
            g.InputTextState.WrapWidth = predictMultilineWrapWidth(
                seedTextWindow, gutterWidth + editorFramePadding, editorScrollbarSize);
        }
    }
    const ImVec2 inputMin = ImGui::GetItemRectMin();
    const ImVec2 inputMax = ImGui::GetItemRectMax();
    ImGuiInputTextState* inputState = ImGui::GetInputTextState(ImGui::GetItemID());
    ImGuiWindow* textWindow = findMultilineTextWindow(ImGui::GetItemID());

    // Vertical scroll for the gutter: the textile's inner child window already
    // scrolls on its own even before the first click, but GetInputTextState()
    // returns NULL until the widget is activated (state->ID != id). So we read
    // from the child window; fall back to the state (if active) and then to 0.
    // (ScrollbarY is only true when the scrollbar is actually visible, which is
    // not the case before scrolling — so don't gate on it.)
    const float inputScrollY =
        textWindow ? textWindow->Scroll.y
        : (inputState ? inputState->Scroll.y : 0.0f);

    // Wrap width: prefer the state's authoritative value when available; fall
    // back to a computed value from the child window's usable width (mirrors
    // ImGui's own formula in InputTextEx).
    const float wrapWidth =
        (inputState && inputState->WrapWidth > 0.0f)
            ? inputState->WrapWidth
            : (textWindow ? textWindow->ContentRegionRect.GetSize().x
                          - (textWindow->ScrollbarX ? 0.0f : ImGui::GetStyle().ScrollbarSize)
              : inputMax.x - inputMin.x - ImGui::GetStyle().FramePadding.x * 2.0f);

    drawEditorLineNumbers(content, inputMin, inputMax, inputScrollY, wrapWidth);
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
    ImGui::PopFont();

    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}

void Graphics::renderPreview(const std::string& content) {
    ImVec2 avail = ImGui::GetContentRegionAvail();
    ImGui::BeginChild("preview_content", avail, false, ImGuiWindowFlags_None);

    if (!content.empty()) {
        static MarkdownRenderer s_renderer;
        // imgui_md never tells the renderer who owns it; this is the hook that
        // lets it find the image cache.
        s_renderer.setGraphics(this);
        s_renderer.print(content.c_str(), content.c_str() + content.size());
    } else {
        ImGui::TextDisabled("No file loaded. Use File -> Open or pass a path on the command line.");
    }

    ImGui::EndChild();
}

Theme Graphics::getTheme() const {
    return m_theme;
}

// ---------------------------------------------------------------------------
// Images — the ![alt](path) syntax in the preview
// ---------------------------------------------------------------------------

// Remote references (http://…, https://…) have no chance here: Lumiscripta has
// no HTTP client, and a viewer should never block on the network anyway.
static bool isRemoteReference(const string& src) {
    return src.find("://") != string::npos;
}

// Absolute paths are left alone, everything else is relative to the markdown
// file that referenced it.
static bool isAbsolutePath(const string& path) {
#ifdef _WIN32
    return path.size() > 1 && path[1] == ':';   // C:\pictures\cat.png
#else
    return !path.empty() && path[0] == '/';     // /home/me/cat.png
#endif
}

static string resolveImagePath(const string& baseDir, const string& src) {
    if (isAbsolutePath(src) || baseDir.empty()) return src;
    return joinPath(baseDir, src);
}

void Graphics::setBaseDirectory(const string& dir) {
    m_baseDir = dir;
}

void Graphics::clearImageCache() {
    // The GL context is alive whenever we are initialized, so this is a safe
    // place to hand the textures back to the driver.
    if (m_initialized) {
        for (std::unordered_map<string, CachedImage>::const_iterator it = m_images.begin();
             it != m_images.end(); ++it) {
            unsigned int id = it->second.id;
            if (id != 0) glDeleteTextures(1, &id);
        }
    }
    m_images.clear();
}

bool Graphics::getImageTexture(const string& src, ImTextureID& texture, ImVec2& size,
                               ImageError* error) {
    if (error) *error = ImageError::None;
    if (!m_initialized || src.empty() || isRemoteReference(src)) {
        // No local file matches a remote URL (and the app has no HTTP client).
        if (error) *error = ImageError::NotFound;
        return false;
    }

    return loadTexture(resolveImagePath(m_baseDir, src), texture, size, error);
}

// Branding art is resolved against the assets folder, not the document, so it
// must not go through resolveImagePath(). Everything else is the same pipeline.
bool Graphics::getWordmarkTexture(Theme theme, ImTextureID& texture, ImVec2& size) {
    const string& path = (theme == Theme::Dark) ? m_wordmarkDarkPath : m_wordmarkLightPath;
    if (!m_initialized || path.empty()) return false;
    return loadTexture(path, texture, size, nullptr);
}

bool Graphics::loadTexture(const string& path, ImTextureID& texture, ImVec2& size,
                           ImageError* error) {
    // Already decoded and uploaded? Then this frame costs one map lookup.
    std::unordered_map<string, CachedImage>::const_iterator cached = m_images.find(path);
    if (cached != m_images.end()) {
        texture = static_cast<ImTextureID>(static_cast<intptr_t>(cached->second.id));
        size = ImVec2(cached->second.width, cached->second.height);
        return true;
    }

    // Ask before decoding, so "the file is not there" and "the file is there
    // but I cannot read it" stay two different failures.
    if (!pathExists(path)) {
        if (error) *error = ImageError::NotFound;
        return false;
    }

    int width = 0;
    int height = 0;
    int fileChannels = 0;
    // Always decode to RGBA: ImGui's renderer expects four components, and a
    // grayscale PNG or a palette GIF would otherwise come out with wrong
    // strides. (fileChannels only receives the source channel count.)
    unsigned char* pixels = stbi_load(path.c_str(), &width, &height, &fileChannels, 4);
    if (!pixels || width <= 0 || height <= 0) {
        if (pixels) stbi_image_free(pixels);
        if (error) *error = ImageError::Unreadable;   // exists but undecodable
        return false;
    }

    unsigned int id = 0;
    glGenTextures(1, &id);
    if (id == 0) {
        stbi_image_free(pixels);
        if (error) *error = ImageError::Unreadable;
        return false;
    }

    glBindTexture(GL_TEXTURE_2D, id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glBindTexture(GL_TEXTURE_2D, 0);
    stbi_image_free(pixels);

    CachedImage image;
    image.id = id;
    image.width = static_cast<float>(width);
    image.height = static_cast<float>(height);
    m_images[path] = image;

    texture = static_cast<ImTextureID>(static_cast<intptr_t>(id));
    size = ImVec2(image.width, image.height);
    return true;
}

// imgui_md calls this for every ![alt](path) it meets. Success: hand back the
// texture and centre the image on the line (imgui_md will scale and draw it).
// Failure: emit a centred italic message instead — imgui_md suppresses the alt
// text itself (render_text() bails out while m_is_image is set), so this is the
// only place a broken reference becomes visible to the user.
bool MarkdownRenderer::get_image(image_info& nfo) const {
    if (m_graphics == nullptr) return false;

    ImageError error = ImageError::None;
    ImTextureID texture = ImTextureID_Invalid;
    ImVec2 size(0.0f, 0.0f);
    if (!m_graphics->getImageTexture(m_href, texture, size, &error)) {
        // The message must be centred the same way imgui_md centers the image,
        // i.e. against the text width and the current zoom level.
        const char* message =
            (error == ImageError::Unreadable)
                ? "Image file is unreadable (corrupted?)"
                : "Image file not found";

        if (g_font_italic) ImGui::PushFont(g_font_italic);
        const float textW = ImGui::CalcTextSize(message).x;
        const float centre = (ImGui::GetContentRegionAvail().x - textW) * 0.5f;
        if (centre > 0.0f) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + centre);
        ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled), message);
        if (g_font_italic) ImGui::PopFont();

        return false;
    }

    nfo.texture_id = texture;
    nfo.size = size;
    nfo.uv0 = ImVec2(0.0f, 0.0f);
    nfo.uv1 = ImVec2(1.0f, 1.0f);
    nfo.col_tint = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    nfo.col_border = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);

    // Centre horizontally when the image is narrower than the available width;
    // the base class narrows wider images to fit right after we return, so a
    // non-positive offset is deliberately skipped.
    const float scale = ImGui::GetIO().FontGlobalScale;
    const float available = ImGui::GetContentRegionAvail().x;
    const float centre = (available - nfo.size.x * scale) * 0.5f;
    if (centre > 0.0f) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + centre);

    return true;
}

// ---------------------------------------------------------------------------
// Style helpers
// ---------------------------------------------------------------------------

static void setupStyleCommon() {
    ImGuiStyle& style = ImGui::GetStyle();

    // Spacing — generous but not wasteful.
    style.WindowPadding = ImVec2(0, 0);      // Managed per-window.
    style.FramePadding = ImVec2(10, 6);
    style.CellPadding = ImVec2(10, 8);       // Table cells breathe.
    style.ItemSpacing = ImVec2(8, 6);
    style.ItemInnerSpacing = ImVec2(6, 4);
    style.IndentSpacing = 28.0f;

    // Rounding — soft, modern.
    style.WindowRounding = 0.0f;
    style.ChildRounding = 8.0f;
    style.FrameRounding = 6.0f;
    style.PopupRounding = 8.0f;
    style.ScrollbarRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.TabRounding = 6.0f;

    // Scrollbar — hairline.
    style.ScrollbarSize = 4.0f;

    // Borders — subtle or none.
    style.WindowBorderSize = 0.0f;
    style.ChildBorderSize = 0.0f;
    style.PopupBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;
    style.TabBorderSize = 0.0f;
}

void Graphics::setupStyleLight() {
    setupStyleCommon();
    ImGui::StyleColorsLight();
    ImGuiStyle& style = ImGui::GetStyle();

    ImVec4 bg(250.0f / 255.0f, 247.0f / 255.0f, 242.0f / 255.0f, 1.0f);
    ImVec4 bgHover(244.0f / 255.0f, 241.0f / 255.0f, 236.0f / 255.0f, 1.0f);
    ImVec4 surface(227.0f / 255.0f, 221.0f / 255.0f, 213.0f / 255.0f, 1.0f);
    ImVec4 surfaceHover(217.0f / 255.0f, 211.0f / 255.0f, 203.0f / 255.0f, 1.0f);
    ImVec4 surfaceActive(207.0f / 255.0f, 201.0f / 255.0f, 193.0f / 255.0f, 1.0f);
    ImVec4 text(45.0f / 255.0f, 42.0f / 255.0f, 40.0f / 255.0f, 1.0f);
    ImVec4 textSecondary(122.0f / 255.0f, 111.0f / 255.0f, 102.0f / 255.0f, 1.0f);
    ImVec4 accent(184.0f / 255.0f, 174.0f / 255.0f, 164.0f / 255.0f, 1.0f);
    ImVec4 accentHover(164.0f / 255.0f, 154.0f / 255.0f, 144.0f / 255.0f, 1.0f);

    style.Colors[ImGuiCol_Text] = text;
    style.Colors[ImGuiCol_TextDisabled] = textSecondary;
    style.Colors[ImGuiCol_WindowBg] = bg;
    style.Colors[ImGuiCol_ChildBg] = bg;
    style.Colors[ImGuiCol_PopupBg] = bg;
    style.Colors[ImGuiCol_Border] = accent;
    style.Colors[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);

    // The input-text caret uses its own color slot, which is not used by the
    // app's custom theme. Give it a visible color (same as body text) so the
    // real caret — ImGui draws it, blinks it, positions it correctly — becomes
    // visible instead of the alpha-0 placeholder slot left by the ImGuiStyle
    // constructor.
    style.Colors[ImGuiCol_InputTextCursor] = text;

    style.Colors[ImGuiCol_FrameBg] = surface;
    style.Colors[ImGuiCol_FrameBgHovered] = surfaceHover;
    style.Colors[ImGuiCol_FrameBgActive] = surfaceActive;

    style.Colors[ImGuiCol_TitleBg] = bg;
    style.Colors[ImGuiCol_TitleBgActive] = bg;
    style.Colors[ImGuiCol_TitleBgCollapsed] = bg;

    style.Colors[ImGuiCol_MenuBarBg] = bg;

    style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0, 0, 0, 0);
    style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(text.x, text.y, text.z, 0.10f);
    style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(text.x, text.y, text.z, 0.22f);
    style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(text.x, text.y, text.z, 0.35f);

    style.Colors[ImGuiCol_CheckMark] = text;
    style.Colors[ImGuiCol_SliderGrab] = accent;
    style.Colors[ImGuiCol_SliderGrabActive] = accentHover;

    style.Colors[ImGuiCol_Button] = surface;
    style.Colors[ImGuiCol_ButtonHovered] = surfaceHover;
    style.Colors[ImGuiCol_ButtonActive] = surfaceActive;

    style.Colors[ImGuiCol_Header] = surface;
    style.Colors[ImGuiCol_HeaderHovered] = surfaceHover;
    style.Colors[ImGuiCol_HeaderActive] = surfaceActive;

    style.Colors[ImGuiCol_Separator] = accent;
    style.Colors[ImGuiCol_SeparatorHovered] = accentHover;
    style.Colors[ImGuiCol_SeparatorActive] = textSecondary;

    style.Colors[ImGuiCol_ResizeGrip] = ImVec4(0, 0, 0, 0);
    style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(text.x, text.y, text.z, 0.08f);
    style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(text.x, text.y, text.z, 0.18f);

    style.Colors[ImGuiCol_Tab] = surface;
    style.Colors[ImGuiCol_TabHovered] = surfaceHover;
    style.Colors[ImGuiCol_TabActive] = surfaceActive;
    style.Colors[ImGuiCol_TabUnfocused] = surface;
    style.Colors[ImGuiCol_TabUnfocusedActive] = surfaceActive;

    style.Colors[ImGuiCol_TableHeaderBg] = surface;
    style.Colors[ImGuiCol_TableBorderStrong] = accent;
    style.Colors[ImGuiCol_TableBorderLight] = ImVec4(accent.x, accent.y, accent.z, 0.5f);
    style.Colors[ImGuiCol_TableRowBg] = ImVec4(0, 0, 0, 0);
    style.Colors[ImGuiCol_TableRowBgAlt] = ImVec4(text.x, text.y, text.z, 0.025f);

    style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(text.x, text.y, text.z, 0.18f);
    style.Colors[ImGuiCol_DragDropTarget] = textSecondary;
    style.Colors[ImGuiCol_NavHighlight] = accent;
    style.Colors[ImGuiCol_NavWindowingHighlight] = accent;
    style.Colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0, 0, 0, 0.20f);
    style.Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0, 0, 0, 0.20f);
}

void Graphics::setupStyleDark() {
    setupStyleCommon();
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();

    ImVec4 bg(27.0f / 255.0f, 29.0f / 255.0f, 32.0f / 255.0f, 1.0f);
    ImVec4 bgHover(35.0f / 255.0f, 37.0f / 255.0f, 42.0f / 255.0f, 1.0f);
    ImVec4 surface(42.0f / 255.0f, 45.0f / 255.0f, 51.0f / 255.0f, 1.0f);
    ImVec4 surfaceHover(52.0f / 255.0f, 55.0f / 255.0f, 62.0f / 255.0f, 1.0f);
    ImVec4 surfaceActive(62.0f / 255.0f, 65.0f / 255.0f, 72.0f / 255.0f, 1.0f);
    ImVec4 text(233.0f / 255.0f, 237.0f / 255.0f, 241.0f / 255.0f, 1.0f);
    ImVec4 textSecondary(138.0f / 255.0f, 145.0f / 255.0f, 153.0f / 255.0f, 1.0f);
    ImVec4 accent(74.0f / 255.0f, 79.0f / 255.0f, 86.0f / 255.0f, 1.0f);
    ImVec4 accentHover(94.0f / 255.0f, 99.0f / 255.0f, 106.0f / 255.0f, 1.0f);

    style.Colors[ImGuiCol_Text] = text;
    style.Colors[ImGuiCol_TextDisabled] = textSecondary;
    style.Colors[ImGuiCol_WindowBg] = bg;
    style.Colors[ImGuiCol_ChildBg] = bg;
    style.Colors[ImGuiCol_PopupBg] = ImVec4(35.0f / 255.0f, 37.0f / 255.0f, 42.0f / 255.0f, 1.0f);
    style.Colors[ImGuiCol_Border] = accent;
    style.Colors[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);

    // Same caret color as the light theme: visible body-text color so the
    // input-text caret (drawn by ImGui, blinking, correctly positioned) is
    // visible in both themes.
    style.Colors[ImGuiCol_InputTextCursor] = text;

    style.Colors[ImGuiCol_FrameBg] = surface;
    style.Colors[ImGuiCol_FrameBgHovered] = surfaceHover;
    style.Colors[ImGuiCol_FrameBgActive] = surfaceActive;

    style.Colors[ImGuiCol_TitleBg] = bg;
    style.Colors[ImGuiCol_TitleBgActive] = bg;
    style.Colors[ImGuiCol_TitleBgCollapsed] = bg;

    style.Colors[ImGuiCol_MenuBarBg] = bg;

    style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0, 0, 0, 0);
    style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(text.x, text.y, text.z, 0.10f);
    style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(text.x, text.y, text.z, 0.22f);
    style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(text.x, text.y, text.z, 0.35f);

    style.Colors[ImGuiCol_CheckMark] = text;
    style.Colors[ImGuiCol_SliderGrab] = accent;
    style.Colors[ImGuiCol_SliderGrabActive] = accentHover;

    style.Colors[ImGuiCol_Button] = surface;
    style.Colors[ImGuiCol_ButtonHovered] = surfaceHover;
    style.Colors[ImGuiCol_ButtonActive] = surfaceActive;

    style.Colors[ImGuiCol_Header] = surface;
    style.Colors[ImGuiCol_HeaderHovered] = surfaceHover;
    style.Colors[ImGuiCol_HeaderActive] = surfaceActive;

    style.Colors[ImGuiCol_Separator] = accent;
    style.Colors[ImGuiCol_SeparatorHovered] = accentHover;
    style.Colors[ImGuiCol_SeparatorActive] = textSecondary;

    style.Colors[ImGuiCol_ResizeGrip] = ImVec4(0, 0, 0, 0);
    style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(text.x, text.y, text.z, 0.08f);
    style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(text.x, text.y, text.z, 0.18f);

    style.Colors[ImGuiCol_Tab] = surface;
    style.Colors[ImGuiCol_TabHovered] = surfaceHover;
    style.Colors[ImGuiCol_TabActive] = surfaceActive;
    style.Colors[ImGuiCol_TabUnfocused] = surface;
    style.Colors[ImGuiCol_TabUnfocusedActive] = surfaceActive;

    style.Colors[ImGuiCol_TableHeaderBg] = surface;
    style.Colors[ImGuiCol_TableBorderStrong] = accent;
    style.Colors[ImGuiCol_TableBorderLight] = ImVec4(accent.x, accent.y, accent.z, 0.5f);
    style.Colors[ImGuiCol_TableRowBg] = ImVec4(0, 0, 0, 0);
    style.Colors[ImGuiCol_TableRowBgAlt] = ImVec4(text.x, text.y, text.z, 0.04f);

    style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(text.x, text.y, text.z, 0.25f);
    style.Colors[ImGuiCol_DragDropTarget] = textSecondary;
    style.Colors[ImGuiCol_NavHighlight] = accent;
    style.Colors[ImGuiCol_NavWindowingHighlight] = accent;
    style.Colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0, 0, 0, 0.35f);
    style.Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0, 0, 0, 0.35f);
}