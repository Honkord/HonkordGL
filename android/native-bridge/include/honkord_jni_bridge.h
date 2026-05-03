#ifndef HONKORD_JNI_BRIDGE_H
#define HONKORD_JNI_BRIDGE_H

#if defined(_WIN32)
#  define HGJ_EXPORT __declspec(dllexport)
#else
#  define HGJ_EXPORT __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

HGJ_EXPORT int hgInitializeAudio(int flags);
HGJ_EXPORT void hgShutdownAudio(void);
HGJ_EXPORT int hgGetAudioDecoderCount(void);

#ifdef __cplusplus
}
#endif

#endif