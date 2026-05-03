.RECIPEPREFIX := >

PLATFORM ?= linux
LINKAGE ?= shared
BUILD_DIR ?= build/$(PLATFORM)/$(LINKAGE)
OBJ_DIR := $(BUILD_DIR)/obj

ifeq ($(PLATFORM),linux)
CC ?= gcc
CXX ?= g++
AR ?= ar
SO_EXT := so
DLL_EXT :=
LIB_PREFIX := lib
EXE_EXT :=
PIC_FLAG := -fPIC
SYS_LIBS := -lX11 -lXrandr -lXi -lXcursor -lwayland-client -lwayland-egl -lwayland-cursor -lEGL -lGL -lasound -ldl -lpthread -lm
CORE_SRCS := \
 src/Video.cpp \
 src/WindowBackend.cpp \
 src/SoftwareRenderer.cpp \
 src/TextUI.cpp \
 src/GpuRenderer.cpp \
 src/GpuCapabilities.cpp \
 src/GpuShaderCompiler.cpp \
 src/Camera.cpp \
 src/Sprite.cpp \
 src/Eclipse.cpp \
 src/Lines.cpp \
 src/DeferredRendererSample.cpp \
 src/internal/DeferredRendererGpuApi.cpp \
 src/Software2DContext.cpp \
 src/SoftwareBlitCollector.cpp \
 src/Audio/AudioDecodeCommon.cpp \
 src/Audio/AudioFeatures.cpp \
 src/Audio/AudioAlsa.cpp \
 src/Joystick/Joystick.cpp \
 works/AsteroidGame/VulkanNoop.cpp
PLATFORM_SRCS := \
 src/X11/WindowBackend.cpp \
 src/X11/EventsX11.cpp \
 src/X11/CursorX11.cpp \
 src/X11/RandrMonitorPoll.cpp \
 src/X11/IpcHelperWindow.cpp \
 src/X11/MonitorsX11.cpp \
 src/X11/GLXRendererContext.cpp \
 src/X11/EGLRendererContextX11.cpp \
 src/Wayland/WindowBackend.cpp \
 src/Wayland/EventsWayland.cpp \
 src/Wayland/PlatformSession.cpp \
 src/Wayland/IpcWayland.cpp \
 src/Wayland/EGLRendererContextWayland.cpp \
 src/DRM/EventsDRM.cpp \
 src/Wayland/generated/xdg-shell-protocol.c
else ifeq ($(PLATFORM),windows)
CROSS ?= x86_64-w64-mingw32-
CC ?= $(CROSS)gcc
CXX ?= $(CROSS)g++
AR ?= $(CROSS)ar
SO_EXT := dll
DLL_EXT := .dll
LIB_PREFIX :=
EXE_EXT := .exe
PIC_FLAG :=
SYS_LIBS := -luser32 -lgdi32 -lwinmm -lole32 -lws2_32 -lopengl32 -ld3d11 -ldxgi -ld3dcompiler
WIN_STATIC_RUNTIME_FLAGS := -static-libgcc -static-libstdc++
CORE_SRCS := \
 src/Video.cpp \
 src/WindowBackend.cpp \
 src/SoftwareRenderer.cpp \
 src/TextUI.cpp \
 src/GpuRenderer.cpp \
 src/GpuCapabilities.cpp \
 src/GpuShaderCompiler.cpp \
 src/Camera.cpp \
 src/Sprite.cpp \
 src/Eclipse.cpp \
 src/Lines.cpp \
 src/DeferredRendererSample.cpp \
 src/internal/DeferredRendererGpuApi.cpp \
 src/Software2DContext.cpp \
 src/SoftwareBlitCollector.cpp \
 src/Audio/AudioDecodeCommon.cpp \
 src/Audio/AudioFeatures.cpp \
 src/Audio/PipeWire.cpp \
 src/Joystick/Joystick.cpp \
 works/AsteroidGame/VulkanNoop.cpp
PLATFORM_SRCS := \
 src/Win32/WindowBackend.cpp \
 src/Win32/EventsWin32.cpp \
 src/Win32/WGLRendererContext.cpp \
 src/Win32/D3D11RendererContext.cpp
