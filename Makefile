CXX ?= g++
CXXFLAGS = -std=c++17 -Wall -Wextra -Iinclude -I.. -I../imgui -I../imgui/backends \
	$(shell pkg-config --cflags glfw3 2>/dev/null || echo -I/opt/homebrew/include)
LDFLAGS_COMMON = -lsqlite3 $(shell pkg-config --libs glfw3 2>/dev/null || echo -L/opt/homebrew/lib -lglfw) \
	-framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo

IMGUI_DIR = ../imgui
IMGUI_SRCS = $(IMGUI_DIR)/imgui.cpp $(IMGUI_DIR)/imgui_draw.cpp $(IMGUI_DIR)/imgui_tables.cpp \
	$(IMGUI_DIR)/imgui_widgets.cpp $(IMGUI_DIR)/backends/imgui_impl_glfw.cpp \
	$(IMGUI_DIR)/backends/imgui_impl_opengl3.cpp

CORE_SRCS = src/storage/sqlite_ring_buffer.cpp src/common/ipc.cpp
CORE_OBJS = $(CORE_SRCS:.cpp=.o)

.PHONY: all clean test

all: sysscope-app sysscope-helper sysscope-cli

sysscope-app: src/app/main.o src/app/app_state.o $(CORE_OBJS) $(IMGUI_SRCS:.cpp=.o)
	$(CXX) $^ $(LDFLAGS_COMMON) -o $@

sysscope-helper: src/helper/main.o $(CORE_OBJS)
	$(CXX) $^ -lsqlite3 -o $@

sysscope-cli: src/cli/main.o $(CORE_OBJS)
	$(CXX) $^ -lsqlite3 -o $@

test: sysscope-tests
	./sysscope-tests

sysscope-tests: tests/test_main.o $(CORE_OBJS)
	$(CXX) $^ -lsqlite3 -o $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(IMGUI_DIR)/%.o: $(IMGUI_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f sysscope-app sysscope-helper sysscope-cli sysscope-tests \
		src/app/*.o src/helper/*.o src/cli/*.o src/common/*.o src/storage/*.o tests/*.o \
		$(IMGUI_DIR)/*.o $(IMGUI_DIR)/backends/*.o
