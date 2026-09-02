#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <string>
#include <algorithm>

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

enum class Theme {
    Light,
    Dark
};

class MarkdownRenderer : public imgui_md {
public:
    MarkdownRenderer() : m_code_block(false), m_table_width(0.0f), m_table_start(0.0f) {}

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

    void BLOCK_CODE(const MD_BLOCK_CODE_DETAIL*, bool e) override {
        if (e) {
            m_is_code = true;
            ImGui::NewLine();
            ImGui::PushFont(g_font_mono);
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, 4.0f));
            ImGui::Indent(12.0f);
            m_code_block = true;
        } else {
            if (m_code_block) {
                ImGui::Unindent(12.0f);
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

    void BLOCK_TABLE(const MD_BLOCK_TABLE_DETAIL* d, bool e) override {
        if (e) {
            const float avail = ImGui::GetContentRegionAvail().x;
            m_table_width = std::min(avail * 0.9f, 900.0f);
            m_table_start = ImGui::GetCursorPosX() + (avail - m_table_width) * 0.5f;
            ImGui::SetCursorPosX(m_table_start);
        }
        imgui_md::BLOCK_TABLE(d, e);
    }

    void BLOCK_THEAD(bool e) override {
        imgui_md::BLOCK_THEAD(e);
        if (!e && m_table_width > 0.0f && m_table_col_pos.size() > 1) {
            const float natural_start = m_table_col_pos.front();
            const float natural_end = m_table_last_pos.x;
            const float natural_width = natural_end - natural_start;
            if (natural_width > 0.0f) {
                for (float& position : m_table_col_pos) {
                    position = m_table_start + (position - natural_start) * m_table_width / natural_width;
                }
                m_table_last_pos.x = m_table_start + m_table_width;
            }
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

private:
    bool m_code_block;
    float m_table_width;
    float m_table_start;
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