else
$(error Unsupported PLATFORM '$(PLATFORM)'; use PLATFORM=linux or PLATFORM=windows)
endif

WIN_STATIC_RUNTIME_FLAGS ?=

ifeq ($(LINKAGE),shared)
LIB_FILE := $(BUILD_DIR)/$(LIB_PREFIX)HonkordGL.$(SO_EXT)
LIB_LINK_FLAGS := -shared
else ifeq ($(LINKAGE),static)
LIB_FILE := $(BUILD_DIR)/$(LIB_PREFIX)HonkordGL.a
LIB_LINK_FLAGS :=
else
$(error Unsupported LINKAGE '$(LINKAGE)'; use LINKAGE=shared or LINKAGE=static)
endif

CPPFLAGS := -Iinclude -Isrc -Isrc/Wayland -Ithird_party/HonkordGL_ImGui -Ithird_party/HonkordGL_Nuklear -Ithird_party/Nuklear
CFLAGS := -O2 $(PIC_FLAG)
CXXFLAGS := -std=c++17 -O2 $(PIC_FLAG)
LDFLAGS :=

LIB_SRCS := $(CORE_SRCS) $(PLATFORM_SRCS)
LIB_CPP_SRCS := $(filter %.cpp,$(LIB_SRCS))
LIB_C_SRCS := $(filter %.c,$(LIB_SRCS))
LIB_OBJS := $(patsubst %.cpp,$(OBJ_DIR)/%.o,$(LIB_CPP_SRCS)) $(patsubst %.c,$(OBJ_DIR)/%.o,$(LIB_C_SRCS))

WORKS_BINS := \
 works/MovingSquare/MovingSquare$(EXE_EXT) \
 works/Tetris/Tetris$(EXE_EXT) \
 works/Checkerboard/Checkerboard$(EXE_EXT) \
 works/AsteroidGame/AsteroidGame$(EXE_EXT) \
 works/GPURaytracing/GPURaytracing$(EXE_EXT) \
 works/SplitScreen/SplitScreen$(EXE_EXT) \
 works/CameraPlayer/CameraPlayer$(EXE_EXT) \
 works/ImGuiDemo/ImGuiDemo$(EXE_EXT) \
 works/ImGuiSoftwareDemo/ImGuiSoftwareDemo$(EXE_EXT) \
 works/NuklearDemo/NuklearDemo$(EXE_EXT)

TEST_BINS := \
 tests/Window/TestWindow$(EXE_EXT) \
 tests/DrawSprite/TestSprite$(EXE_EXT) \
 tests/DeferredRenderer/TestDeferredRenderer$(EXE_EXT) \
 tests/Audio/TestAudioSmoke$(EXE_EXT)

ASSET_TARGETS := \
 tests/DrawSprite/Honkord__APILogo.png \
 works/Tetris/Party\ Sector.ogg

.PHONY: all help lib examples tests assets bundles-capi clean \
	linux windows \
	cmake-mingw cmake-mingw-x86 \
	cross-platform all-cross \
	visualc msbuild

all: lib examples tests assets

help:
>@echo "Usage:"
>@echo "  make [PLATFORM=linux|windows] [LINKAGE=shared|static] [target]"
>@echo ""
>@echo "Targets:"
>@echo "  all       Build library + examples + tests + assets (default)"
>@echo "  lib       Build HonkordGL library ($(LINKAGE))"
>@echo "  examples  Build all works/* binaries into each folder"
>@echo "  tests     Build all tests/* binaries into each folder"
>@echo "  assets    Copy runtime assets into example/test folders"
>@echo "  bundles-capi  Compile bundles/HonkordGL.cpp → bundles/HonkordGL.o (simplified C API)"
>@echo "  clean     Remove build artifacts and compiled binaries"
>@echo ""
>@echo "Cross-platform convenience (from repo root):"
>@echo "  linux           Same as: make PLATFORM=linux all"
>@echo "  windows         MinGW cross-build (win32 gcc flavor); PLATFORM=windows all"
>@echo "  cmake-mingw     CMake + mingw toolchain (x64, repo root) -> build/cmake-mingw-x64"
>@echo "  cmake-mingw-x86 CMake + mingw toolchain (x86, repo root) -> build/cmake-mingw-x86"
>@echo "  cross-platform  linux + windows + cmake-mingw (needs toolchains on this machine)"
>@echo "  visualc / msbuild  Build visualC/HonkordGL.sln if msbuild is on PATH"
>@echo "                     (otherwise prints how to open the solution on Windows)"

