#include "lumiscripta/graphics.h"

#include "imgui.h"
#include "imgui/backends/imgui_impl_glfw.h"
#include "imgui/backends/imgui_impl_opengl3.h"
#include "imgui/misc/cpp/imgui_stdlib.h"
#include "imgui_md/imgui_md.h"
#include "IconsFontAwesome/IconsFontAwesome7.h"
#include <GLFW/glfw3.h>
#include <iostream>

// Font handles used by MarkdownRenderer.
ImFont* g_font_regular = nullptr;
ImFont* g_font_bold = nullptr;
ImFont* g_font_bold_large = nullptr;
ImFont* g_font_mono = nullptr;

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
        return false;
    }

    const char* glsl_version = "#version 330";
    if (!ImGui_ImplOpenGL3_Init(glsl_version)) {
        std::cerr << "ImGui_ImplOpenGL3_Init failed\n";
        return false;
    }

    // Load fonts.
    {
        ImGuiIO& io2 = ImGui::GetIO();

        // Build a merged font atlas: Inter base + FontAwesome icons.
        // We load Inter first, then merge FA into it.
        g_font_regular = io2.Fonts->AddFontFromFileTTF("assets/fonts/Inter-Regular.ttf", 17.0f);
        g_font_bold = io2.Fonts->AddFontFromFileTTF("assets/fonts/Inter-Bold.ttf", 17.0f);
        g_font_bold_large = io2.Fonts->AddFontFromFileTTF("assets/fonts/Inter-Bold.ttf", 28.0f);
        g_font_mono = io2.Fonts->AddFontFromFileTTF("assets/fonts/JetBrainsMono-Regular.ttf", 14.0f);

        // Merge FontAwesome 7 Solid icons into the regular font.
        if (g_font_regular) {
            ImFontConfig cfg;
            cfg.MergeMode = true;
            cfg.GlyphMinAdvanceX = 17.0f;
            static const ImWchar icon_ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
            io2.Fonts->AddFontFromFileTTF("assets/fonts/fa-solid-900.otf", 17.0f, &cfg, icon_ranges);
        }
        if (!g_font_regular) {
            std::cerr << "Failed to load Inter-Regular.ttf\n";
        }

        // Fallback chain.
        if (!g_font_regular) g_font_regular = io2.Fonts->AddFontDefault();
        if (!g_font_bold) g_font_bold = g_font_regular;
        if (!g_font_bold_large) g_font_bold_large = g_font_bold;
        if (!g_font_mono) g_font_mono = g_font_regular;

        io2.Fonts->Build();
    }

    applyTheme(Theme::Light);
    m_initialized = true;
    return true;
}

void Graphics::shutdown() {
    if (!m_initialized) return;
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
    ImGui::PushFont(g_font_mono);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImGui::GetStyle().Colors[ImGuiCol_WindowBg]);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12, 12));
    ImGui::InputTextMultiline("##markdown_source", &content, avail,
        ImGuiInputTextFlags_AllowTabInput);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
    ImGui::PopFont();
}

void Graphics::renderPreview(const std::string& content) {
    ImVec2 avail = ImGui::GetContentRegionAvail();
    ImGui::BeginChild("preview_content", avail, false, ImGuiWindowFlags_None);

    if (!content.empty()) {
        static MarkdownRenderer s_renderer;
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