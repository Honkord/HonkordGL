package org.honkord.generated

object SpriteBinding {
    init {
        System.loadLibrary("honkord_jni_bridge")
    }

    external fun isSpriteApiReady(): Int
}
