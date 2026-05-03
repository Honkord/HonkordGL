package org.honkord.generated

object VideoBinding {
    init {
        System.loadLibrary("honkord_jni_bridge")
    }

    external fun isVideoInitialized(): Int
}
