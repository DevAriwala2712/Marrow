CMAKE ?= cmake
BUILD_DIR ?= build

.PHONY: all configure clean test

all: configure
	$(CMAKE) --build "$(BUILD_DIR)" --parallel

configure:
	$(CMAKE) -S . -B "$(BUILD_DIR)" -DCMAKE_BUILD_TYPE=Release

test: all
	$(CMAKE) -E chdir "$(BUILD_DIR)" ctest --output-on-failure

clean:
	$(CMAKE) --build "$(BUILD_DIR)" --target clean
