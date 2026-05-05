# HonkordGL — macOS / Apple targets (included from HonkordGL.cmake when APPLE)

set(HONKORDGL_CORE_APPLE
    "${HONKORDGL_ROOT}/src/VulkanIntegrationStubs.cpp"
    "${HONKORDGL_ROOT}/src/LinuxDisplayIntegration.cpp"
    "${HONKORDGL_ROOT}/src/LinuxEvdevIntegration.cpp"
    "${HONKORDGL_ROOT}/src/Direct3DIntegrationStub.cpp"
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
    "${HONKORDGL_ROOT}/src/Audio/CoreAudio.cpp"
    "${HONKORDGL_ROOT}/src/Joystick/Joystick.cpp"
    "${HONKORDGL_ROOT}/src/VulkanRendererContext.cpp"
)

set(HONKORDGL_PLATFORM_APPLE
    "${HONKORDGL_ROOT}/src/Cocoa/WindowBackend.mm"
    "${HONKORDGL_ROOT}/src/Cocoa/EventsCocoa.mm"
    "${HONKORDGL_ROOT}/src/Cocoa/PlatformSession.mm"
    "${HONKORDGL_ROOT}/src/Cocoa/MonitorsCocoa.mm"
    "${HONKORDGL_ROOT}/src/Cocoa/DisplayMonitorPoll.mm"
    "${HONKORDGL_ROOT}/src/Cocoa/CocoaIpcWindow.mm"
    "${HONKORDGL_ROOT}/src/Cocoa/NSOpenGLRendererContext.mm"
    "${HONKORDGL_ROOT}/src/Cocoa/MetalRendererContext.mm"
)

add_library(HonkordGL ${HONKORDGL_CORE_APPLE} ${HONKORDGL_PLATFORM_APPLE})

target_link_libraries(HonkordGL PRIVATE
    "-framework Cocoa"
    "-framework OpenGL"
    "-framework Metal"
    "-framework QuartzCore"
    "-framework CoreAudio"
    "-framework AudioToolbox"
    "-framework IOKit"
)

set_target_properties(HonkordGL PROPERTIES
    MACOSX_RPATH ON
    XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC "YES"
)

if(CMAKE_GENERATOR MATCHES "Xcode")
    set(_honkordgl_lib_sources ${HONKORDGL_CORE_APPLE} ${HONKORDGL_PLATFORM_APPLE})
    source_group(TREE "${HONKORDGL_ROOT}" PREFIX "HonkordGL" FILES ${_honkordgl_lib_sources})

    file(GLOB HONKORDGL_MACOS_PKGCONFIG_FILES CONFIGURE_DEPENDS
        "${HONKORDGL_ROOT}/misc/pkgconfig/macos/*.pc")
    if(HONKORDGL_MACOS_PKGCONFIG_FILES)
        target_sources(HonkordGL PRIVATE ${HONKORDGL_MACOS_PKGCONFIG_FILES})
        set_source_files_properties(${HONKORDGL_MACOS_PKGCONFIG_FILES} PROPERTIES HEADER_FILE_ONLY ON)
        source_group("HonkordGL/pkgconfig" FILES ${HONKORDGL_MACOS_PKGCONFIG_FILES})
    endif()
endif()
