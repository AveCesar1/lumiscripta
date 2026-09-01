#include "lumiscripta/graphics.h"

#include "imgui.h"
#include "imgui/backends/imgui_impl_glfw.h"
#include "imgui/backends/imgui_impl_opengl3.h"
#include "imgui_md/imgui_md.h"
#include <GLFW/glfw3.h>
#include <iostream>

// Font handles used by MarkdownRenderer (declared in graphics.h usage).
ImFont* g_font_regular = nullptr;
ImFont* g_font_bold = nullptr;
ImFont* g_font_bold_large = nullptr;

Graphics::Graphics()
	: m_ctx(nullptr), m_window(nullptr), m_theme(Theme::Light), m_initialized(false) {}

Graphics::~Graphics() {
}

bool Graphics::init(GLFWwindow* window) {
	if (m_initialized) return true;
	if (!window) return false;
	m_window = window;

	IMGUI_CHECKVERSION();
	m_ctx = ImGui::CreateContext();
	ImGui::SetCurrentContext(m_ctx);
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

	ImGui::StyleColorsDark();

	if (!ImGui_ImplGlfw_InitForOpenGL(window, true)) {
		std::cerr << "ImGui_ImplGlfw_InitForOpenGL failed\n";
		return false;
	}

	const char* glsl_version = "#version 330";
	if (!ImGui_ImplOpenGL3_Init(glsl_version)) {
		std::cerr << "ImGui_ImplOpenGL3_Init failed\n";
		return false;
	}

	// Try to load Inter fonts from assets. If unavailable, fall back to default.
	{
		ImGuiIO& io2 = ImGui::GetIO();
		const char* regular_path = "assets/fonts/Inter-Regular.ttf";
		const char* bold_path = "assets/fonts/Inter-Bold.ttf";

		g_font_regular = io2.Fonts->AddFontFromFileTTF(regular_path, 16.0f);
		g_font_bold = io2.Fonts->AddFontFromFileTTF(bold_path, 16.0f);
		g_font_bold_large = io2.Fonts->AddFontFromFileTTF(bold_path, 24.0f);

		if (!g_font_regular || !g_font_bold || !g_font_bold_large) {
			// Fallback: ensure there is at least a default font
			if (!io2.Fonts->Fonts.empty()) {
				if (!g_font_regular) g_font_regular = io2.Fonts->Fonts[0];
				if (!g_font_bold) g_font_bold = io2.Fonts->Fonts[0];
				if (!g_font_bold_large) g_font_bold_large = io2.Fonts->Fonts[0];
			} else {
				g_font_regular = io2.Fonts->AddFontDefault();
				g_font_bold = g_font_regular;
				g_font_bold_large = g_font_regular;
			}
		}

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
	ImGui::BeginChild("editor_content", ImVec2(0,0), false);
	ImGui::TextUnformatted("Editor is not available in this MVP.");
	ImGui::EndChild();
}

void Graphics::renderPreview(const std::string& content) {
    ImGui::BeginChild("preview_content", ImVec2(0, 0), false, 
                      ImGuiWindowFlags_HorizontalScrollbar);
    
    if (!content.empty()) {
        static MarkdownRenderer s_renderer;
        s_renderer.print(content.c_str(), content.c_str() + content.size());
    } else {
        ImGui::TextWrapped("No file loaded. Use File -> Open or pass a path on the command line.");
    }
    
    ImGui::EndChild();
}

Theme Graphics::getTheme() const {
	return m_theme;
}

void Graphics::setupStyleLight() {
	ImGuiStyle& style = ImGui::GetStyle();
	style.WindowRounding = 0.0f;
	style.FrameRounding = 6.0f;
	style.ScrollbarSize = 4.0f;

	ImVec4 bg(250.0f/255.0f, 247.0f/255.0f, 242.0f/255.0f, 1.0f);
	ImVec4 surface(227.0f/255.0f, 221.0f/255.0f, 213.0f/255.0f, 1.0f);
	ImVec4 text(45.0f/255.0f, 42.0f/255.0f, 40.0f/255.0f, 1.0f);
	ImVec4 textSecondary(122.0f/255.0f, 111.0f/255.0f, 102.0f/255.0f, 1.0f);
	ImVec4 accent(184.0f/255.0f, 174.0f/255.0f, 164.0f/255.0f, 1.0f);

	style.Colors[ImGuiCol_Text] = text;
	style.Colors[ImGuiCol_TextDisabled] = textSecondary;
	style.Colors[ImGuiCol_WindowBg] = bg;
	style.Colors[ImGuiCol_FrameBg] = surface;
	style.Colors[ImGuiCol_FrameBgHovered] = accent;
	style.Colors[ImGuiCol_FrameBgActive] = accent;
	style.Colors[ImGuiCol_Border] = accent;
	style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0,0,0,0);
	style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(text.x, text.y, text.z, 0.12f);
	style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(text.x, text.y, text.z, 0.25f);
}

void Graphics::setupStyleDark() {
	ImGuiStyle& style = ImGui::GetStyle();
	style.WindowRounding = 0.0f;
	style.FrameRounding = 6.0f;
	style.ScrollbarSize = 4.0f;

	ImVec4 bg(27.0f/255.0f, 29.0f/255.0f, 32.0f/255.0f, 1.0f);
	ImVec4 surface(42.0f/255.0f, 45.0f/255.0f, 51.0f/255.0f, 1.0f);
	ImVec4 text(233.0f/255.0f, 237.0f/255.0f, 241.0f/255.0f, 1.0f);
	ImVec4 textSecondary(138.0f/255.0f, 145.0f/255.0f, 153.0f/255.0f, 1.0f);
	ImVec4 accent(74.0f/255.0f, 79.0f/255.0f, 86.0f/255.0f, 1.0f);

	style.Colors[ImGuiCol_Text] = text;
	style.Colors[ImGuiCol_TextDisabled] = textSecondary;
	style.Colors[ImGuiCol_WindowBg] = bg;
	style.Colors[ImGuiCol_FrameBg] = surface;
	style.Colors[ImGuiCol_FrameBgHovered] = accent;
	style.Colors[ImGuiCol_FrameBgActive] = accent;
	style.Colors[ImGuiCol_Border] = accent;
	style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0,0,0,0);
	style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(text.x, text.y, text.z, 0.12f);
	style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(text.x, text.y, text.z, 0.25f);
}