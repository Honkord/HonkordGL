#include <jni.h>

#include "../../C/HonkordGL_C.h"

extern "C" {

JNIEXPORT jint JNICALL Java_com_honkordgl_HonkordGL_buildPlatform(JNIEnv *, jclass) {
    return hkgl_c_build_platform();
}

JNIEXPORT jint JNICALL Java_com_honkordgl_HonkordGL_videoDriverCount(JNIEnv *, jclass) {
    return hkgl_c_video_driver_count();
}

JNIEXPORT jlong JNICALL Java_com_honkordgl_HonkordGL_appCreate(JNIEnv *, jclass) {
    return reinterpret_cast<jlong>(hkgl_c_app_create());
}

JNIEXPORT void JNICALL Java_com_honkordgl_HonkordGL_appDestroy(JNIEnv *, jclass, jlong appHandle) {
    hkgl_c_app_destroy(reinterpret_cast<HKGL_C_App *>(appHandle));
}

JNIEXPORT void JNICALL Java_com_honkordgl_HonkordGL_appSetTitle(JNIEnv * env, jclass, jlong appHandle, jstring title) {
    const char * raw = title ? env->GetStringUTFChars(title, nullptr) : nullptr;
    hkgl_c_app_set_title(reinterpret_cast<HKGL_C_App *>(appHandle), raw);
    if (title && raw) env->ReleaseStringUTFChars(title, raw);
}

JNIEXPORT void JNICALL Java_com_honkordgl_HonkordGL_appSetClientRect(JNIEnv *, jclass, jlong appHandle, jint x, jint y, jint w, jint h) {
    hkgl_c_app_set_client_rect(reinterpret_cast<HKGL_C_App *>(appHandle), x, y, w, h);
}

JNIEXPORT jlong JNICALL Java_com_honkordgl_HonkordGL_backendCreate(JNIEnv *, jclass) {
    return reinterpret_cast<jlong>(hkgl_c_backend_create());
}

JNIEXPORT void JNICALL Java_com_honkordgl_HonkordGL_backendDestroy(JNIEnv *, jclass, jlong backendHandle) {
    hkgl_c_backend_destroy(reinterpret_cast<HKGL_C_WindowBackend *>(backendHandle));
}

JNIEXPORT jboolean JNICALL Java_com_honkordgl_HonkordGL_backendInitialize(JNIEnv * env, jclass, jlong backendHandle, jstring displayOrAppName) {
    const char * raw = displayOrAppName ? env->GetStringUTFChars(displayOrAppName, nullptr) : nullptr;
    const bool ok = hkgl_c_backend_initialize(reinterpret_cast<HKGL_C_WindowBackend *>(backendHandle), raw);
    if (displayOrAppName && raw) env->ReleaseStringUTFChars(displayOrAppName, raw);
    return ok ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL Java_com_honkordgl_HonkordGL_backendCreateWindow(JNIEnv *, jclass, jlong backendHandle, jlong appHandle) {
    return hkgl_c_backend_create_window(reinterpret_cast<HKGL_C_WindowBackend *>(backendHandle), reinterpret_cast<HKGL_C_App *>(appHandle))
        ? JNI_TRUE
        : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL Java_com_honkordgl_HonkordGL_backendProcessEvents(JNIEnv *, jclass, jlong backendHandle, jlong appHandle) {
    return hkgl_c_backend_process_events(reinterpret_cast<HKGL_C_WindowBackend *>(backendHandle), reinterpret_cast<HKGL_C_App *>(appHandle))
        ? JNI_TRUE
        : JNI_FALSE;
}

JNIEXPORT jlong JNICALL Java_com_honkordgl_HonkordGL_rendererCreate(JNIEnv *, jclass) {
    return reinterpret_cast<jlong>(hkgl_c_renderer_create());
}

JNIEXPORT void JNICALL Java_com_honkordgl_HonkordGL_rendererDestroy(JNIEnv *, jclass, jlong rendererHandle) {
    hkgl_c_renderer_destroy(reinterpret_cast<HKGL_C_GpuRenderer *>(rendererHandle));
}

JNIEXPORT void JNICALL Java_com_honkordgl_HonkordGL_rendererBind(JNIEnv *, jclass, jlong rendererHandle, jlong appHandle) {
    hkgl_c_renderer_bind(reinterpret_cast<HKGL_C_GpuRenderer *>(rendererHandle), reinterpret_cast<HKGL_C_App *>(appHandle));
}

JNIEXPORT jint JNICALL Java_com_honkordgl_HonkordGL_rendererCreateBestAvailable(JNIEnv *, jclass, jlong rendererHandle) {
    return hkgl_c_renderer_create_best_available(reinterpret_cast<HKGL_C_GpuRenderer *>(rendererHandle));
}

JNIEXPORT jint JNICALL Java_com_honkordgl_HonkordGL_rendererMakeCurrent(JNIEnv *, jclass, jlong rendererHandle) {
    return hkgl_c_renderer_make_current(reinterpret_cast<HKGL_C_GpuRenderer *>(rendererHandle));
}

JNIEXPORT void JNICALL Java_com_honkordgl_HonkordGL_rendererClearColor(
    JNIEnv *, jclass, jlong rendererHandle, jfloat r, jfloat g, jfloat b, jfloat a) {
    hkgl_c_renderer_clear_color(reinterpret_cast<HKGL_C_GpuRenderer *>(rendererHandle), r, g, b, a);
}

JNIEXPORT void JNICALL Java_com_honkordgl_HonkordGL_rendererSwapBuffers(JNIEnv *, jclass, jlong rendererHandle) {
    hkgl_c_renderer_swap_buffers(reinterpret_cast<HKGL_C_GpuRenderer *>(rendererHandle));
}

} // extern "C"
