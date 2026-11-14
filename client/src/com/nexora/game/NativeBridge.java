package com.nexora.game;

import android.content.res.AssetManager;

/** Thin JNI bridge between the Android shell and the native engine. */
public final class NativeBridge {
    private NativeBridge() {}

    public static native void setAssetManager(AssetManager am);
    public static native void setCacheDir(String path);
    public static native void setFilesDir(String path);
    public static native void setActivity(Object activity);
    public static native void onPermissionResult(String permission, boolean granted);

    // Surface lifecycle
    public static native void surfaceCreated(Object surface);
    public static native void surfaceChanged(Object surface, int width, int height, int orientation);
    public static native void surfaceDestroyed();
    public static native void pause();
    public static native void resume();

    // Input (coordinates are already normalized 0..1 by the view, density-independent)
    public static native void touchDown(int id, float x, float y, long time);
    public static native void touchMove(int id, float x, float y, long time);
    public static native void touchUp(int id, float x, float y, long time);
    public static native void key(int code, boolean down);
    // IME text: typed UTF-8 characters, and key codes (67 = backspace, 66 = enter)
    public static native void textTyped(String utf8);
    public static native void keyTyped(int code);

    // Audio sample rate negotiation for the native mixer
    public static native void setAudioSampleRate(int rate);
    public static native void setAudioFramesPerBuffer(int frames);
}
