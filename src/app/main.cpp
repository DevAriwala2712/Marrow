#include "marrow/app_state.hpp"

#include "ui_assets.hpp"

#include "imgui/backends/imgui_impl_glfw.h"
#include "imgui/backends/imgui_impl_opengl3.h"
#include "imgui/imgui.h"

#define GL_SILENCE_DEPRECATION
#include <GLFW/glfw3.h>

#include <array>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <vector>

namespace {

constexpr ImVec4 kBgRoot = ImVec4(13.0f / 255.0f, 13.0f / 255.0f, 13.0f / 255.0f, 1.0f);
constexpr ImVec4 kBgPanel = ImVec4(17.0f / 255.0f, 17.0f / 255.0f, 17.0f / 255.0f, 1.0f);
constexpr ImVec4 kBorderColor = ImVec4(34.0f / 255.0f, 34.0f / 255.0f, 34.0f / 255.0f, 1.0f);
constexpr ImVec4 kTextPrimary = ImVec4(224.0f / 255.0f, 224.0f / 255.0f, 224.0f / 255.0f, 1.0f);
constexpr ImVec4 kTextMuted = ImVec4(85.0f / 255.0f, 85.0f / 255.0f, 85.0f / 255.0f, 1.0f);
constexpr ImVec4 kAccentColor = ImVec4(0.0f, 200.0f / 255.0f, 1.0f, 1.0f);
constexpr ImVec4 kBadgeBg = ImVec4(26.0f / 255.0f, 26.0f / 255.0f, 26.0f / 255.0f, 1.0f);

struct UiResources {
    ImFont* body_font = nullptr;
    ImFont* value_font = nullptr;
    ImFont* label_font = nullptr;
    ImFont* header_font = nullptr;
};

void glfw_error_callback(int error, const char* description) {
    std::fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

void apply_theme() {
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowPadding = ImVec2(16.0f, 16.0f);
    style.FramePadding = ImVec2(8.0f, 6.0f);
    style.ItemSpacing = ImVec2(8.0f, 6.0f);
    style.WindowRounding = 0.0f;
    style.FrameRounding = 4.0f;
    style.ScrollbarRounding = 4.0f;

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg] = kBgRoot;
    colors[ImGuiCol_ChildBg] = kBgPanel;
    colors[ImGuiCol_PopupBg] = kBgPanel;
    colors[ImGuiCol_Border] = kBorderColor;
    colors[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);
    colors[ImGuiCol_Text] = kTextPrimary;
    colors[ImGuiCol_TextDisabled] = kTextMuted;
    colors[ImGuiCol_Header] = ImVec4(0, 0, 0, 0);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0, 0, 0, 0);
    colors[ImGuiCol_HeaderActive] = ImVec4(0, 0, 0, 0);
    colors[ImGuiCol_Button] = ImVec4(0, 0, 0, 0);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0, 0, 0, 0);
    colors[ImGuiCol_ButtonActive] = ImVec4(0, 0, 0, 0);
    colors[ImGuiCol_SliderGrab] = kAccentColor;
    colors[ImGuiCol_SliderGrabActive] = kAccentColor;
    colors[ImGuiCol_FrameBg] = ImVec4(26.0f / 255.0f, 26.0f / 255.0f, 26.0f / 255.0f, 1.0f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(32.0f / 255.0f, 32.0f / 255.0f, 32.0f / 255.0f, 1.0f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(35.0f / 255.0f, 35.0f / 255.0f, 35.0f / 255.0f, 1.0f);
    colors[ImGuiCol_CheckMark] = kAccentColor;
}

UiResources load_fonts(ImGuiIO& io) {
    const std::array<const char*, 5> candidates = {
        "../imgui/misc/fonts/JetBrainsMono-Regular.ttf",
        "../imgui/misc/fonts/IBMPlexMono-Regular.ttf",
        "imgui/misc/fonts/JetBrainsMono-Regular.ttf",
        "imgui/misc/fonts/IBMPlexMono-Regular.ttf",
        "../imgui/misc/fonts/Cousine-Regular.ttf",
    };

    UiResources fonts;
    for (const char* path : candidates) {
        if (!std::filesystem::exists(path)) continue;
        fonts.body_font = io.Fonts->AddFontFromFileTTF(path, 13.0f);
        if (fonts.body_font != nullptr) {
            fonts.value_font = io.Fonts->AddFontFromFileTTF(path, 22.0f);
            fonts.label_font = io.Fonts->AddFontFromFileTTF(path, 11.0f);
            fonts.header_font = io.Fonts->AddFontFromFileTTF(path, 16.0f);
            break;
        }
    }

    if (fonts.body_font == nullptr) {
        ImFontConfig body_cfg;
        body_cfg.SizePixels = 13.0f;
        fonts.body_font = io.Fonts->AddFontDefault(&body_cfg);
        ImFontConfig value_cfg;
        value_cfg.SizePixels = 22.0f;
        fonts.value_font = io.Fonts->AddFontDefault(&value_cfg);
        ImFontConfig label_cfg;
        label_cfg.SizePixels = 11.0f;
        fonts.label_font = io.Fonts->AddFontDefault(&label_cfg);
        ImFontConfig header_cfg;
        header_cfg.SizePixels = 16.0f;
        fonts.header_font = io.Fonts->AddFontDefault(&header_cfg);
    }

    io.FontDefault = fonts.body_font;
    return fonts;
}

GLuint load_logo_texture() {
    std::vector<unsigned char> rgba;
    int width = 0;
    int height = 0;
    if (!marrow::load_png_rgba("assets/logo.png", rgba, width, height)) return 0;
    if (width <= 0 || height <= 0 || rgba.empty()) return 0;

    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    return texture;
}

void draw_version_badge(const char* label, ImFont* font) {
    ImGui::PushFont(font != nullptr ? font : ImGui::GetFont());
    const ImVec2 text_size = ImGui::CalcTextSize(label);
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const ImVec2 pad(8.0f, 4.0f);
    const ImVec2 max(pos.x + text_size.x + pad.x * 2.0f, pos.y + text_size.y + pad.y * 2.0f);
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->AddRectFilled(pos, max, ImGui::GetColorU32(kBadgeBg), 6.0f);
    draw_list->AddRect(pos, max, ImGui::GetColorU32(kBorderColor), 6.0f, 0, 0.5f);
    draw_list->AddText(ImVec2(pos.x + pad.x, pos.y + pad.y), ImGui::GetColorU32(kTextMuted), label);
    ImGui::Dummy(ImVec2(max.x - pos.x, max.y - pos.y));
    ImGui::PopFont();
}

void draw_status_dot(const char* label, ImVec4 dot_color, ImFont* font) {
    const ImVec2 p = ImGui::GetCursorScreenPos();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->AddCircleFilled(ImVec2(p.x + 3.0f, p.y + 8.0f), 3.0f, ImGui::GetColorU32(dot_color));
    ImGui::Dummy(ImVec2(10.0f, 0.0f));
    ImGui::SameLine(0.0f, 4.0f);
    ImGui::PushFont(font != nullptr ? font : ImGui::GetFont());
    ImGui::TextColored(kTextMuted, "%s", label);
    ImGui::PopFont();
}

void draw_top_status_bar(const marrow::AppState& app, const UiResources& fonts) {
    if (!ImGui::BeginTable("top_status", 3, ImGuiTableFlags_SizingStretchProp)) return;
    ImGui::TableSetupColumn("left", ImGuiTableColumnFlags_WidthFixed, 160.0f);
    ImGui::TableSetupColumn("center", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("right", ImGuiTableColumnFlags_WidthFixed, 120.0f);

    ImGui::TableNextColumn();
    draw_version_badge("v0.1.0", fonts.label_font);

    ImGui::TableNextColumn();
    const ImVec4 green = ImVec4(0.0f, 1.0f, 136.0f / 255.0f, 1.0f);
    const ImVec4 amber = ImVec4(1.0f, 185.0f / 255.0f, 84.0f / 255.0f, 1.0f);
    draw_status_dot("Helper", app.helper_connected ? green : amber, fonts.label_font);
    ImGui::SameLine(0.0f, 16.0f);
    draw_status_dot("Thermal", app.snapshot.has_thermal ? green : amber, fonts.label_font);
    ImGui::SameLine(0.0f, 16.0f);
    draw_status_dot("CPU", app.snapshot.has_cpu ? green : amber, fonts.label_font);

    std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm* local = std::localtime(&now);
    char time_buf[16] = "--:--:--";
    if (local != nullptr) std::strftime(time_buf, sizeof(time_buf), "%H:%M:%S", local);

    ImGui::TableNextColumn();
    ImGui::PushFont(fonts.label_font != nullptr ? fonts.label_font : ImGui::GetFont());
    const float tw = ImGui::CalcTextSize(time_buf).x;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, ImGui::GetContentRegionAvail().x - tw));
    ImGui::TextColored(kTextMuted, "%s", time_buf);
    ImGui::PopFont();
    ImGui::EndTable();
}

}  // namespace

int main(int, char**) {
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) return 1;

    const char* glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

    GLFWwindow* window = glfwCreateWindow(1200, 800, "Marrow", nullptr, nullptr);
    if (!window) return 1;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    apply_theme();
    const UiResources fonts = load_fonts(io);

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    marrow::AppState app;
    app.set_fonts(fonts.body_font, fonts.value_font, fonts.label_font, fonts.header_font);
    const GLuint logo_texture = load_logo_texture();
    app.set_sidebar_logo(logo_texture != 0 ? static_cast<ImTextureID>(logo_texture) : static_cast<ImTextureID>(0));
    app.start();

    double last = glfwGetTime();
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        const double now = glfwGetTime();
        const double dt = now - last;
        last = now;
        app.tick(dt);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
        ImGui::Begin("MarrowRoot", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
        draw_top_status_bar(app, fonts);
        ImGui::Separator();
        app.draw();
        ImGui::End();

        ImGui::Render();
        int w, h;
        glfwGetFramebufferSize(window, &w, &h);
        glViewport(0, 0, w, h);
        glClearColor(kBgRoot.x, kBgRoot.y, kBgRoot.z, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    if (logo_texture != 0) glDeleteTextures(1, &logo_texture);
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
