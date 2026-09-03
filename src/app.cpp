#include "lumiscripta/app.h"
#include "lumiscripta/file.h"
#include "lumiscripta/graphics.h"

#include <GLFW/glfw3.h>
#include "imgui.h"
#include "imgui/backends/imgui_impl_glfw.h"
#include "imgui/backends/imgui_impl_opengl3.h"
#include "IconsFontAwesome/IconsFontAwesome7.h"
#include <functional>
#include <cstdio>
#include <iostream>
#include <memory>

static string chooseFilePath() {
    const char* command = nullptr;
#ifdef __APPLE__
    command = "osascript -e 'POSIX path of (choose file with prompt \"Open Markdown File\")' 2>/dev/null";
#elif defined(_WIN32)
    command = "powershell -NoProfile -Command \"Add-Type -AssemblyName System.Windows.Forms; $d=New-Object System.Windows.Forms.OpenFileDialog; $d.Filter='Markdown files (*.md)|*.md|All files (*.*)|*.*'; if($d.ShowDialog() -eq 'OK'){ $d.FileName }\"";
#else
    command = "zenity --file-selection --title='Open Markdown File' --file-filter='Markdown files | *.md *.markdown' --file-filter='All files | *' 2>/dev/null || kdialog --getopenfilename . '*.md *.markdown' 2>/dev/null";
#endif

    FILE* pipe = popen(command, "r");
    if (!pipe) return string();

    string path;
    char buffer[512];
    while (fgets(buffer, sizeof(buffer), pipe)) path += buffer;
    pclose(pipe);

    while (!path.empty() && (path.back() == '\n' || path.back() == '\r')) path.pop_back();
    return path;
}

LumiscriptaApp::LumiscriptaApp()
        : m_file(nullptr), m_graphics(nullptr), m_window(nullptr), m_viewMode(ViewMode::Preview),
            m_running(false) {}

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
        processInput();
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
    ImGuiIO& io = ImGui::GetIO();
    if ((io.KeyCtrl || io.KeySuper) && ImGui::IsKeyPressed(ImGuiKey_S) && m_file) {
        if (!m_file->getPath().empty()) {
            saveFile(m_file->getPath());
        }
    }
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

    // Draw the bar background before its widgets.
    ImDrawList* draw = ImGui::GetWindowDrawList();
    ImVec2 p0 = viewport->WorkPos;
    ImVec2 p1 = ImVec2(p0.x + viewport->WorkSize.x, p0.y + barHeight);

    ImU32 bgCol = ImGui::GetColorU32(ImGuiCol_WindowBg);
    ImU32 borderCol = ImGui::GetColorU32(ImGuiCol_Border);

    draw->AddRectFilled(p0, p1, bgCol);
    draw->AddLine(ImVec2(p0.x, p1.y), ImVec2(p1.x, p1.y), borderCol, 1.0f);

    ImGui::SetCursorScreenPos(ImVec2(p0.x + 20, p0.y + 6));
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetColorU32(ImGuiCol_Text));
    ImGui::TextUnformatted("Lumiscripta");
    ImGui::PopStyleColor();

    ImGui::SameLine(0.0f, 24.0f);
    ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_FrameBg]);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyle().Colors[ImGuiCol_FrameBgHovered]);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::GetStyle().Colors[ImGuiCol_FrameBgActive]);
    if (ImGui::Button("Open", ImVec2(64, 28))) {
        const string path = chooseFilePath();
        if (!path.empty()) loadFile(path);
    }
    ImGui::PopStyleColor(3);

    float toggleWidth = 176.0f;
    float centerX = p0.x + (viewport->WorkSize.x - toggleWidth) * 0.5f;
    ImGui::SetCursorScreenPos(ImVec2(centerX, p0.y + 6));

    ImVec4 textCol = ImGui::GetStyleColorVec4(ImGuiCol_Text);
    ImVec4 bgColVec = ImGui::GetStyleColorVec4(ImGuiCol_WindowBg);
    ImVec4 surfaceCol = ImGui::GetStyleColorVec4(ImGuiCol_FrameBg);

    ImGui::PushStyleColor(ImGuiCol_Button, m_viewMode == ViewMode::Editor ? textCol : ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, surfaceCol);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, surfaceCol);
    ImGui::PushStyleColor(ImGuiCol_Text, m_viewMode == ViewMode::Editor ? bgColVec : textCol);
    if (ImGui::Button("Code", ImVec2(84, 28)) && m_viewMode != ViewMode::Editor) toggleView();
    ImGui::PopStyleColor(4);

    ImGui::SameLine(0.0f, 8.0f);
    ImGui::PushStyleColor(ImGuiCol_Button, m_viewMode == ViewMode::Preview ? textCol : ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, surfaceCol);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, surfaceCol);
    ImGui::PushStyleColor(ImGuiCol_Text, m_viewMode == ViewMode::Preview ? bgColVec : textCol);
    if (ImGui::Button("Preview", ImVec2(84, 28)) && m_viewMode != ViewMode::Preview) toggleView();
    ImGui::PopStyleColor(4);

    float iconBtnSize = 64.0f;
    float rightX = p1.x - iconBtnSize - 20;
    ImGui::SetCursorScreenPos(ImVec2(rightX, p0.y + 6));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, surfaceCol);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, surfaceCol);
    if (ImGui::Button(m_graphics && m_graphics->getTheme() == Theme::Light ? "Light" : "Dark", ImVec2(iconBtnSize, 28))) toggleTheme();
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
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float barHeight = 40.0f;

    // The content window fills the remaining work area.
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x, viewport->WorkPos.y + barHeight));
    ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, viewport->WorkSize.y - barHeight));

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
            m_file->setContent(editable);
        } else {
            string empty;
            m_graphics->renderEditor(empty);
        }
    }

    ImGui::End();
    ImGui::PopStyleVar();

    renderMenuBar();
}