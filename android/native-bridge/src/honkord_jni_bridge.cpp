#include "honkord_jni_bridge.h"

#include <HonkordGL/Audio.h>

#include <jni.h>

int hgInitializeAudio(int flags)
{
    return HonkordGL::Audio::InitializeAudioSubsystem(static_cast<unsigned int>(flags));
}

void hgShutdownAudio(void)
{
    HonkordGL::Audio::ShutdownAudioSubsystem();
}

int hgGetAudioDecoderCount(void)
{
    return HonkordGL::Audio::GetAudioDecoderCount();
}

extern "C" JNIEXPORT jint JNICALL Java_org_honkord_internal_HonkordNative_hgInitializeAudio(
    JNIEnv *, jclass, jint flags)
{
    return static_cast<jint>(hgInitializeAudio(static_cast<int>(flags)));
}

extern "C" JNIEXPORT void JNICALL Java_org_honkord_internal_HonkordNative_hgShutdownAudio(
    JNIEnv *, jclass)
{
    hgShutdownAudio();
}

extern "C" JNIEXPORT jint JNICALL Java_org_honkord_internal_HonkordNative_hgGetAudioDecoderCount(
    JNIEnv *, jclass)
{
    return static_cast<jint>(hgGetAudioDecoderCount());
}