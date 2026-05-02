package org.honkord.generated

object WindowBinding {
    init {
        System.loadLibrary("honkord_jni_bridge")
    }

    external fun isBackendReady(): Int
}
