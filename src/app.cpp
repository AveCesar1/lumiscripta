#include "lumiscripta/app.h"
#include "lumiscripta/file.h"
#include "lumiscripta/graphics.h"
#include "lumiscripta/utils.h"   // parentDirectory() for resolving relative image paths

#include <GLFW/glfw3.h>
#include "imgui.h"
#include "imgui/backends/imgui_impl_glfw.h"
#include "imgui/backends/imgui_impl_opengl3.h"
#include "IconsFontAwesome/IconsFontAwesome7.h"
// Declarations only: the STB_IMAGE_IMPLEMENTATION half lives in graphics.cpp.
#include "stb/stb_image.h"
#include <functional>
#include <cstdio>
#include <cstdlib>   // std::system, for the browser and file-manager launchers
#include <cfloat>    // FLT_MAX, in the dialog's text measuring helper
#include <cstring>   // std::strlen
#include <iostream>
#include <memory>

// Draw the branding wordmark at the current cursor, scaled to 'targetHeight'
// with its aspect ratio preserved (the art's pixel size never matters). Passing
// 'availableWidth' > 0 centres it horizontally in that width.
static bool drawWordmark(Graphics* graphics, Theme theme, float targetHeight, float availableWidth) {
    if (graphics == nullptr || targetHeight <= 0.0f) return false;

    ImTextureID texture = ImTextureID_Invalid;
    ImVec2 natural(0.0f, 0.0f);
    if (!graphics->getWordmarkTexture(theme, texture, natural) || natural.y <= 0.0f) return false;

    const ImVec2 size(targetHeight * (natural.x / natural.y), targetHeight);
    if (availableWidth > 0.0f) {
        const float centre = (availableWidth - size.x) * 0.5f;
        if (centre > 0.0f) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + centre);
    }
    ImGui::Image(texture, size);
    return true;
}

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

// ---------------------------------------------------------------------------
// Launchers
//
// Opening a browser or a file manager is a job for the OS, and neither is worth
// a dependency: the file picker above shells out the same way. Everything that
// comes from a document is quoted before it reaches the shell.
// ---------------------------------------------------------------------------

#ifndef _WIN32
// Single-quote a value for /bin/sh. URIs and paths may contain spaces,
// apostrophes or other metacharacters; quoting keeps them arguments instead of
// shell syntax.
static string shellQuote(const string& value) {
    string quoted = "'";
    for (size_t i = 0; i < value.size(); ++i) {
        if (value[i] == '\'') quoted += "'\\''";   // close, escape, reopen
        else quoted += value[i];
    }
    return quoted + "'";
}
#endif

// Open a web address with the platform's default browser.
static void openInBrowser(const string& url) {
    if (url.empty()) return;
#ifdef _WIN32
    // 'start' takes the window title as its first argument; without the empty
    // pair a quoted URL would be read as that title instead of the target.
    const string command = "start \"\" \"" + url + "\"";
#elif defined(__APPLE__)
    const string command = "open " + shellQuote(url);
#else
    // xdg-open can stay in the foreground for as long as the browser lives, so
    // the command is detached from the app.
    const string command = "xdg-open " + shellQuote(url) + " >/dev/null 2>&1 &";
#endif
    if (std::system(command.c_str()) != 0) {
        std::cerr << "Could not open the link: " << url << "\n";
    }
}

// Show 'path' in the platform's file manager — revealed, never opened.
static void revealInFileManager(const string& path) {
    if (path.empty()) return;
#ifdef _WIN32
    // explorer reports a non-zero exit code even when it succeeds, so its status
    // is deliberately ignored.
    const string command = "explorer /select,\"" + path + "\"";
    std::system(command.c_str());
#elif defined(__APPLE__)
    const string command = "open -R " + shellQuote(path);
    if (std::system(command.c_str()) != 0) {
        std::cerr << "Could not reveal the file: " << path << "\n";
    }
#else
    // Prefer a file manager that can select the file; fall back to opening the
    // containing folder. Detached for the same reason as above.
    const string command =
        "( dolphin --select " + shellQuote(path) +
        " || nautilus --select " + shellQuote(path) +
        " || xdg-open " + shellQuote(parentDirectory(path)) +
        " ) >/dev/null 2>&1 &";
    std::system(command.c_str());
#endif
}

