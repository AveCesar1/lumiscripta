#include "lumiscripta/app.h"
#include "lumiscripta/file.h"
#include "lumiscripta/graphics.h"
#include "lumiscripta/utils.h"   // parentDirectory() for resolving relative image paths

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
        : m_file(nullptr), m_graphics(nullptr), m_window(nullptr), m_viewMode(ViewMode::Welcome),
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

    m_window = glfwCreateWindow(440, 300, "Lumiscripta", NULL, NULL);
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
    if (m_viewMode == ViewMode::Welcome) {
        enterMainUI(ViewMode::Editor);
        return;
    }
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
    if (m_graphics) {
        // A new document brings new image references: release the textures that
        // belonged to the previous one, and resolve relative paths against this
        // file's directory from now on.
        m_graphics->clearImageCache();
        m_graphics->setBaseDirectory(parentDirectory(path));
    }
    // Loading a file (from the file picker or the command line) leaves the
    // welcome screen behind and opens the main interface in preview mode.
    enterMainUI(ViewMode::Preview);
    return true;
}

void LumiscriptaApp::enterMainUI(ViewMode mode) {
    m_viewMode = mode;
    if (m_window) {
        // The welcome window is small; grow back to the full editor size.
        glfwSetWindowSize(m_window, 1280, 800);
    }
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
    bool ctrlOrCmd = io.KeyCtrl || io.KeySuper;

    // Ctrl/Cmd + O -> open file
    if (ctrlOrCmd && ImGui::IsKeyPressed(ImGuiKey_O)) {
        const string path = chooseFilePath();
        if (!path.empty()) loadFile(path);
    }

    // Ctrl/Cmd + E -> toggle editor / preview
    if (ctrlOrCmd && ImGui::IsKeyPressed(ImGuiKey_E) && m_viewMode != ViewMode::Welcome) {
        toggleView();
    }

    // Ctrl/Cmd + Shift + T -> toggle light / dark theme
    if (ctrlOrCmd && io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_T)) {
        toggleTheme();
    }

    // Font size controls: '+' increases, '-' decreases (physical keys, cross-platform)
    if (ctrlOrCmd) {
        if (ImGui::IsKeyPressed(ImGuiKey_Equal) || ImGui::IsKeyPressed(ImGuiKey_KeypadAdd)) {
            io.FontGlobalScale = std::min(io.FontGlobalScale + 0.1f, 2.5f);
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Minus) || ImGui::IsKeyPressed(ImGuiKey_KeypadSubtract)) {
            io.FontGlobalScale = std::max(io.FontGlobalScale - 0.1f, 0.5f);
        }
    }

    // Ctrl/Cmd + S -> save (existing, preserved)
    if (ctrlOrCmd && ImGui::IsKeyPressed(ImGuiKey_S) && m_file) {
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
    ImVec4 surfaceCol = ImGui::GetStyleColorVec4(ImGuiCol_FrameBg);
    ImVec4 accentColor = ImGui::GetStyleColorVec4(ImGuiCol_Border);

    const bool codeActive = m_viewMode == ViewMode::Editor;
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, codeActive ? 1.0f : 0.0f);
    ImGui::PushStyleColor(ImGuiCol_Border, accentColor);
    ImGui::PushStyleColor(ImGuiCol_Button, codeActive ? surfaceCol : ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, surfaceCol);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, surfaceCol);
    ImGui::PushStyleColor(ImGuiCol_Text, textCol);
    if (ImGui::Button("Code", ImVec2(84, 28)) && m_viewMode != ViewMode::Editor) toggleView();
    ImGui::PopStyleColor(5);
    ImGui::PopStyleVar();

    ImGui::SameLine(0.0f, 8.0f);
    const bool previewActive = m_viewMode == ViewMode::Preview;
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, previewActive ? 1.0f : 0.0f);
    ImGui::PushStyleColor(ImGuiCol_Border, accentColor);
    ImGui::PushStyleColor(ImGuiCol_Button, previewActive ? surfaceCol : ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, surfaceCol);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, surfaceCol);
    ImGui::PushStyleColor(ImGuiCol_Text, textCol);
    if (ImGui::Button("Preview", ImVec2(84, 28)) && m_viewMode != ViewMode::Preview) toggleView();
    ImGui::PopStyleColor(5);
    ImGui::PopStyleVar();

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
// Welcome screen — a small centered window with no top bar, shown when the
// app starts without a file (and for as long as no document is open).
// ---------------------------------------------------------------------------
void LumiscriptaApp::renderWelcome() {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    // Fill the full viewport so resizing never leaves black gaps.
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(36, 36));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6, 12));
    ImGui::Begin("Welcome", nullptr,
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus |
        ImGuiWindowFlags_NoScrollbar);

    // Center horizontally: compute offset from content region.
    if (g_font_bold_large) ImGui::PushFont(g_font_bold_large);
    float avail = ImGui::GetContentRegionAvail().x;
    float textW = ImGui::CalcTextSize("Lumiscripta").x;
    float offset = (avail - textW) * 0.5f;
    if (offset > 0) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset);
    ImGui::TextColored(ImVec4(0.28f, 0.22f, 0.16f, 1.0f), "Lumiscripta");
    if (g_font_bold_large) ImGui::PopFont();

    ImGui::Spacing();
    float wTextW = ImGui::CalcTextSize("Welcome").x;
    float wOff = (avail - wTextW) * 0.5f;
    if (wOff > 0) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + wOff);
    ImGui::TextColored(ImVec4(0.55f, 0.42f, 0.30f, 1.0f), "Welcome");
    ImGui::Spacing();

    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, viewport->WorkSize.y * 0.07f));

    float btnW = avail * 0.35f;
    float totalBtnW = btnW * 2.0f + 8.0f;
    float btnOff = (avail - totalBtnW) * 0.5f;
    if (btnOff > 0) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + btnOff);

    if (ImGui::Button("Create file", ImVec2(btnW, 36.0f))) {
        if (!m_file) m_file = std::make_unique<File>();
        enterMainUI(ViewMode::Editor);
    }
    ImGui::SameLine(0.0f, 8.0f);
    if (ImGui::Button("Open file", ImVec2(btnW, 36.0f))) {
        const string path = chooseFilePath();
        if (!path.empty()) loadFile(path);
    }

    ImGui::End();
    ImGui::PopStyleVar(2);
}

// ---------------------------------------------------------------------------
// Main content area — uses the full viewport below the top bar.
// ---------------------------------------------------------------------------
void LumiscriptaApp::renderUI() {
    if (m_viewMode == ViewMode::Welcome) {
        renderWelcome();
        return;
    }

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float barHeight = 40.0f;

    // The content window fills the remaining work area.
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x, viewport->WorkPos.y + barHeight));
    ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, viewport->WorkSize.y - barHeight));

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
        m_viewMode == ViewMode::Editor ? ImVec2(0, 0) : ImVec2(28, 24));
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
