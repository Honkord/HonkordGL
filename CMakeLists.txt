cmake_minimum_required(VERSION 3.20)

project(HonkordGL VERSION 1.0.0 LANGUAGES C CXX)

if(APPLE)
    enable_language(OBJCXX)
endif()

set(HONKORDGL_ROOT "${CMAKE_CURRENT_LIST_DIR}")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(APPLE)
    set(CMAKE_OBJCXX_STANDARD 17)
    set(CMAKE_OBJCXX_STANDARD_REQUIRED ON)
endif()

if(APPLE AND NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version" FORCE)
endif()

include(HonkordGL)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
if(BUILD_SHARED_LIBS)
    set(HONKORDGL_INSTALL_SHARED_LIBS ON)
else()
    set(HONKORDGL_INSTALL_SHARED_LIBS OFF)
endif()
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/HonkordGLConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/HonkordGLConfig.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/HonkordGL")
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/HonkordGLConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/honkordgl.pc.in"
    "${CMAKE_CURRENT_BINARY_DIR}/honkordgl.pc"
    @ONLY)

if(HONKORDGL_INSTALL)
    install(TARGETS HonkordGL
        EXPORT HonkordGLTargets
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/HonkordGL"
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/generated/HonkordGL/BuildFeatures.h"
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/HonkordGL)
    install(EXPORT HonkordGLTargets
        FILE HonkordGLTargets.cmake
        NAMESPACE HonkordGL::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/HonkordGL)
    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/HonkordGLConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/HonkordGLConfigVersion.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/HonkordGL)
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/honkordgl.pc"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
endif()

add_library(HonkordGL_C OBJECT
    "${HONKORDGL_ROOT}/bundles/C/HonkordGL_C.cpp"
)
target_include_directories(HonkordGL_C
    PUBLIC
        "${HONKORDGL_ROOT}/bundles/C"
        "${HONKORDGL_ROOT}/include"
)
target_link_libraries(HonkordGL_C PRIVATE HonkordGL)

add_custom_target(HonkordGL_C_bundle_object ALL
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "$<TARGET_OBJECTS:HonkordGL_C>"
        "${HONKORDGL_ROOT}/bundles/C/HonkordGL_C.o"
    DEPENDS HonkordGL_C
    VERBATIM
)