LumiscriptaApp::LumiscriptaApp()
        : m_file(nullptr), m_graphics(nullptr), m_window(nullptr), m_viewMode(ViewMode::Welcome),
            m_running(false), m_linkTarget(LinkTarget::None) {}

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

    // Window (taskbar) icon. GLFW applies it on Windows and X11; macOS and
    // Wayland ignore the call and take the icon from the executable's bundle or
    // desktop entry instead (installed by `make install` / `make macos-bundle`).
    {
        const string iconPath = resolveAsset("branding/logo-light.png");
        int width = 0;
        int height = 0;
        int channels = 0;
        unsigned char* pixels = iconPath.empty()
            ? nullptr
            : stbi_load(iconPath.c_str(), &width, &height, &channels, 4);
        if (pixels && width > 0 && height > 0) {
            GLFWimage icon;
            icon.width = width;
            icon.height = height;
            icon.pixels = pixels;
            glfwSetWindowIcon(m_window, 1, &icon);
        }
        if (pixels) stbi_image_free(pixels);
    }

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
    // While the link dialog is up, the keyboard belongs to it: Enter accepts and
    // the arrow keys move between the buttons.
    if (m_linkTarget != LinkTarget::None) return;

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

    // Wordmark instead of the app name. It is scaled to the height of the
    // "Open" button next to it and shares the same line, so it is centred
    // vertically in the bar whatever the art's pixel size is. Which of the two
    // wordmarks is used follows the theme (dark art on the dark palette).
    const float barBtnHeight = 28.0f;
    ImGui::SetCursorScreenPos(ImVec2(p0.x + 20, p0.y + 6));
    const Theme currentTheme = m_graphics ? m_graphics->getTheme() : Theme::Light;
    if (!drawWordmark(m_graphics.get(), currentTheme, barBtnHeight, 0.0f)) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetColorU32(ImGuiCol_Text));
        ImGui::TextUnformatted("Lumiscripta");   // fallback: art not installed
        ImGui::PopStyleColor();
    }

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

    // Wordmark instead of the app name: scaled to the window, centred, and
    // theme-aware like the top bar's copy.
    float avail = ImGui::GetContentRegionAvail().x;
    const Theme welcomeTheme = m_graphics ? m_graphics->getTheme() : Theme::Light;
    if (!drawWordmark(m_graphics.get(), welcomeTheme, viewport->WorkSize.y * 0.12f, avail)) {
        if (g_font_bold_large) ImGui::PushFont(g_font_bold_large);
        ImGui::TextColored(ImVec4(0.28f, 0.22f, 0.16f, 1.0f), "Lumiscripta");
        if (g_font_bold_large) ImGui::PopFont();
    }

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

    // A hyperlink click from this frame becomes a dialog (or a file-manager
    // reveal) here, and the dialog is drawn if one is waiting.
    processLinkRequests();
    renderLinkDialog();
}

// ---------------------------------------------------------------------------
// Hyperlinks clicked in the preview
//
// imgui_md reports a click through MarkdownRenderer::open_url(), which queues the
// target on Graphics: the click happens inside a render pass and the answer to it
// is a dialog, so it cannot be dealt with right there. On the next frame the app
// decides what the target is —
//   - a web address         -> confirm, then the default browser
//   - a local markdown file -> confirm, then it replaces the open document
//   - any other local file  -> revealed in the OS file manager, no question asked
// ---------------------------------------------------------------------------

static bool looksLikeWebAddress(const string& href) {
    return href.find("://") != string::npos;   // http, https, ftp, ...
}

static bool isMarkdownFile(const string& path) {
    const string extension = fileExtension(path);
    return extension == "md" || extension == "markdown";
}

// '#' starts a fragment ("notes.md#setup"). The app does not scroll to anchors,
// so only the part before it addresses a file.
static string stripFragment(const string& href) {
    const size_t hash = href.find('#');
    return (hash == string::npos) ? href : href.substr(0, hash);
}

// Absolute paths are used as they are, everything else is relative to the open
// document — the same rule the preview images follow.
static string resolveLinkPath(const string& documentPath, const string& href) {
    const string target = stripFragment(href);
    if (target.empty()) return string();
    if (isAbsolutePath(target)) return target;
    return joinPath(parentDirectory(documentPath), target);
}

