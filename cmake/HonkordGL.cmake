if(NOT DEFINED HONKORDGL_ROOT)
    message(FATAL_ERROR "HonkordGL.cmake requires HONKORDGL_ROOT")
endif()

option(BUILD_SHARED_LIBS "Build HonkordGL as a shared library (otherwise static)" ON)
option(HONKORDGL_BUILD_EXAMPLES "Build works/ examples" ON)
option(HONKORDGL_BUILD_TESTS "Build tests/" ON)
option(HONKORDGL_USE_BUNDLED_WAYLAND_PROTOCOLS "Use checked-in Wayland protocol sources instead of wayland-scanner" OFF)

set(HONKORDGL_CORE_LINUX
    "${HONKORDGL_ROOT}/src/LinuxDisplayIntegration.cpp"
    "${HONKORDGL_ROOT}/src/LinuxEvdevIntegration.cpp"
    "${HONKORDGL_ROOT}/src/Direct3DIntegrationStub.cpp"
    "${HONKORDGL_ROOT}/src/MetalIntegrationStub.cpp"
    "${HONKORDGL_ROOT}/src/Video.cpp"
    "${HONKORDGL_ROOT}/src/WindowBackend.cpp"
    "${HONKORDGL_ROOT}/src/SoftwareRenderer.cpp"
    "${HONKORDGL_ROOT}/src/TextUI.cpp"
    "${HONKORDGL_ROOT}/src/GpuRenderer.cpp"
    "${HONKORDGL_ROOT}/src/GpuRenderTarget.cpp"
    "${HONKORDGL_ROOT}/src/GpuRenderGraph.cpp"
    "${HONKORDGL_ROOT}/src/GpuCapabilities.cpp"
    "${HONKORDGL_ROOT}/src/OpenGlIntegration.cpp"
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
)

if(UNIX AND NOT APPLE AND NOT ANDROID)
    find_package(Vulkan QUIET)
endif()

if(Vulkan_FOUND AND UNIX AND NOT APPLE AND NOT ANDROID)
    list(APPEND HONKORDGL_CORE_LINUX "${HONKORDGL_ROOT}/src/VulkanRendererContext.cpp")
elseif(UNIX AND NOT APPLE AND NOT ANDROID)
    list(APPEND HONKORDGL_CORE_LINUX "${HONKORDGL_ROOT}/works/AsteroidGame/VulkanNoop.cpp")
    list(APPEND HONKORDGL_CORE_LINUX "${HONKORDGL_ROOT}/src/VulkanIntegrationStubs.cpp")
    message(WARNING "HonkordGL: Vulkan SDK not found — using VulkanNoop and VulkanIntegration stubs (install libvulkan-dev for full Vulkan attach + integration API).")
endif()

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
    "${HONKORDGL_ROOT}/src/VulkanIntegrationStubs.cpp"
    "${HONKORDGL_ROOT}/src/LinuxDisplayIntegration.cpp"
    "${HONKORDGL_ROOT}/src/LinuxEvdevIntegration.cpp"
    "${HONKORDGL_ROOT}/src/MetalIntegrationStub.cpp"
    "${HONKORDGL_ROOT}/src/Video.cpp"
    "${HONKORDGL_ROOT}/src/WindowBackend.cpp"
    "${HONKORDGL_ROOT}/src/SoftwareRenderer.cpp"
    "${HONKORDGL_ROOT}/src/TextUI.cpp"
    "${HONKORDGL_ROOT}/src/GpuRenderer.cpp"
    "${HONKORDGL_ROOT}/src/GpuRenderTarget.cpp"
    "${HONKORDGL_ROOT}/src/GpuRenderGraph.cpp"
    "${HONKORDGL_ROOT}/src/GpuCapabilities.cpp"
    "${HONKORDGL_ROOT}/src/OpenGlIntegration.cpp"
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
elseif(APPLE)
    include("${CMAKE_CURRENT_LIST_DIR}/HonkordGLApple.cmake")
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
    if(Vulkan_FOUND)
        target_link_libraries(HonkordGL PUBLIC Vulkan::Vulkan)
    endif()
else()
    message(FATAL_ERROR "HonkordGL CMake currently supports Windows, Linux desktop, and macOS (Apple) only.")
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

