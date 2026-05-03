package org.honkord

import org.honkord.internal.HonkordNative

object Honkord {
    fun initializeAudio(flags: Int = 0): Int {
        HonkordNative.ensureLoaded()
        return HonkordNative.hgInitializeAudio(flags)
    }

    fun shutdownAudio() {
        HonkordNative.ensureLoaded()
        HonkordNative.hgShutdownAudio()
    }

    fun decoderCount(): Int {
        HonkordNative.ensureLoaded()
        return HonkordNative.hgGetAudioDecoderCount()
    }
}