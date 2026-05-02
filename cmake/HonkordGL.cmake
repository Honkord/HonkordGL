if(NOT DEFINED HONKORDGL_ROOT)
    message(FATAL_ERROR "HonkordGL.cmake requires HONKORDGL_ROOT")
endif()

option(BUILD_SHARED_LIBS "Build HonkordGL as a shared library (otherwise static)" ON)
option(HONKORDGL_BUILD_EXAMPLES "Build works/ examples" ON)
option(HONKORDGL_BUILD_TESTS "Build tests/" ON)
option(HONKORDGL_USE_BUNDLED_WAYLAND_PROTOCOLS "Use checked-in Wayland protocol sources instead of wayland-scanner" OFF)

set(HONKORDGL_CORE_LINUX
    "${HONKORDGL_ROOT}/src/Video.cpp"
    "${HONKORDGL_ROOT}/src/WindowBackend.cpp"
    "${HONKORDGL_ROOT}/src/SoftwareRenderer.cpp"
    "${HONKORDGL_ROOT}/src/TextUI.cpp"
    "${HONKORDGL_ROOT}/src/GpuRenderer.cpp"
    "${HONKORDGL_ROOT}/src/GpuShaderCompiler.cpp"
    "${HONKORDGL_ROOT}/src/Camera.cpp"
    "${HONKORDGL_ROOT}/src/Sprite.cpp"
    "${HONKORDGL_ROOT}/src/Eclipse.cpp"
    "${HONKORDGL_ROOT}/src/Lines.cpp"
    "${HONKORDGL_ROOT}/src/DeferredRendererSample.cpp"
    "${HONKORDGL_ROOT}/src/internal/DeferredRendererGpuApi.cpp"
    "${HONKORDGL_ROOT}/src/Software2DContext.cpp"
    "${HONKORDGL_ROOT}/src/SoftwareBlitCollector.cpp"
    "${HONKORDGL_ROOT}/src/Audio/AudioDecodeCommon.cpp"
    "${HONKORDGL_ROOT}/src/Audio/AudioFeatures.cpp"
    "${HONKORDGL_ROOT}/src/Audio/AudioAlsa.cpp"
    "${HONKORDGL_ROOT}/src/Joystick/Joystick.cpp"
    "${HONKORDGL_ROOT}/works/AsteroidGame/VulkanNoop.cpp"
)

set(HONKORDGL_PLATFORM_LINUX
    "${HONKORDGL_ROOT}/src/X11/WindowBackend.cpp"
    "${HONKORDGL_ROOT}/src/X11/EventsX11.cpp"
    "${HONKORDGL_ROOT}/src/X11/CursorX11.cpp"
    "${HONKORDGL_ROOT}/src/X11/RandrMonitorPoll.cpp"
    "${HONKORDGL_ROOT}/src/X11/IpcHelperWindow.cpp"
    "${HONKORDGL_ROOT}/src/X11/MonitorsX11.cpp"
    "${HONKORDGL_ROOT}/src/X11/GLXRendererContext.cpp"
    "${HONKORDGL_ROOT}/src/X11/EGLRendererContextX11.cpp"
    "${HONKORDGL_ROOT}/src/Wayland/WindowBackend.cpp"
    "${HONKORDGL_ROOT}/src/Wayland/EventsWayland.cpp"
    "${HONKORDGL_ROOT}/src/Wayland/PlatformSession.cpp"
    "${HONKORDGL_ROOT}/src/Wayland/IpcWayland.cpp"
    "${HONKORDGL_ROOT}/src/Wayland/EGLRendererContextWayland.cpp"
    "${HONKORDGL_ROOT}/src/DRM/EventsDRM.cpp"
)

set(HONKORDGL_CORE_WINDOWS
    "${HONKORDGL_ROOT}/src/Video.cpp"
    "${HONKORDGL_ROOT}/src/WindowBackend.cpp"
    "${HONKORDGL_ROOT}/src/SoftwareRenderer.cpp"
    "${HONKORDGL_ROOT}/src/TextUI.cpp"
    "${HONKORDGL_ROOT}/src/GpuRenderer.cpp"
    "${HONKORDGL_ROOT}/src/GpuShaderCompiler.cpp"
    "${HONKORDGL_ROOT}/src/Camera.cpp"
    "${HONKORDGL_ROOT}/src/Sprite.cpp"
    "${HONKORDGL_ROOT}/src/Eclipse.cpp"
    "${HONKORDGL_ROOT}/src/Lines.cpp"
    "${HONKORDGL_ROOT}/src/DeferredRendererSample.cpp"
    "${HONKORDGL_ROOT}/src/internal/DeferredRendererGpuApi.cpp"
    "${HONKORDGL_ROOT}/src/Software2DContext.cpp"
    "${HONKORDGL_ROOT}/src/SoftwareBlitCollector.cpp"
    "${HONKORDGL_ROOT}/src/Audio/AudioDecodeCommon.cpp"
    "${HONKORDGL_ROOT}/src/Audio/AudioFeatures.cpp"
    "${HONKORDGL_ROOT}/src/Audio/PipeWire.cpp"
    "${HONKORDGL_ROOT}/src/Joystick/Joystick.cpp"
    "${HONKORDGL_ROOT}/works/AsteroidGame/VulkanNoop.cpp"
)