void LumiscriptaApp::processLinkRequests() {
    string href;
    if (!m_graphics || !m_graphics->takePendingLink(href)) return;

    if (looksLikeWebAddress(href)) {
        m_linkTarget = LinkTarget::Web;
        m_linkHref = href;
        m_linkPath.clear();
        return;
    }

    const string path = resolveLinkPath(m_file ? m_file->getPath() : string(), href);
    if (path.empty() || !pathExists(path)) {
        // A scheme the app cannot act on (mailto:, tel:, ...) or a path that is
        // not on disk: nothing to open, nothing to confirm.
        std::cerr << "Link target not found: " << href << "\n";
        return;
    }

    if (isMarkdownFile(path)) {
        m_linkTarget = LinkTarget::Document;
        m_linkHref = href;
        m_linkPath = path;
        return;
    }

    // Some other local file: show it in the file manager, never open it.
    revealInFileManager(path);
}

void LumiscriptaApp::performLinkAction() {
    switch (m_linkTarget) {
        case LinkTarget::Web:
            openInBrowser(m_linkHref);
            break;
        case LinkTarget::Document:
            // Exactly what picking this path in the Open dialog does.
            loadFile(m_linkPath);
            break;
        default:
            break;
    }

    m_linkTarget = LinkTarget::None;
    m_linkHref.clear();
    m_linkPath.clear();
}

// ---------------------------------------------------------------------------
// Dialog layout helper
//
// Two constraints shape this: the app's windows carry zero padding (the global
// style leaves every inset to the individual window), and ImGui cannot both wrap
// and centre text — TextUnformatted never wraps and TextWrapped only left-aligns.
// So each line is measured with the font's own word-wrap rule and drawn at the
// requested alignment. Note that ItemSize() adds Style.ItemSpacing.y to every
// cursor advance; the dialog pushes that spacing to zero and spaces its blocks
// with explicit Dummy() calls, so every vertical gap stays a deliberate number.
// ---------------------------------------------------------------------------

// Word-wrap 'text' to 'wrap_width' pixels at 'font' and return the height it
// occupies. With 'draw' set, the lines are painted from 'left_x' — centred inside
// the wrap width when 'centre' is set — and the cursor is advanced past them.
static float layoutWrappedText(ImFont* font, const char* text, const ImVec4& color,
                               float left_x, float wrap_width, bool centre, bool draw) {
    if (font == nullptr) font = ImGui::GetFont();
    if (text == nullptr || *text == '\0' || wrap_width <= 0.0f) return 0.0f;

    // The size comes from the pushed font: ImFont has no FontSize member in 1.92+,
    // and GetFontSize() is the value that already carries the user's zoom.
    ImGui::PushFont(font);
    const float size = ImGui::GetFontSize();
    const float line_height = size * 1.35f;
    const char* const text_end = text + std::strlen(text);
    ImDrawList* draw_list = draw ? ImGui::GetWindowDrawList() : nullptr;
    const ImU32 col = draw ? ImGui::GetColorU32(color) : 0u;

    float height = 0.0f;
    const char* line = text;
    while (line < text_end) {
        const char* line_end = font->CalcWordWrapPosition(size, line, text_end, wrap_width);
        if (line_end <= line) line_end = line + 1;

        // A token wider than the wrap width (a path without spaces) would spill
        // over the margin: trim it back until it fits.
        while (line_end - line > 1 &&
               font->CalcTextSizeA(size, FLT_MAX, 0.0f, line, line_end).x > wrap_width) {
            --line_end;
        }

        if (draw) {
            const float line_w = font->CalcTextSizeA(size, FLT_MAX, 0.0f, line, line_end).x;
            const float x = centre ? left_x + (wrap_width - line_w) * 0.5f : left_x;
            draw_list->AddText(font, size, ImVec2(x, ImGui::GetCursorScreenPos().y),
                               col, line, line_end);
            ImGui::Dummy(ImVec2(0.0f, line_height));
        }
        height += line_height;

        line = line_end;
        while (line < text_end && (*line == ' ' || *line == '\n')) ++line;
    }
    ImGui::PopFont();
    return height;
}

