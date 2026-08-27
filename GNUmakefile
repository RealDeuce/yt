CMAKE ?= cmake
BUILD_DIR ?= build
CONFIG ?= $(if $(DEBUG),Debug,Release)
CMAKE_FLAGS ?=

.DEFAULT_GOAL := all

.PHONY: all build game data configure test install clean deepclean

all build game: configure
	$(CMAKE) --build "$(BUILD_DIR)" --config "$(CONFIG)"

data: configure
	$(CMAKE) --build "$(BUILD_DIR)" --config "$(CONFIG)" --target yt-data

configure:
	$(CMAKE) -S . -B "$(BUILD_DIR)" \
	    -DCMAKE_BUILD_TYPE="$(CONFIG)" $(CMAKE_FLAGS)

test: configure
	$(CMAKE) --build "$(BUILD_DIR)" --config "$(CONFIG)"
	ctest --test-dir "$(BUILD_DIR)" -C "$(CONFIG)" --output-on-failure

install: configure
	$(CMAKE) --build "$(BUILD_DIR)" --config "$(CONFIG)"
	$(CMAKE) --install "$(BUILD_DIR)" --config "$(CONFIG)" \
	    $(if $(PREFIX),--prefix "$(PREFIX)")

clean deepclean:
	$(CMAKE) -E remove_directory "$(BUILD_DIR)"
