package org.honkord

object HonkordAndroid {
    init {
        System.loadLibrary("honkord_jni_bridge")
    }

    external fun hgInitializeAudio(flags: Int = 0): Int
    external fun hgShutdownAudio()
    external fun hgGetAudioDecoderCount(): Int
}
