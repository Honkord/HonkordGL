package org.honkord.internal;

public final class HonkordNative {
    private HonkordNative() {}

    private static volatile boolean loaded = false;

    public static synchronized void ensureLoaded() {
        if (loaded) return;
        String custom = System.getProperty("honkord.native.path");
        if (custom != null && !custom.isBlank()) {
            System.load(custom);
        } else {
            System.loadLibrary("honkord_jni_bridge");
        }
        loaded = true;
    }

    public static native int hgInitializeAudio(int flags);
    public static native void hgShutdownAudio();
    public static native int hgGetAudioDecoderCount();
}
