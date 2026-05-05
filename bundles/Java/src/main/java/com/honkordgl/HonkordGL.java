package com.honkordgl;

public final class HonkordGL {
    static {
        NativeLoader.load();
    }

    private HonkordGL() {}

    public static native int buildPlatform();
    public static native int videoDriverCount();

    public static native long appCreate();
    public static native void appDestroy(long appHandle);
    public static native void appSetTitle(long appHandle, String title);
    public static native void appSetClientRect(long appHandle, int x, int y, int w, int h);

    public static native long backendCreate();
    public static native void backendDestroy(long backendHandle);
    public static native boolean backendInitialize(long backendHandle, String displayOrAppName);
    public static native boolean backendCreateWindow(long backendHandle, long appHandle);
    public static native boolean backendProcessEvents(long backendHandle, long appHandle);

    public static native long rendererCreate();
    public static native void rendererDestroy(long rendererHandle);
    public static native void rendererBind(long rendererHandle, long appHandle);
    public static native int rendererCreateBestAvailable(long rendererHandle);
    public static native int rendererMakeCurrent(long rendererHandle);
    public static native void rendererClearColor(long rendererHandle, float r, float g, float b, float a);
    public static native void rendererSwapBuffers(long rendererHandle);
}