void LumiscriptaApp::renderLinkDialog() {
    if (m_linkTarget == LinkTarget::None) return;

    // Centred in the viewport; a fixed width so the wording never reflows oddly,
    // and a fitted height so the block keeps one silhouette for every target.
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(460.0f, 0.0f), ImGuiCond_Appearing);

    const char* dialogId = "Open link";
    if (!ImGui::IsPopupOpen(dialogId)) ImGui::OpenPopup(dialogId);

    bool accepted = false;
    bool dismissed = false;

    // The dialog supplies both insets the global style delegates to each window:
    // margins from the border, and zero item spacing so every gap below is an
    // explicit number rather than a remainder (see layoutWrappedText).
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(34.0f, 24.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                        ImVec2(ImGui::GetStyle().ItemSpacing.x, 0.0f));

    // No stock title bar: its label is left-aligned and reads as an afterthought.
    // The question itself is the header — centred, bold, wrapped line by line.
    if (ImGui::BeginPopupModal(dialogId, nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings)) {

        const bool web = (m_linkTarget == LinkTarget::Web);
        const char* question = web
            ? "Open this link in your default browser?"
            : "Open this document and replace the current one?";
        const char* target = web ? m_linkHref.c_str() : m_linkPath.c_str();

        const ImGuiStyle& style = ImGui::GetStyle();
        const float content_x = ImGui::GetCursorScreenPos().x;
        const float content_w = ImGui::GetContentRegionAvail().x;

        // 1. Header — centred and bold.
        layoutWrappedText(g_font_bold, question, style.Colors[ImGuiCol_Text],
                          content_x, content_w, true, true);

        // 2. Address or path — an inset chip in the monospaced face so it reads as
        //    data rather than prose. Its edges line up with the rule below.
        if (target != nullptr && *target != '\0') {
            ImGui::Dummy(ImVec2(0.0f, 14.0f));
            const ImVec2 pad(12.0f, 9.0f);
            const float text_w = content_w - pad.x * 2.0f;
            const ImVec2 chip_pos = ImGui::GetCursorScreenPos();
            const float text_h = layoutWrappedText(g_font_mono, target,
                style.Colors[ImGuiCol_TextDisabled], chip_pos.x + pad.x, text_w, false, false);
            const ImVec2 chip_size(content_w, text_h + pad.y * 2.0f);

            ImGui::GetWindowDrawList()->AddRectFilled(chip_pos,
                ImVec2(chip_pos.x + chip_size.x, chip_pos.y + chip_size.y),
                ImGui::GetColorU32(ImGuiCol_FrameBg), style.FrameRounding);

            ImGui::SetCursorScreenPos(ImVec2(chip_pos.x + pad.x, chip_pos.y + pad.y));
            layoutWrappedText(g_font_mono, target, style.Colors[ImGuiCol_TextDisabled],
                              chip_pos.x + pad.x, text_w, false, true);
            ImGui::SetCursorScreenPos(ImVec2(chip_pos.x + chip_size.x, chip_pos.y + chip_size.y));
        }

        // 3. Rule — inset like everything else (Separator spans the padded width).
        ImGui::Dummy(ImVec2(0.0f, 16.0f));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0.0f, 14.0f));

        // 4. Actions — the centred pair the welcome screen uses. Accept carries a
        //    hairline border (the top bar's active-segment treatment); Cancel is
        //    the ghost button the theme switch uses.
        const float button_width = 110.0f;
        const float button_gap = 12.0f;
        const float offset =
            (ImGui::GetContentRegionAvail().x - (button_width * 2.0f + button_gap)) * 0.5f;
        if (offset > 0.0f) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset);

        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
        if (ImGui::Button("Accept", ImVec2(button_width, 36.0f))) accepted = true;
        ImGui::PopStyleVar();
        // Accept is the default item; that is what lets a bare Enter confirm.
        ImGui::SetItemDefaultFocus();

        ImGui::SameLine(0.0f, button_gap);
        // Ghost button, the treatment the top bar gives its theme switch.
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, style.Colors[ImGuiCol_FrameBg]);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, style.Colors[ImGuiCol_FrameBgActive]);
        if (ImGui::Button("Cancel", ImVec2(button_width, 36.0f))) dismissed = true;
        ImGui::PopStyleColor(3);

        // The arrow keys walk between the two buttons (keyboard navigation is on),
        // Enter confirms even after the focus moved, Escape dismisses.
        if (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter)) {
            accepted = true;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) dismissed = true;

        if (accepted || dismissed) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    } else {
        // Gone without an answer (Escape closed it): nothing was confirmed.
        dismissed = true;
    }

    ImGui::PopStyleVar(2);   // WindowPadding + ItemSpacing

    if (accepted) performLinkAction();
    else if (dismissed) m_linkTarget = LinkTarget::None;
}