# -----------------------------------------------------------------------------
# Cross-platform builds (one target per toolchain)
# -----------------------------------------------------------------------------

# Linux: native gcc/g++ (default PLATFORM)
linux:
>$(MAKE) PLATFORM=linux all

# Windows via MinGW: prefer *-gcc-win32 / *-g++-win32 to avoid occasional ld issues
WINDOWS_CC ?= x86_64-w64-mingw32-gcc-win32
WINDOWS_CXX ?= x86_64-w64-mingw32-g++-win32
WINDOWS_AR ?= x86_64-w64-mingw32-ar

windows:
>$(MAKE) PLATFORM=windows CC="$(WINDOWS_CC)" CXX="$(WINDOWS_CXX)" AR="$(WINDOWS_AR)" all

# CMake MinGW (see mingw/README.md)
MINGW_CMAKE_X64_DIR ?= build/cmake-mingw-x64
MINGW_TC_X64 ?= $(abspath cmake/toolchains/mingw-x86_64.cmake)
MINGW_CMAKE_X86_DIR ?= build/cmake-mingw-x86
MINGW_TC_X86 ?= $(abspath cmake/toolchains/mingw-i686.cmake)

cmake-mingw:
>cmake -S . -B "$(MINGW_CMAKE_X64_DIR)" -DCMAKE_TOOLCHAIN_FILE="$(MINGW_TC_X64)" -DHONKORDGL_BUILD_EXAMPLES=ON
>cmake --build "$(MINGW_CMAKE_X64_DIR)" --parallel

cmake-mingw-x86:
>cmake -S . -B "$(MINGW_CMAKE_X86_DIR)" -DCMAKE_TOOLCHAIN_FILE="$(MINGW_TC_X86)" -DHONKORDGL_BUILD_EXAMPLES=ON
>cmake --build "$(MINGW_CMAKE_X86_DIR)" --parallel

# Build every Makefile/CMake path available from a Unix host (Linux/macOS/WSL)
all-cross: linux windows cmake-mingw
cross-platform: all-cross

# Visual Studio solution (Windows Developer Command Prompt, or msbuild on PATH)
visualc msbuild:
>@sh -c 'if command -v msbuild >/dev/null 2>&1; then msbuild visualC/HonkordGL.sln /p:Configuration=Release /p:Platform=x64 /m; elif command -v MSBuild.exe >/dev/null 2>&1; then MSBuild.exe visualC/HonkordGL.sln /p:Configuration=Release /p:Platform=x64 /m; else echo "msbuild not found on PATH."; echo "On Windows run: msbuild visualC/HonkordGL.sln /p:Configuration=Release /p:Platform=x64"; echo "Or open visualC/HonkordGL.sln in Visual Studio."; fi'

bundles-capi: bundles/HonkordGL.o

bundles/HonkordGL.o: bundles/HonkordGL.cpp
>@mkdir -p "$(dir $@)"
>$(CXX) $(CPPFLAGS) -std=c++17 $(WIN_STATIC_RUNTIME_FLAGS) -c bundles/HonkordGL.cpp -o bundles/HonkordGL.o

lib: $(LIB_FILE)

$(LIB_FILE): $(LIB_OBJS)
>@mkdir -p "$(dir $@)"
ifeq ($(LINKAGE),shared)
>$(CXX) $(LDFLAGS) $(LIB_LINK_FLAGS) -o "$@" $(LIB_OBJS) $(SYS_LIBS)
else
>$(AR) rcs "$@" $(LIB_OBJS)
endif

$(OBJ_DIR)/%.o: %.cpp
>@mkdir -p "$(dir $@)"
>$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(WIN_STATIC_RUNTIME_FLAGS) -c "$<" -o "$@"

$(OBJ_DIR)/%.o: %.c
>@mkdir -p "$(dir $@)"
>$(CC) $(CPPFLAGS) $(CFLAGS) -c "$<" -o "$@"