set(HONKORDGL_PLATFORM_WINDOWS
    "${HONKORDGL_ROOT}/src/Win32/WindowBackend.cpp"
    "${HONKORDGL_ROOT}/src/Win32/EventsWin32.cpp"
    "${HONKORDGL_ROOT}/src/Win32/WGLRendererContext.cpp"
    "${HONKORDGL_ROOT}/src/Win32/D3D11RendererContext.cpp"
)

if(WIN32)
    add_library(HonkordGL ${HONKORDGL_CORE_WINDOWS} ${HONKORDGL_PLATFORM_WINDOWS})
    target_link_libraries(HonkordGL PRIVATE user32 gdi32 winmm ole32 ws2_32 d3d11 dxgi d3dcompiler)
    target_link_libraries(HonkordGL PUBLIC opengl32)
elseif(UNIX AND NOT APPLE AND NOT ANDROID)
    find_program(HONKORDGL_WAYLAND_SCANNER NAMES wayland-scanner)
    if(HONKORDGL_USE_BUNDLED_WAYLAND_PROTOCOLS OR NOT HONKORDGL_WAYLAND_SCANNER)
        file(GLOB HONKORDGL_WAYLAND_SRCS CONFIGURE_DEPENDS
            "${HONKORDGL_ROOT}/src/Wayland/generated/*-protocol.c")
    else()
        include("${CMAKE_CURRENT_LIST_DIR}/WaylandProtocols.cmake")
        set(HONKORDGL_WAYLAND_SRCS "${HONKORD_WAYLAND_GENERATED_C}")
    endif()

    add_library(HonkordGL
        ${HONKORDGL_CORE_LINUX}
        ${HONKORDGL_PLATFORM_LINUX}
        ${HONKORDGL_WAYLAND_SRCS})
    target_link_libraries(HonkordGL PRIVATE
        X11 Xrandr Xi Xcursor
        wayland-client wayland-egl wayland-cursor
        asound dl pthread m)
    target_link_libraries(HonkordGL PUBLIC EGL GL)
else()
    message(FATAL_ERROR "HonkordGL CMake currently supports Windows and Linux desktop only.")
endif()

target_include_directories(HonkordGL
    PUBLIC
        "${HONKORDGL_ROOT}/include"
        "${HONKORDGL_ROOT}/third_party/HonkordGL_ImGui"
        "${HONKORDGL_ROOT}/third_party/HonkordGL_Nuklear"
        "${HONKORDGL_ROOT}/third_party/Nuklear"
    PRIVATE
        "${HONKORDGL_ROOT}/src"
        "${HONKORDGL_ROOT}/src/Wayland"
)

