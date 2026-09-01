#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <string>

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

enum class Theme {
    Light,
    Dark
};

class MarkdownRenderer : public imgui_md {
public:
    ImFont* get_font() const override {
        if (m_is_table_header) {
            return g_font_bold;
        }
        switch (m_hlevel) {
            case 0:  return m_is_strong ? g_font_bold : g_font_regular;
            case 1:  return g_font_bold_large;  // H1
            default: return g_font_bold;        // H2, H3...
        }
    }

    void open_url() const override {
        // Optional: open URL in the default browser. This is platform-dependent.
        // In macOS: system(("open " + m_href).c_str());
        // In Linux: system(("xdg-open " + m_href).c_str());
    }

    bool get_image(image_info& nfo) const override {
        // Optional: load images.
        return false;  // for now, we don't handle images.
    }
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

private:
    ImGuiContext* m_ctx;
    GLFWwindow* m_window;
    Theme m_theme;
    bool m_initialized;

    void setupStyleLight();
    void setupStyleDark();
};

#endif /* GRAPHICS_H */