examples: lib $(WORKS_BINS) assets

tests: lib $(TEST_BINS) assets

assets: $(ASSET_TARGETS)

tests/DrawSprite/Honkord__APILogo.png:
>@if [ -f "Honkord__APILogo.png" ]; then cp -f "Honkord__APILogo.png" "$@"; else echo "warning: Honkord__APILogo.png not found at repo root"; fi

works/Tetris/Party\ Sector.ogg:
>@if [ -f "works/Tetris/Party Sector.ogg" ]; then : ; else echo "warning: works/Tetris/Party Sector.ogg not found"; fi

# Build examples against either static or shared library.
ifeq ($(LINKAGE),shared)
LIB_USE_FLAGS := -L$(BUILD_DIR) -Wl,-rpath,'$$ORIGIN/../../build/$(PLATFORM)/$(LINKAGE)' -lHonkordGL
else
LIB_USE_FLAGS := $(LIB_FILE)
endif

works/MovingSquare/MovingSquare$(EXE_EXT): works/MovingSquare/Main.cpp lib
>$(CXX) $(CPPFLAGS) -std=c++17 $(WIN_STATIC_RUNTIME_FLAGS) "$<" -o "$@" $(LIB_USE_FLAGS) $(SYS_LIBS)

works/Tetris/Tetris$(EXE_EXT): works/Tetris/Main.cpp lib
>$(CXX) $(CPPFLAGS) -std=c++17 $(WIN_STATIC_RUNTIME_FLAGS) "$<" -o "$@" $(LIB_USE_FLAGS) $(SYS_LIBS)

works/Checkerboard/Checkerboard$(EXE_EXT): works/Checkerboard/Main.cpp lib
>$(CXX) $(CPPFLAGS) -std=c++17 $(WIN_STATIC_RUNTIME_FLAGS) "$<" -o "$@" $(LIB_USE_FLAGS) $(SYS_LIBS)

works/AsteroidGame/AsteroidGame$(EXE_EXT): works/AsteroidGame/Main.cpp lib
>$(CXX) $(CPPFLAGS) -std=c++17 $(WIN_STATIC_RUNTIME_FLAGS) "$<" -o "$@" $(LIB_USE_FLAGS) $(SYS_LIBS)

works/GPURaytracing/GPURaytracing$(EXE_EXT): works/GPURaytracing/Main.cpp lib
>$(CXX) $(CPPFLAGS) -std=c++17 $(WIN_STATIC_RUNTIME_FLAGS) "$<" -o "$@" $(LIB_USE_FLAGS) $(SYS_LIBS)

works/SplitScreen/SplitScreen$(EXE_EXT): works/SplitScreen/Main.cpp lib
>$(CXX) $(CPPFLAGS) -std=c++17 $(WIN_STATIC_RUNTIME_FLAGS) "$<" -o "$@" $(LIB_USE_FLAGS) $(SYS_LIBS)

works/CameraPlayer/CameraPlayer$(EXE_EXT): works/CameraPlayer/Main.cpp lib
>$(CXX) $(CPPFLAGS) -std=c++17 $(WIN_STATIC_RUNTIME_FLAGS) "$<" -o "$@" $(LIB_USE_FLAGS) $(SYS_LIBS)

works/ImGuiDemo/ImGuiDemo$(EXE_EXT): works/ImGuiDemo/Main.cpp lib
>$(CXX) $(CPPFLAGS) -std=c++17 $(WIN_STATIC_RUNTIME_FLAGS) \
>  works/ImGuiDemo/Main.cpp \
>  third_party/HonkordGL_ImGui/imgui.cpp \
>  third_party/HonkordGL_ImGui/imgui_draw.cpp \
>  third_party/HonkordGL_ImGui/imgui_tables.cpp \
>  third_party/HonkordGL_ImGui/imgui_widgets.cpp \
>  third_party/HonkordGL_ImGui/imgui_impl_honkordgl.cpp \
>  -o "$@" $(LIB_USE_FLAGS) $(SYS_LIBS)

