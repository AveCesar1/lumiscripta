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
        MarkdownRenderer()
                : m_code_block(false), m_code_draw_list(nullptr), m_code_start(0.0f, 0.0f),
                    m_code_width(0.0f), m_table_width(0.0f), m_table_start(0.0f) {}

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
            const float avail = ImGui::GetContentRegionAvail().x;
            const float target = std::min(avail * 0.9f, 900.0f);
            const float x = ImGui::GetCursorScreenPos().x;
            ImDrawList* draw = ImGui::GetWindowDrawList();
            draw->ChannelsSplit(2);
            draw->ChannelsSetCurrent(1);
            m_code_draw_list = draw;
            m_code_start = ImVec2(x, ImGui::GetCursorScreenPos().y);
            m_code_width = target;
            ImGui::PushFont(g_font_mono);
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, 4.0f));
            ImGui::SetCursorScreenPos(ImVec2(x, m_code_start.y + 10.0f));
            m_code_block = true;
        } else {
            if (m_code_block) {
                const float bottom = ImGui::GetCursorScreenPos().y + 6.0f;
                m_code_draw_list->ChannelsSetCurrent(0);
                m_code_draw_list->AddRectFilled(
                    m_code_start,
                    ImVec2(m_code_start.x + m_code_width, bottom),
                        ImGui::GetColorU32(ImVec4(
                            ImGui::GetStyle().Colors[ImGuiCol_FrameBg].x * 0.55f +
                                ImGui::GetStyle().Colors[ImGuiCol_WindowBg].x * 0.45f,
                            ImGui::GetStyle().Colors[ImGuiCol_FrameBg].y * 0.55f +
                                ImGui::GetStyle().Colors[ImGuiCol_WindowBg].y * 0.45f,
                            ImGui::GetStyle().Colors[ImGuiCol_FrameBg].z * 0.55f +
                                ImGui::GetStyle().Colors[ImGuiCol_WindowBg].z * 0.45f,
                            1.0f)),
                    8.0f);
                m_code_draw_list->ChannelsMerge();
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
            imgui_md::BLOCK_TABLE(d, e);
            m_table_col_pos.clear();
            if (d->col_count > 0) {
                const float column_width = m_table_width / static_cast<float>(d->col_count);
                for (unsigned i = 0; i < d->col_count; ++i) {
                    m_table_col_pos.push_back(m_table_start + column_width * i);
                }
            }
            m_table_last_pos.x = m_table_start + m_table_width;
            ImGui::SetCursorPosX(m_table_start);
            return;
        }
        imgui_md::BLOCK_TABLE(d, e);
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
    ImDrawList* m_code_draw_list;
    ImVec2 m_code_start;
    float m_code_width;
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