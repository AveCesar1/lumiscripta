#include "lumiscripta/app.h"
#include "lumiscripta/file.h"
#include "lumiscripta/graphics.h"

#include <GLFW/glfw3.h>
#include "imgui.h"
#include "imgui/backends/imgui_impl_glfw.h"
#include "imgui/backends/imgui_impl_opengl3.h"
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
	if (m_viewMode == ViewMode::Preview) m_viewMode = ViewMode::Editor;
	else m_viewMode = ViewMode::Preview;
}

void LumiscriptaApp::toggleTheme() {
	if (!m_graphics) return;
	if (m_graphics->getTheme() == Theme::Light) m_graphics->applyTheme(Theme::Dark);
	else m_graphics->applyTheme(Theme::Light);
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
	// For MVP we handle no global input here. Kept for future expansion.
}

void LumiscriptaApp::renderMenuBar() {
	if (ImGui::BeginMainMenuBar()) {
		ImGui::TextUnformatted("Lumiscripta");
		if (ImGui::BeginMenu("File")) {
			if (ImGui::MenuItem("Open...")) {
				// For MVP we will not implement native dialogs. User can pass path via argv or use the console.
			}
			if (ImGui::MenuItem("Exit")) {
				glfwSetWindowShouldClose(m_window, GLFW_TRUE);
			}
			ImGui::EndMenu();
		}

		// Place theme toggle at the right side of the menu bar
		ImGui::SameLine();
		ImGui::SetCursorPosX(ImGui::GetIO().DisplaySize.x - 60);
		if (ImGui::Button(m_graphics && m_graphics->getTheme() == Theme::Light ? "☀︎" : "☾")) {
			toggleTheme();
		}

		ImGui::EndMainMenuBar();
	}
}

void LumiscriptaApp::renderUI() {
	renderMenuBar();

	ImGui::Begin("Content", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
	ImGui::SetWindowPos(ImVec2(0, 32));
	ImGui::SetWindowSize(ImGui::GetIO().DisplaySize);

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
}