option(HONKORDGL_ENFORCE_PUBLIC_HEADER_BOUNDARY "Run scripts/check_public_headers.py before building HonkordGL" ON)
if(HONKORDGL_ENFORCE_PUBLIC_HEADER_BOUNDARY)
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        add_custom_target(honkordgl_public_header_check
            COMMAND ${Python3_EXECUTABLE} "${HONKORDGL_ROOT}/scripts/check_public_headers.py"
            COMMENT "Checking HonkordGL public header boundary"
            VERBATIM)
        add_dependencies(HonkordGL honkordgl_public_header_check)
    else()
        message(WARNING "HonkordGL: Python 3 not found; install it or set HONKORDGL_ENFORCE_PUBLIC_HEADER_BOUNDARY=OFF (public header boundary check skipped)")
    endif()
endif()

if(MINGW)
    target_compile_options(HonkordGL PRIVATE
        "$<$<COMPILE_LANGUAGE:C>:-static-libgcc>"
        "$<$<COMPILE_LANGUAGE:CXX>:-static-libgcc;-static-libstdc++>")
endif()

# Xcode: pair each executable’s primary source with its misc/pkgconfig/macos/*.pc in the Project Navigator.
function(honkordgl_xcode_pair_sources target group_prefix subdir main_filename pc_basename)
    if(NOT APPLE OR NOT CMAKE_GENERATOR MATCHES "Xcode")
        return()
    endif()
    set(_main "${HONKORDGL_ROOT}/${group_prefix}/${subdir}/${main_filename}")
    set(_pc "${HONKORDGL_ROOT}/misc/pkgconfig/macos/${pc_basename}.pc")
    if(NOT EXISTS "${_pc}")
        return()
    endif()
    target_sources(${target} PRIVATE "${_pc}")
    set_source_files_properties("${_pc}" PROPERTIES HEADER_FILE_ONLY ON)
    source_group("${group_prefix}/${subdir}" FILES "${_main}" "${_pc}")
