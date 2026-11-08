#include "sysscope/app_state.hpp"

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#define GL_SILENCE_DEPRECATION
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3native.h>
#import <AppKit/AppKit.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <filesystem>
#include <string_view>

namespace {

void glfw_error_callback(int error, const char* description) {
    std::fprintf(stderr, "GLFW error %d: %s\n", error, description);
}

void style_ui() {
    ImGui::StyleColorsDark();
    auto& style = ImGui::GetStyle();
    style.WindowPadding = {0, 0};
    style.ChildRounding = 10;
    style.FrameRounding = 6;
    style.PopupRounding = 8;
    style.GrabRounding = 6;
    style.ScrollbarRounding = 6;
    style.ScrollbarSize = 10;
    style.FramePadding = {10, 7};
    style.ItemSpacing = {10, 10};
    style.CellPadding = {10, 8};

    auto* color = style.Colors;
    color[ImGuiCol_Text] = {0.91f, 0.94f, 0.97f, 1};
    color[ImGuiCol_TextDisabled] = {0.58f, 0.64f, 0.72f, 1};
    color[ImGuiCol_WindowBg] = {0.052f, 0.068f, 0.094f, 1};
    color[ImGuiCol_ChildBg] = {0.073f, 0.093f, 0.126f, 1};
    color[ImGuiCol_Border] = {0.15f, 0.19f, 0.24f, 1};
    color[ImGuiCol_Separator] = color[ImGuiCol_Border];
    color[ImGuiCol_FrameBg] = {0.09f, 0.12f, 0.16f, 1};
    color[ImGuiCol_Header] = {0.12f, 0.23f, 0.28f, 1};
    color[ImGuiCol_HeaderHovered] = {0.12f, 0.19f, 0.24f, 1};
    color[ImGuiCol_HeaderActive] = {0.15f, 0.29f, 0.33f, 1};
    color[ImGuiCol_TableHeaderBg] = {0.096f, 0.123f, 0.163f, 1};
    color[ImGuiCol_TableBorderLight] = {0.14f, 0.18f, 0.23f, 1};
    color[ImGuiCol_TableBorderStrong] = color[ImGuiCol_Border];
    color[ImGuiCol_TableRowBgAlt] = {1, 1, 1, 0.022f};
    color[ImGuiCol_NavCursor] = {0.32f, 0.87f, 0.80f, 1};
}

ImFont* load_font(float size, float density) {
    ImFontConfig config;
    config.RasterizerDensity = density;
    config.SizePixels = size;
    const char* system_font = "/System/Library/Fonts/SFNS.ttf";
    auto* atlas = ImGui::GetIO().Fonts;
    if (std::filesystem::exists(system_font)) {
        if (auto* font = atlas->AddFontFromFileTTF(system_font, size, &config)) return font;
    }
    return atlas->AddFontDefault(&config);
}

GLuint load_logo() {
    NSString* path = [NSBundle.mainBundle pathForResource:@"marrow-logo" ofType:@"png"];
    if (!path) return 0;
    NSImage* image = [[NSImage alloc] initWithContentsOfFile:path];
    CGImageRef source = [image CGImageForProposedRect:nullptr context:nil hints:nil];
    if (!source) {
        [image release];
        return 0;
    }

    constexpr int size = 128;
    std::array<unsigned char, size * size * 4> pixels{};
    CGColorSpaceRef color_space = CGColorSpaceCreateDeviceRGB();
    CGContextRef context = CGBitmapContextCreate(pixels.data(), size, size, 8, size * 4, color_space,
                                                kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big);
    CGColorSpaceRelease(color_space);
    if (!context) {
        [image release];
        return 0;
    }
    CGContextDrawImage(context, CGRectMake(0, 0, size, size), source);
    CGContextRelease(context);
    [image release];

    // ImGui's renderer blends textures using straight alpha.
    for (size_t i = 0; i < pixels.size(); i += 4) {
        const unsigned int alpha = pixels[i + 3];
        if (alpha == 0 || alpha == 255) continue;
        for (size_t channel = 0; channel < 3; ++channel) {
            pixels[i + channel] = static_cast<unsigned char>(
                std::min(255u, pixels[i + channel] * 255u / alpha));
        }
    }
    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, size, size, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    glBindTexture(GL_TEXTURE_2D, 0);
    return texture;
}

}  // namespace

int main(int argc, char** argv) {
    bool demo = false;
    if (argc == 2 && std::string_view(argv[1]) == "--demo") demo = true;
    else if (argc != 1) {
        std::fprintf(stderr, "Usage: Marrow [--demo]\n");
        return 2;
    }

    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) return 1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);

    GLFWwindow* window = glfwCreateWindow(1180, 760, "Marrow", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return 1;
    }
    NSWindow* native_window = glfwGetCocoaWindow(window);
    native_window.appearance = [NSAppearance appearanceNamed:NSAppearanceNameDarkAqua];
    glfwSetWindowSizeLimits(window, 900, 640, GLFW_DONT_CARE, GLFW_DONT_CARE);
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    float scale_x, scale_y;
    glfwGetWindowContentScale(window, &scale_x, &scale_y);
    const float density = std::max(scale_x, scale_y);
    io.FontDefault = load_font(15, density);
    auto* heading_font = load_font(31, density);
    auto* value_font = load_font(24, density);
    style_ui();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 150");

    GLuint logo_texture = 0;
    @autoreleasepool {
        logo_texture = load_logo();
    }
    sysscope::AppState app(demo, heading_font, value_font, logo_texture);
    double last = glfwGetTime();
    while (!glfwWindowShouldClose(window)) {
        // Wake for input or a redraw without spinning at the display's maximum refresh rate.
        glfwWaitEventsTimeout(1.0 / 30.0);
        const double now = glfwGetTime();
        app.tick(now - last);
        last = now;

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos({0, 0});
        ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::Begin("Marrow dashboard", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                         ImGuiWindowFlags_NoSavedSettings);
        app.draw();
        ImGui::End();

        ImGui::Render();
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        glViewport(0, 0, width, height);
        glClearColor(0.052f, 0.068f, 0.094f, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    glDeleteTextures(1, &logo_texture);
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
