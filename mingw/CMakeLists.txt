cmake_minimum_required(VERSION 3.20)

project(HonkordGLMingw LANGUAGES C CXX)

set(HONKORDGL_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")

option(HONKORDGL_BUILD_EXAMPLES "Build works/ examples" ON)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(HONKORDGL_WINDOWS_SRCS
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
    "${HONKORDGL_ROOT}/src/Win32/WindowBackend.cpp"
    "${HONKORDGL_ROOT}/src/Win32/EventsWin32.cpp"
    "${HONKORDGL_ROOT}/src/Win32/WGLRendererContext.cpp"
    "${HONKORDGL_ROOT}/src/Win32/D3D11RendererContext.cpp"
)

add_library(HonkordGL SHARED ${HONKORDGL_WINDOWS_SRCS})

target_include_directories(HonkordGL
    PUBLIC
        "${HONKORDGL_ROOT}/include"
    PRIVATE
        "${HONKORDGL_ROOT}/src"
        "${HONKORDGL_ROOT}/src/Wayland"
)

target_link_libraries(HonkordGL
    PRIVATE
        user32
        gdi32
        winmm
        ole32
        ws2_32
        opengl32
        d3d11
        dxgi
        d3dcompiler
)

target_compile_options(HonkordGL PRIVATE
    "$<$<COMPILE_LANGUAGE:C>:-static-libgcc>"
    "$<$<COMPILE_LANGUAGE:CXX>:-static-libgcc;-static-libstdc++>"
)

if(HONKORDGL_BUILD_EXAMPLES)
    set(HONKORDGL_EXAMPLES
        MovingSquare
        Tetris
        Checkerboard
        AsteroidGame
        GPURaytracing
        SplitScreen
        CameraPlayer
    )

    foreach(example IN LISTS HONKORDGL_EXAMPLES)
        add_executable("${example}" "${HONKORDGL_ROOT}/works/${example}/Main.cpp")
        target_include_directories("${example}" PRIVATE
            "${HONKORDGL_ROOT}/include"
            "${HONKORDGL_ROOT}/src"
        )
        target_link_libraries("${example}" PRIVATE HonkordGL)
        target_link_options("${example}" PRIVATE -static-libgcc -static-libstdc++)
        add_dependencies("${example}" HonkordGL)
        add_custom_command(TARGET "${example}" POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                    "$<TARGET_FILE:HonkordGL>"
                    "$<TARGET_FILE_DIR:${example}>"
        )
    endforeach()
endif()
