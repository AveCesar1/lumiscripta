#include "lumiscripta/app.h"
#include "lumiscripta/file.h"
#include "lumiscripta/graphics.h"

#include <GLFW/glfw3.h>
#include "imgui.h"
#include "imgui/backends/imgui_impl_glfw.h"
#include "imgui/backends/imgui_impl_opengl3.h"
#include "IconsFontAwesome/IconsFontAwesome7.h"
#include <functional>
#include <iostream>
#include <memory>

LumiscriptaApp::LumiscriptaApp()
    : m_file(nullptr), m_graphics(nullptr), m_window(nullptr), m_viewMode(ViewMode::Preview), m_running(false) {}

LumiscriptaApp::~LumiscriptaApp() {}

bool LumiscriptaApp::init() {
    if (!glfwInit()) {
        std::cerr << "glfwInit failed\n";
        return false;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    m_window = glfwCreateWindow(1280, 800, "Lumiscripta", NULL, NULL);
    if (!m_window) {
        std::cerr << "glfwCreateWindow failed\n";
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(m_window);
    glfwSwapInterval(1);

    m_graphics = std::make_unique<Graphics>();
    if (!m_graphics->init(m_window)) {
        std::cerr << "Graphics initialization failed\n";
        return false;
    }

    m_file = std::make_unique<File>();

    return true;
}

void LumiscriptaApp::run() {
    m_running = true;
    while (m_running && !glfwWindowShouldClose(m_window)) {
        glfwPollEvents();

        m_graphics->beginFrame();
        renderUI();
        m_graphics->endFrame();

        glfwSwapBuffers(m_window);
    }
}

void LumiscriptaApp::shutdown() {
    if (m_graphics) m_graphics->shutdown();
    if (m_window) {
        glfwDestroyWindow(m_window);
        m_window = nullptr;
    }
    glfwTerminate();
}

void LumiscriptaApp::toggleView() {
    m_viewMode = (m_viewMode == ViewMode::Preview) ? ViewMode::Editor : ViewMode::Preview;
}

void LumiscriptaApp::toggleTheme() {
    if (!m_graphics) return;
    m_graphics->applyTheme(
        (m_graphics->getTheme() == Theme::Light) ? Theme::Dark : Theme::Light
    );
}

bool LumiscriptaApp::loadFile(const string& path) {
    if (!m_file) m_file = std::make_unique<File>();
    bool ok = m_file->load(path);
    if (!ok) return false;
    m_viewMode = ViewMode::Preview;
    return true;
}

bool LumiscriptaApp::saveFile(const string& path) {
    if (!m_file) return false;
    return m_file->save(path);
}

ViewMode LumiscriptaApp::getViewMode() const {
    return m_viewMode;
}

void LumiscriptaApp::processInput() {
    // Global shortcuts can be added here later.
}

// ---------------------------------------------------------------------------
// Top bar — clean, minimal, no dropdown menus.
// ---------------------------------------------------------------------------
void LumiscriptaApp::renderMenuBar() {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    float barHeight = 40.0f;

    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, barHeight));
    ImGui::Begin("TopBar", nullptr,
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus |
        ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoSavedSettings);

    // Draw the bar background manually so it blends perfectly with the theme.
    ImDrawList* draw = ImGui::GetForegroundDrawList();
    ImVec2 p0 = viewport->WorkPos;
    ImVec2 p1 = ImVec2(p0.x + viewport->WorkSize.x, p0.y + barHeight);

    ImU32 bgCol = ImGui::GetColorU32(ImGuiCol_WindowBg);
    ImU32 borderCol = ImGui::GetColorU32(ImGuiCol_Border);

    draw->AddRectFilled(p0, p1, bgCol);
    draw->AddLine(ImVec2(p0.x, p1.y), ImVec2(p1.x, p1.y), borderCol, 1.0f);

    // Position cursor inside the bar.
    ImGui::SetCursorScreenPos(ImVec2(p0.x + 20, p0.y + 8));
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_Text));
    ImGui::TextUnformatted("Lumiscripta");
    ImGui::PopStyleColor();

    // Center: Editor / Preview toggle.
    float toggleWidth = 152;
    float centerX = p0.x + (viewport->WorkSize.x - toggleWidth) * 0.5f;
    ImGui::SetCursorScreenPos(ImVec2(centerX, p0.y + 6));

    ImVec2 btnSize(72, 28);
    ImVec4 textCol = ImGui::GetStyleColorVec4(ImGuiCol_Text);
    ImVec4 bgColVec = ImGui::GetStyleColorVec4(ImGuiCol_WindowBg);
    ImVec4 surfaceCol = ImGui::GetStyleColorVec4(ImGuiCol_FrameBg);

    auto drawPillButton = [&](const char* label, bool active, std::function<void()> onClick) {
        ImVec4 btnBg = active ? textCol : ImVec4(0, 0, 0, 0);
        ImVec4 btnText = active ? bgColVec : textCol;

        ImGui::PushStyleColor(ImGuiCol_Button, btnBg);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, active ? textCol : surfaceCol);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, active ? textCol : surfaceCol);
        ImGui::PushStyleColor(ImGuiCol_Text, btnText);

        if (ImGui::Button(label, btnSize) && !active) onClick();

        ImGui::PopStyleColor(4);
    };

    drawPillButton("Editor", m_viewMode == ViewMode::Editor, [&]() { toggleView(); });
    ImGui::SameLine();
    drawPillButton("Preview", m_viewMode == ViewMode::Preview, [&]() { toggleView(); });

    // Right: theme icon button.
    float iconBtnSize = 28;
    float rightX = p1.x - iconBtnSize - 20;
    ImGui::SetCursorScreenPos(ImVec2(rightX, p0.y + 6));

    const char* icon = (m_graphics && m_graphics->getTheme() == Theme::Light)
        ? ICON_FA_SUN : ICON_FA_MOON;

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, surfaceCol);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, surfaceCol);
    if (ImGui::Button(icon, ImVec2(iconBtnSize, iconBtnSize))) toggleTheme();
    ImGui::PopStyleColor(3);

    // Reserve the bar height so WorkPos/WorkSize exclude it next frame.
    ImGui::SetCursorScreenPos(ImVec2(p0.x, p1.y));
    ImGui::Dummy(ImVec2(1.0f, 1.0f));
    ImGui::End();
}

// ---------------------------------------------------------------------------
// Main content area — uses the full viewport below the top bar.
// ---------------------------------------------------------------------------
void LumiscriptaApp::renderUI() {
    renderMenuBar();

    const ImGuiViewport* viewport = ImGui::GetMainViewport();

    // The content window fills the remaining work area.
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(28, 24));
    ImGui::Begin("Content", nullptr,
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus |
        ImGuiWindowFlags_NoScrollbar);

    if (m_viewMode == ViewMode::Preview) {
        const string& content = m_file ? m_file->getContent() : string();
        m_graphics->renderPreview(content);
    } else {
        if (m_file) {
            string editable = m_file->getContent();
            m_graphics->renderEditor(editable);
        } else {
            string empty;
            m_graphics->renderEditor(empty);
        }
    }

    ImGui::End();
    ImGui::PopStyleVar();
}