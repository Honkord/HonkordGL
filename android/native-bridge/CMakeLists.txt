cmake_minimum_required(VERSION 3.20)
project(honkord_jni_bridge LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(HONKORD_ROOT "${CMAKE_CURRENT_LIST_DIR}/../..")

add_library(honkord_jni_bridge SHARED
    src/honkord_jni_bridge.cpp
    src/generated_bindings.cpp
)

target_include_directories(honkord_jni_bridge
    PRIVATE
        include
        "${HONKORD_ROOT}/include"
)

# Keep bridge independent: it links against prebuilt HonkordGL.
# Configure HONKORDGL_LIB_DIR per platform/toolchain.
set(HONKORDGL_LIB_DIR "" CACHE PATH "Directory containing HonkordGL native library")
if (HONKORDGL_LIB_DIR)
    target_link_directories(honkord_jni_bridge PRIVATE "${HONKORDGL_LIB_DIR}")
endif()

if (ANDROID)
    find_library(log-lib log)
    set(HONKORDGL_ANDROID_PREBUILT
        "${HONKORDGL_LIB_DIR}/${ANDROID_ABI}/libHonkordGL.so"
        CACHE FILEPATH "Path to prebuilt HonkordGL Android shared library")
    if (EXISTS "${HONKORDGL_ANDROID_PREBUILT}")
        add_library(HonkordGL SHARED IMPORTED)
        set_target_properties(HonkordGL PROPERTIES IMPORTED_LOCATION "${HONKORDGL_ANDROID_PREBUILT}")
    endif()
    target_link_libraries(honkord_jni_bridge PRIVATE HonkordGL ${log-lib})
elseif (WIN32)
    target_link_libraries(honkord_jni_bridge PRIVATE HonkordGL)
elseif (APPLE)
    target_link_libraries(honkord_jni_bridge PRIVATE HonkordGL)
else()
    target_link_libraries(honkord_jni_bridge PRIVATE HonkordGL dl pthread)
endif()
