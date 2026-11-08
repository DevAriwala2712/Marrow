cmake_minimum_required(VERSION 3.24)

if(NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0" CACHE STRING "Minimum macOS version")
endif()
project(Marrow VERSION 0.1.0 LANGUAGES C CXX OBJC OBJCXX)
if(NOT APPLE)
    message(FATAL_ERROR "Marrow requires macOS.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OBJCXX_STANDARD 17)
set(CMAKE_OBJCXX_STANDARD_REQUIRED ON)

include(FetchContent)
set(BUILD_SHARED_LIBS OFF)
set(GLFW_BUILD_EXAMPLES OFF)
set(GLFW_BUILD_TESTS OFF)
set(GLFW_BUILD_DOCS OFF)
set(GLFW_INSTALL OFF)
set(JSON_Install OFF)
FetchContent_Declare(glfw
    URL https://codeload.github.com/glfw/glfw/tar.gz/refs/tags/3.4
    URL_HASH SHA256=c038d34200234d071fae9345bc455e4a8f2f544ab60150765d7704e08f3dac01
)
FetchContent_Declare(imgui
    URL https://codeload.github.com/ocornut/imgui/tar.gz/refs/tags/v1.91.9b
    URL_HASH SHA256=8e1bbc76c71d74fef2fb85db7e7ca8eba13d6a86623c54992b60162db554ffdb
)
FetchContent_Declare(nlohmann_json
    URL https://codeload.github.com/nlohmann/json/tar.gz/refs/tags/v3.12.0
    URL_HASH SHA256=4b92eb0c06d10683f7447ce9406cb97cd4b453be18d7279320f7b2f025c10187
)
FetchContent_MakeAvailable(glfw imgui nlohmann_json)

find_package(OpenGL REQUIRED)
find_package(SQLite3 REQUIRED)
if(TARGET SQLite3::SQLite3)
    set(MARROW_SQLITE_TARGET SQLite3::SQLite3)
else()
    set(MARROW_SQLITE_TARGET SQLite::SQLite3)
endif()
find_library(IOKIT IOKit REQUIRED)
find_library(FOUNDATION Foundation REQUIRED)
find_library(CORE_GRAPHICS CoreGraphics REQUIRED)

add_library(marrow_imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(marrow_imgui PUBLIC ${imgui_SOURCE_DIR})
target_link_libraries(marrow_imgui PUBLIC glfw OpenGL::GL)

add_library(marrow_core STATIC
    src/common/ipc.cpp
    src/common/metrics_codec.cpp
    src/common/stub_provider_factory.cpp
    src/storage/sqlite_ring_buffer.cpp
    src/helper/provider_factory.cpp
    src/helper/providers/cpu_provider.cpp
    src/helper/providers/memory_provider.cpp
    src/helper/providers/process_provider.cpp
    src/helper/providers/network_provider.cpp
    src/helper/providers/disk_provider.mm
    src/helper/providers/thermal_provider.mm
)
target_include_directories(marrow_core PUBLIC include)
target_compile_definitions(marrow_core PUBLIC MARROW_VERSION="${PROJECT_VERSION}")
target_compile_options(marrow_core PRIVATE -Wall -Wextra)
target_link_libraries(marrow_core
    PRIVATE nlohmann_json::nlohmann_json ${MARROW_SQLITE_TARGET} ${IOKIT} ${FOUNDATION}
)

set(MARROW_ARTWORK assets/Marrow.icns assets/marrow-logo.png)
set_source_files_properties(${MARROW_ARTWORK} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
add_executable(marrow MACOSX_BUNDLE src/app/main.mm src/app/app_state.cpp ${MARROW_ARTWORK})
set_target_properties(marrow PROPERTIES
    OUTPUT_NAME Marrow
    MACOSX_BUNDLE_GUI_IDENTIFIER io.github.devariwala2712.marrow
    MACOSX_BUNDLE_BUNDLE_NAME Marrow
    MACOSX_BUNDLE_ICON_FILE Marrow.icns
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
)
target_compile_options(marrow PRIVATE -Wall -Wextra)
target_link_libraries(marrow PRIVATE marrow_core marrow_imgui ${CORE_GRAPHICS})

add_executable(marrow-cli src/cli/main.cpp)
target_compile_options(marrow-cli PRIVATE -Wall -Wextra)
target_link_libraries(marrow-cli PRIVATE marrow_core)

add_executable(sysscope-helper EXCLUDE_FROM_ALL src/helper/main.cpp src/helper/helper_server.cpp)
target_link_libraries(sysscope-helper PRIVATE marrow_core)

include(CTest)
if(BUILD_TESTING)
    add_executable(marrow-tests Tests/test_main.cpp)
    target_link_libraries(marrow-tests PRIVATE marrow_core)
    add_test(NAME marrow-core COMMAND marrow-tests)
    add_test(NAME marrow-cli-version COMMAND marrow-cli --version)
    set_tests_properties(marrow-cli-version PROPERTIES PASS_REGULAR_EXPRESSION "Marrow ${PROJECT_VERSION}")
    add_test(NAME marrow-cli-json COMMAND marrow-cli)
    set_tests_properties(marrow-cli-json PROPERTIES PASS_REGULAR_EXPRESSION "cpu.*memory")
    add_test(NAME marrow-cli-prometheus COMMAND marrow-cli --format prometheus)
    set_tests_properties(marrow-cli-prometheus PROPERTIES PASS_REGULAR_EXPRESSION "sysscope_cpu_user_percent")
    add_test(NAME marrow-cli-invalid-format COMMAND marrow-cli --format yaml)
    set_tests_properties(marrow-cli-invalid-format PROPERTIES WILL_FAIL TRUE)
endif()
