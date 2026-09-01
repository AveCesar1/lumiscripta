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
struct ImGuiContext;

enum class Theme {
    Light,
    Dark
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