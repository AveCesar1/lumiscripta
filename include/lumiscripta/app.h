#ifndef APP_H
#define APP_H

#include <memory>
#include <string>

using std::string;
using std::unique_ptr; // This one's harder. A 'new' type of pointer that automatically deletes the object it points to when it goes out of scope.

struct GLFWwindow;

// We again forward-declare classes here. In this case, of our own classes. Read graphics.h
class File;
class Graphics;

enum class ViewMode {
    Editor,
    Preview,
    Welcome
};

// A hyperlink target that still needs the user's answer: web addresses and other
// markdown documents are confirmed before anything happens. Any other local file
// needs no confirmation — it is revealed in the OS file manager on the spot — so
// it never reaches this state.
enum class LinkTarget {
    None,
    Web,
    Document
};

class LumiscriptaApp {
public:
    LumiscriptaApp();
    ~LumiscriptaApp();

    // Create window, init graphics, setup ImGui.
    bool init();

    // Main loop. Blocks until window closes.
    void run();

    // Destroy window, shutdown graphics.
    void shutdown();

    // Togglers
    void toggleView();
    void toggleTheme();

    // Open a file from disk.
    bool loadFile(const string& path);

    // Save the current file to disk.
    bool saveFile(const string& path);

    // Current view mode.
    ViewMode getViewMode() const;

private:
    unique_ptr<File> m_file;
    unique_ptr<Graphics> m_graphics;
    GLFWwindow* m_window;
    ViewMode m_viewMode;
    bool m_running;

    // Hyperlink clicked in the preview (see processLinkRequests/renderLinkDialog).
    LinkTarget m_linkTarget;
    string m_linkHref;   // target exactly as written in the document
    string m_linkPath;   // resolved path, for Document targets

    void processInput();
    void renderUI();
    void renderMenuBar();
    void renderWelcome();
    void enterMainUI(ViewMode mode);
    void processLinkRequests();
    void renderLinkDialog();
    void performLinkAction();
};

#endif /* APP_H */