works/ImGuiSoftwareDemo/ImGuiSoftwareDemo$(EXE_EXT): works/ImGuiSoftwareDemo/Main.cpp lib
>$(CXX) $(CPPFLAGS) -std=c++17 $(WIN_STATIC_RUNTIME_FLAGS) \
>  works/ImGuiSoftwareDemo/Main.cpp \
>  third_party/HonkordGL_ImGui/imgui.cpp \
>  third_party/HonkordGL_ImGui/imgui_draw.cpp \
>  third_party/HonkordGL_ImGui/imgui_tables.cpp \
>  third_party/HonkordGL_ImGui/imgui_widgets.cpp \
>  third_party/HonkordGL_ImGui/imgui_impl_honkord_software.cpp \
>  -o "$@" $(LIB_USE_FLAGS) $(SYS_LIBS)

works/NuklearDemo/NuklearDemo$(EXE_EXT): works/NuklearDemo/Main.cpp lib
>$(CXX) $(CPPFLAGS) -std=c++17 $(WIN_STATIC_RUNTIME_FLAGS) \
>  works/NuklearDemo/Main.cpp \
>  third_party/HonkordGL_Nuklear/nuklear_honkord_core.cpp \
>  third_party/HonkordGL_Nuklear/nuklear_impl_honkord_input.cpp \
>  third_party/HonkordGL_Nuklear/nuklear_impl_honkordgl.cpp \
>  -o "$@" $(LIB_USE_FLAGS) $(SYS_LIBS)

tests/Window/TestWindow$(EXE_EXT): tests/Window/TestWindow.cpp lib
>$(CXX) $(CPPFLAGS) -std=c++17 $(WIN_STATIC_RUNTIME_FLAGS) "$<" -o "$@" $(LIB_USE_FLAGS) $(SYS_LIBS)

tests/DrawSprite/TestSprite$(EXE_EXT): tests/DrawSprite/TestSprite.cpp lib
>$(CXX) $(CPPFLAGS) -std=c++17 $(WIN_STATIC_RUNTIME_FLAGS) "$<" -o "$@" $(LIB_USE_FLAGS) $(SYS_LIBS)

tests/DeferredRenderer/TestDeferredRenderer$(EXE_EXT): tests/DeferredRenderer/TestDeferredRenderer.cpp lib
>$(CXX) $(CPPFLAGS) -std=c++17 $(WIN_STATIC_RUNTIME_FLAGS) "$<" -o "$@" $(LIB_USE_FLAGS) $(SYS_LIBS)

tests/Audio/TestAudioSmoke$(EXE_EXT): tests/Audio/TestAudioSmoke.cpp lib
>$(CXX) $(CPPFLAGS) -std=c++17 $(WIN_STATIC_RUNTIME_FLAGS) "$<" -o "$@" $(LIB_USE_FLAGS) $(SYS_LIBS)

clean:
>rm -rf build
>rm -f works/MovingSquare/MovingSquare works/Tetris/Tetris works/Checkerboard/Checkerboard works/AsteroidGame/AsteroidGame works/GPURaytracing/GPURaytracing works/SplitScreen/SplitScreen works/CameraPlayer/CameraPlayer works/ImGuiDemo/ImGuiDemo works/ImGuiSoftwareDemo/ImGuiSoftwareDemo works/NuklearDemo/NuklearDemo
>rm -f works/MovingSquare/MovingSquare.exe works/Tetris/Tetris.exe works/Checkerboard/Checkerboard.exe works/AsteroidGame/AsteroidGame.exe works/GPURaytracing/GPURaytracing.exe works/SplitScreen/SplitScreen.exe works/CameraPlayer/CameraPlayer.exe works/ImGuiDemo/ImGuiDemo.exe works/ImGuiSoftwareDemo/ImGuiSoftwareDemo.exe works/NuklearDemo/NuklearDemo.exe
>rm -f tests/Window/TestWindow tests/DrawSprite/TestSprite tests/DeferredRenderer/TestDeferredRenderer
>rm -f tests/Audio/TestAudioSmoke tests/Window/TestWindow.exe tests/DrawSprite/TestSprite.exe tests/DeferredRenderer/TestDeferredRenderer.exe tests/Audio/TestAudioSmoke.exe
>rm -f bundles/HonkordGL.o bundles/HonkordGL.obj