set_target_properties(HonkordGL PROPERTIES
    OUTPUT_NAME HonkordGL
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

if(MINGW)
    target_compile_options(HonkordGL PRIVATE
        "$<$<COMPILE_LANGUAGE:C>:-static-libgcc>"
        "$<$<COMPILE_LANGUAGE:CXX>:-static-libgcc;-static-libstdc++>")
endif()

function(honkordgl_private_app name sources_var)
    add_executable("${name}" ${${sources_var}})
    target_include_directories("${name}" PRIVATE
        "${HONKORDGL_ROOT}/include"
        "${HONKORDGL_ROOT}/src")
    target_link_libraries("${name}" PRIVATE HonkordGL)
    add_dependencies("${name}" HonkordGL)
    if(MINGW)
        target_compile_options("${name}" PRIVATE -static-libgcc -static-libstdc++)
        target_link_options("${name}" PRIVATE -static-libgcc -static-libstdc++)
    endif()
    if(WIN32 AND BUILD_SHARED_LIBS)
        add_custom_command(TARGET "${name}" POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "$<TARGET_FILE:HonkordGL>"
                "$<TARGET_FILE_DIR:${name}>")
    endif()
endfunction()

function(honkordgl_set_example_output_dir target_name subdir)
    set_target_properties("${target_name}" PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${HONKORDGL_ROOT}/${subdir}")
    if(UNIX AND NOT APPLE AND NOT ANDROID AND BUILD_SHARED_LIBS)
        file(RELATIVE_PATH _rp "${HONKORDGL_ROOT}/${subdir}" "${CMAKE_BINARY_DIR}")
        set_target_properties("${target_name}" PROPERTIES
            BUILD_RPATH "$ORIGIN/${_rp}"
            INSTALL_RPATH "$ORIGIN/${_rp}")
    endif()
endfunction()

function(honkordgl_set_test_output_dir target_name subdir)
    honkordgl_set_example_output_dir("${target_name}" "${subdir}")
endfunction()

if(HONKORDGL_BUILD_EXAMPLES)
    set(_movsq "${HONKORDGL_ROOT}/works/MovingSquare/Main.cpp")
    honkordgl_private_app(MovingSquare _movsq)
    honkordgl_set_example_output_dir(MovingSquare works/MovingSquare)

    set(_tet "${HONKORDGL_ROOT}/works/Tetris/Main.cpp")
    honkordgl_private_app(Tetris _tet)
    honkordgl_set_example_output_dir(Tetris works/Tetris)

    set(_chk "${HONKORDGL_ROOT}/works/Checkerboard/Main.cpp")
    honkordgl_private_app(Checkerboard _chk)
    honkordgl_set_example_output_dir(Checkerboard works/Checkerboard)

    set(_ast "${HONKORDGL_ROOT}/works/AsteroidGame/Main.cpp")
    honkordgl_private_app(AsteroidGame _ast)
    honkordgl_set_example_output_dir(AsteroidGame works/AsteroidGame)

    set(_gpu "${HONKORDGL_ROOT}/works/GPURaytracing/Main.cpp")
    honkordgl_private_app(GPURaytracing _gpu)
    honkordgl_set_example_output_dir(GPURaytracing works/GPURaytracing)

    set(_spl "${HONKORDGL_ROOT}/works/SplitScreen/Main.cpp")
    honkordgl_private_app(SplitScreen _spl)
    honkordgl_set_example_output_dir(SplitScreen works/SplitScreen)

    set(_cam "${HONKORDGL_ROOT}/works/CameraPlayer/Main.cpp")
    honkordgl_private_app(CameraPlayer _cam)
    honkordgl_set_example_output_dir(CameraPlayer works/CameraPlayer)

    add_executable(ImGuiDemo
        "${HONKORDGL_ROOT}/works/ImGuiDemo/Main.cpp"
        "${HONKORDGL_ROOT}/third_party/HonkordGL_ImGui/imgui.cpp"
        "${HONKORDGL_ROOT}/third_party/HonkordGL_ImGui/imgui_draw.cpp"
        "${HONKORDGL_ROOT}/third_party/HonkordGL_ImGui/imgui_tables.cpp"
        "${HONKORDGL_ROOT}/third_party/HonkordGL_ImGui/imgui_widgets.cpp"
        "${HONKORDGL_ROOT}/third_party/HonkordGL_ImGui/imgui_impl_honkordgl.cpp")
    target_include_directories(ImGuiDemo PRIVATE
        "${HONKORDGL_ROOT}/include"
        "${HONKORDGL_ROOT}/src")
    target_link_libraries(ImGuiDemo PRIVATE HonkordGL)
    add_dependencies(ImGuiDemo HonkordGL)
    honkordgl_set_example_output_dir(ImGuiDemo works/ImGuiDemo)
    if(MINGW)
        target_compile_options(ImGuiDemo PRIVATE -static-libgcc -static-libstdc++)
        target_link_options(ImGuiDemo PRIVATE -static-libgcc -static-libstdc++)
    endif()
    if(WIN32 AND BUILD_SHARED_LIBS)
        add_custom_command(TARGET ImGuiDemo POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "$<TARGET_FILE:HonkordGL>" "$<TARGET_FILE_DIR:ImGuiDemo>")
    endif()

    add_executable(ImGuiSoftwareDemo
        "${HONKORDGL_ROOT}/works/ImGuiSoftwareDemo/Main.cpp"
        "${HONKORDGL_ROOT}/third_party/HonkordGL_ImGui/imgui.cpp"
        "${HONKORDGL_ROOT}/third_party/HonkordGL_ImGui/imgui_draw.cpp"
        "${HONKORDGL_ROOT}/third_party/HonkordGL_ImGui/imgui_tables.cpp"
        "${HONKORDGL_ROOT}/third_party/HonkordGL_ImGui/imgui_widgets.cpp"
        "${HONKORDGL_ROOT}/third_party/HonkordGL_ImGui/imgui_impl_honkord_software.cpp")
    target_include_directories(ImGuiSoftwareDemo PRIVATE
        "${HONKORDGL_ROOT}/include"
        "${HONKORDGL_ROOT}/src")
    target_link_libraries(ImGuiSoftwareDemo PRIVATE HonkordGL)
    add_dependencies(ImGuiSoftwareDemo HonkordGL)
    honkordgl_set_example_output_dir(ImGuiSoftwareDemo works/ImGuiSoftwareDemo)
    if(MINGW)
        target_compile_options(ImGuiSoftwareDemo PRIVATE -static-libgcc -static-libstdc++)
        target_link_options(ImGuiSoftwareDemo PRIVATE -static-libgcc -static-libstdc++)
    endif()
    if(WIN32 AND BUILD_SHARED_LIBS)
        add_custom_command(TARGET ImGuiSoftwareDemo POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "$<TARGET_FILE:HonkordGL>" "$<TARGET_FILE_DIR:ImGuiSoftwareDemo>")
    endif()

    add_executable(NuklearDemo
        "${HONKORDGL_ROOT}/works/NuklearDemo/Main.cpp"
        "${HONKORDGL_ROOT}/third_party/HonkordGL_Nuklear/nuklear_honkord_core.cpp"
        "${HONKORDGL_ROOT}/third_party/HonkordGL_Nuklear/nuklear_impl_honkord_input.cpp"
        "${HONKORDGL_ROOT}/third_party/HonkordGL_Nuklear/nuklear_impl_honkordgl.cpp")
    target_include_directories(NuklearDemo PRIVATE
        "${HONKORDGL_ROOT}/include"
        "${HONKORDGL_ROOT}/src")
    target_link_libraries(NuklearDemo PRIVATE HonkordGL)
    add_dependencies(NuklearDemo HonkordGL)
    honkordgl_set_example_output_dir(NuklearDemo works/NuklearDemo)
    if(MINGW)
        target_compile_options(NuklearDemo PRIVATE -static-libgcc -static-libstdc++)
        target_link_options(NuklearDemo PRIVATE -static-libgcc -static-libstdc++)
    endif()
    if(WIN32 AND BUILD_SHARED_LIBS)
        add_custom_command(TARGET NuklearDemo POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "$<TARGET_FILE:HonkordGL>" "$<TARGET_FILE_DIR:NuklearDemo>")
    endif()
endif()

if(HONKORDGL_BUILD_TESTS)
    set(_tw "${HONKORDGL_ROOT}/tests/Window/TestWindow.cpp")
    honkordgl_private_app(TestWindow _tw)
    honkordgl_set_test_output_dir(TestWindow tests/Window)

    set(_ts "${HONKORDGL_ROOT}/tests/DrawSprite/TestSprite.cpp")
    honkordgl_private_app(TestSprite _ts)
    honkordgl_set_test_output_dir(TestSprite tests/DrawSprite)

    set(_td "${HONKORDGL_ROOT}/tests/DeferredRenderer/TestDeferredRenderer.cpp")
    honkordgl_private_app(TestDeferredRenderer _td)
    honkordgl_set_test_output_dir(TestDeferredRenderer tests/DeferredRenderer)

    set(_ta "${HONKORDGL_ROOT}/tests/Audio/TestAudioSmoke.cpp")
    honkordgl_private_app(TestAudioSmoke _ta)
    honkordgl_set_test_output_dir(TestAudioSmoke tests/Audio)
endif()

set(HONKORDGL_BUNDLE_COMPILE
    "${CMAKE_CXX_COMPILER}"
    -std=c++17
    -I"${HONKORDGL_ROOT}/include"
    -I"${HONKORDGL_ROOT}/src"
    -c "${HONKORDGL_ROOT}/bundles/HonkordGL.cpp"
    -o "${HONKORDGL_ROOT}/bundles/HonkordGL.o")
if(MINGW)
    list(APPEND HONKORDGL_BUNDLE_COMPILE -static-libgcc -static-libstdc++)
endif()

add_custom_target(bundles-capi
    COMMAND ${HONKORDGL_BUNDLE_COMPILE}
    BYPRODUCTS "${HONKORDGL_ROOT}/bundles/HonkordGL.o"
    COMMENT "Compiling bundles/HonkordGL.cpp to bundles/HonkordGL.o"
    VERBATIM)
