CXX ?= g++
CXXFLAGS = -std=c++17 -Wall -Wextra -Iinclude -Ithird_party/nlohmann -I.. -I../imgui -I../imgui/backends \
	$(shell pkg-config --cflags glfw3 2>/dev/null || echo -I/opt/homebrew/include)
LDFLAGS_COMMON = -lsqlite3 $(shell pkg-config --libs glfw3 2>/dev/null || echo -L/opt/homebrew/lib -lglfw) \
	-framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo
LDFLAGS_HELPER = -lsqlite3 -lproc -framework IOKit -framework CoreFoundation

IMGUI_DIR = ../imgui
IMGUI_SRCS = $(IMGUI_DIR)/imgui.cpp $(IMGUI_DIR)/imgui_draw.cpp $(IMGUI_DIR)/imgui_tables.cpp \
	$(IMGUI_DIR)/imgui_widgets.cpp $(IMGUI_DIR)/backends/imgui_impl_glfw.cpp \
	$(IMGUI_DIR)/backends/imgui_impl_opengl3.cpp

CORE_SRCS = src/storage/sqlite_ring_buffer.cpp src/common/ipc.cpp src/common/metrics_codec.cpp \
	src/common/stub_provider_factory.cpp
CORE_OBJS = $(CORE_SRCS:.cpp=.o)

HELPER_SRCS = src/helper/main.cpp src/helper/helper_server.cpp src/helper/provider_factory.cpp \
	src/helper/providers/cpu_provider.cpp \
	src/helper/providers/memory_provider.cpp \
	src/helper/providers/process_provider.cpp \
	src/helper/providers/network_provider.cpp \
	src/helper/providers/thermal_provider.cpp
HELPER_MM = src/helper/providers/disk_provider.mm
HELPER_OBJS = $(HELPER_SRCS:.cpp=.o) $(HELPER_MM:.mm=.o)

.PHONY: all clean test

ASSETS = assets/logo.png
APP_OBJS = src/app/main.o src/app/app_state.o src/app/ui_assets.o $(CORE_OBJS) $(IMGUI_SRCS:.cpp=.o)

all: marrow-app marrow-helper marrow-cli

marrow-app: $(APP_OBJS) $(ASSETS)
	$(CXX) $(APP_OBJS) $(LDFLAGS_COMMON) -o $@

marrow-helper: $(HELPER_OBJS) $(CORE_OBJS)
	$(CXX) $^ $(LDFLAGS_HELPER) -o $@

marrow-cli: src/cli/main.o $(CORE_OBJS)
	$(CXX) $^ -lsqlite3 -o $@

test: marrow-tests
	./marrow-tests

marrow-tests: tests/test_main.o $(CORE_OBJS)
	$(CXX) $^ -lsqlite3 -o $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

%.o: %.mm
	$(CXX) $(CXXFLAGS) -ObjC++ -c $< -o $@

$(IMGUI_DIR)/%.o: $(IMGUI_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f marrow-app marrow-helper marrow-cli marrow-tests \
		src/app/*.o src/helper/*.o src/helper/providers/*.o src/cli/*.o \
		src/common/*.o src/storage/*.o tests/*.o \
		$(IMGUI_DIR)/*.o $(IMGUI_DIR)/backends/*.o