endfunction()

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
    if(APPLE AND BUILD_SHARED_LIBS)
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
    if(APPLE AND BUILD_SHARED_LIBS)
        set_target_properties("${target_name}" PROPERTIES
            BUILD_RPATH "@loader_path"
            INSTALL_RPATH "@loader_path")
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

    set(_hm "${HONKORDGL_ROOT}/works/OpenGLHeightmap/Main.cpp")
    honkordgl_private_app(OpenGLHeightmap _hm)
    honkordgl_set_example_output_dir(OpenGLHeightmap works/OpenGLHeightmap)

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
    if((WIN32 OR APPLE) AND BUILD_SHARED_LIBS)
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
    if((WIN32 OR APPLE) AND BUILD_SHARED_LIBS)
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
    if((WIN32 OR APPLE) AND BUILD_SHARED_LIBS)
        add_custom_command(TARGET NuklearDemo POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "$<TARGET_FILE:HonkordGL>" "$<TARGET_FILE_DIR:NuklearDemo>")
    endif()

    if(APPLE AND CMAKE_GENERATOR MATCHES "Xcode")
        honkordgl_xcode_pair_sources(MovingSquare works MovingSquare Main.cpp MovingSquare)
        honkordgl_xcode_pair_sources(Tetris works Tetris Main.cpp Tetris)
        honkordgl_xcode_pair_sources(Checkerboard works Checkerboard Main.cpp Checkerboard)
        honkordgl_xcode_pair_sources(AsteroidGame works AsteroidGame Main.cpp AsteroidGame)
        honkordgl_xcode_pair_sources(GPURaytracing works GPURaytracing Main.cpp GPURaytracing)
        honkordgl_xcode_pair_sources(OpenGLHeightmap works OpenGLHeightmap Main.cpp OpenGLHeightmap)
        honkordgl_xcode_pair_sources(SplitScreen works SplitScreen Main.cpp SplitScreen)
        honkordgl_xcode_pair_sources(CameraPlayer works CameraPlayer Main.cpp CameraPlayer)

        set(_pc_imgui_demo "${HONKORDGL_ROOT}/misc/pkgconfig/macos/ImGuiDemo.pc")
        target_sources(ImGuiDemo PRIVATE "${_pc_imgui_demo}")
        set_source_files_properties("${_pc_imgui_demo}" PROPERTIES HEADER_FILE_ONLY ON)
        set(_imgui_demo_main "${HONKORDGL_ROOT}/works/ImGuiDemo/Main.cpp")
        set(_imgui_demo_vendor
            "${HONKORDGL_ROOT}/third_party/HonkordGL_ImGui/imgui.cpp"
            "${HONKORDGL_ROOT}/third_party/HonkordGL_ImGui/imgui_draw.cpp"
            "${HONKORDGL_ROOT}/third_party/HonkordGL_ImGui/imgui_tables.cpp"
            "${HONKORDGL_ROOT}/third_party/HonkordGL_ImGui/imgui_widgets.cpp"
            "${HONKORDGL_ROOT}/third_party/HonkordGL_ImGui/imgui_impl_honkordgl.cpp")
        source_group("works/ImGuiDemo" FILES "${_imgui_demo_main}")
        source_group("works/ImGuiDemo/imgui" FILES ${_imgui_demo_vendor})
        source_group("works/ImGuiDemo/pkgconfig" FILES "${_pc_imgui_demo}")

        set(_pc_imgui_sw "${HONKORDGL_ROOT}/misc/pkgconfig/macos/ImGuiSoftwareDemo.pc")
        target_sources(ImGuiSoftwareDemo PRIVATE "${_pc_imgui_sw}")
        set_source_files_properties("${_pc_imgui_sw}" PROPERTIES HEADER_FILE_ONLY ON)
        set(_imgui_sw_main "${HONKORDGL_ROOT}/works/ImGuiSoftwareDemo/Main.cpp")
        set(_imgui_sw_vendor
            "${HONKORDGL_ROOT}/third_party/HonkordGL_ImGui/imgui.cpp"
            "${HONKORDGL_ROOT}/third_party/HonkordGL_ImGui/imgui_draw.cpp"
            "${HONKORDGL_ROOT}/third_party/HonkordGL_ImGui/imgui_tables.cpp"
            "${HONKORDGL_ROOT}/third_party/HonkordGL_ImGui/imgui_widgets.cpp"
            "${HONKORDGL_ROOT}/third_party/HonkordGL_ImGui/imgui_impl_honkord_software.cpp")
        source_group("works/ImGuiSoftwareDemo" FILES "${_imgui_sw_main}")
        source_group("works/ImGuiSoftwareDemo/imgui" FILES ${_imgui_sw_vendor})
        source_group("works/ImGuiSoftwareDemo/pkgconfig" FILES "${_pc_imgui_sw}")

        set(_pc_nk "${HONKORDGL_ROOT}/misc/pkgconfig/macos/NuklearDemo.pc")
        target_sources(NuklearDemo PRIVATE "${_pc_nk}")
        set_source_files_properties("${_pc_nk}" PROPERTIES HEADER_FILE_ONLY ON)
        set(_nk_main "${HONKORDGL_ROOT}/works/NuklearDemo/Main.cpp")
        set(_nk_vendor
            "${HONKORDGL_ROOT}/third_party/HonkordGL_Nuklear/nuklear_honkord_core.cpp"
            "${HONKORDGL_ROOT}/third_party/HonkordGL_Nuklear/nuklear_impl_honkord_input.cpp"
            "${HONKORDGL_ROOT}/third_party/HonkordGL_Nuklear/nuklear_impl_honkordgl.cpp")
        source_group("works/NuklearDemo" FILES "${_nk_main}")
        source_group("works/NuklearDemo/nuklear" FILES ${_nk_vendor})
        source_group("works/NuklearDemo/pkgconfig" FILES "${_pc_nk}")
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

    if(APPLE AND CMAKE_GENERATOR MATCHES "Xcode")
        honkordgl_xcode_pair_sources(TestWindow tests Window TestWindow.cpp TestWindow)
        honkordgl_xcode_pair_sources(TestSprite tests DrawSprite TestSprite.cpp TestSprite)
        honkordgl_xcode_pair_sources(TestDeferredRenderer tests DeferredRenderer TestDeferredRenderer.cpp TestDeferredRenderer)
        honkordgl_xcode_pair_sources(TestAudioSmoke tests Audio TestAudioSmoke.cpp TestAudioSmoke)
    endif()
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
