#include "sysscope/app_state.hpp"

#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_glfw.h"
#include "imgui/backends/imgui_impl_opengl3.h"

#define GL_SILENCE_DEPRECATION
#include <GLFW/glfw3.h>

#include <cstdio>
#include <vector>

static void glfw_error_callback(int error, const char* description) {
    std::fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

int main(int, char**) {
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) return 1;

    const char* glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

    GLFWwindow* window = glfwCreateWindow(1200, 800, "SysScope", nullptr, nullptr);
    if (!window) return 1;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    sysscope::AppState app;
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
        ImGui::Begin("SysScopeRoot", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
        if (ImGui::BeginTable("root_status", 3, ImGuiTableFlags_SizingStretchSame)) {
            ImGui::TableSetupColumn("helper");
            ImGui::TableSetupColumn("thermal");
            ImGui::TableSetupColumn("spark");
            ImGui::TableNextColumn();
            ImGui::TextColored(app.helper_connected ? ImVec4(0.35f, 0.90f, 0.60f, 1.0f)
                                                    : ImVec4(0.95f, 0.55f, 0.35f, 1.0f),
                               "Helper: %s", app.helper_connected ? app.helper_version.c_str() : "offline");
            ImGui::TextDisabled("Section: %s", sysscope::section_name(app.section));

            ImGui::TableNextColumn();
            ImGui::TextColored(app.snapshot.has_thermal ? ImVec4(0.45f, 0.85f, 1.0f, 1.0f)
                                                        : ImVec4(0.70f, 0.70f, 0.70f, 1.0f),
                               "%s", app.snapshot.has_thermal ? "Thermal grid ready" : "Thermal warming up");
            ImGui::TextDisabled("CPU samples: %zu", app.cpu_history.size());

            ImGui::TableNextColumn();
            if (!app.cpu_history.empty()) {
                std::vector<float> spark(app.cpu_history.begin(), app.cpu_history.end());
                ImGui::PlotLines("##cpu_spark", spark.data(), static_cast<int>(spark.size()), 0, nullptr, 0.f,
                                 100.f, ImVec2(-1, 26));
            } else {
                ImGui::TextDisabled("CPU sparkline waiting...");
            }

            ImGui::EndTable();
        }
        ImGui::Separator();
        app.draw();
        ImGui::End();

        ImGui::Render();
        int w, h;
        glfwGetFramebufferSize(window, &w, &h);
        glViewport(0, 0, w, h);
        glClearColor(0.05f, 0.05f, 0.06f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
