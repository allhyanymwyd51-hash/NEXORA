package com.nexora.game;

import android.content.Context;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.view.MotionEvent;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

/**
 * EGL surface host. All rendering happens on the native render thread; this class only forwards
 * surface lifecycle, touch input and display metrics.
 */
public class NexoraSurfaceView extends SurfaceView implements SurfaceHolder.Callback2 {
    private static final String TAG = "NexoraSurface";
    private final Handler main = new Handler(Looper.getMainLooper());
    private boolean destroyed = false;

    public NexoraSurfaceView(Context context) {
        super(context);
        getHolder().addCallback(this);
        setFocusable(true);
        setFocusableInTouchMode(true);
        setKeepScreenOn(true);
    }

    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        NativeBridge.surfaceCreated(holder.getSurface());
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
        int orientation = getResources().getConfiguration().orientation;
        NativeBridge.surfaceChanged(holder.getSurface(), width, height, orientation);
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        NativeBridge.surfaceDestroyed();
    }

    @Override
    public void surfaceRedrawNeeded(SurfaceHolder holder) {
        // handled by the render loop
    }

    void onResumeHost() {
        if (!destroyed) NativeBridge.resume();
    }

    void onPauseHost() {
        if (!destroyed) NativeBridge.pause();
    }

    void onDestroyHost() {
        destroyed = true;
    }

    void onConfigChanged(int orientation) {
        if (getHolder().getSurface() != null && getHolder().getSurface().isValid()) {
            NativeBridge.surfaceChanged(getHolder().getSurface(), getWidth(), getHeight(), orientation);
        }
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        final int w = Math.max(1, getWidth());
        final int h = Math.max(1, getHeight());
        final int action = event.getActionMasked();
        switch (action) {
            case MotionEvent.ACTION_DOWN:
            case MotionEvent.ACTION_POINTER_DOWN: {
                int idx = event.getActionIndex();
                NativeBridge.touchDown(event.getPointerId(idx), event.getX(idx) / w, event.getY(idx) / h,
                        event.getEventTime());
                break;
            }
            case MotionEvent.ACTION_MOVE: {
                for (int i = 0; i < event.getPointerCount(); i++) {
                    NativeBridge.touchMove(event.getPointerId(i), event.getX(i) / w, event.getY(i) / h,
                            event.getEventTime());
                }
                break;
            }
            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_POINTER_UP: {
                int idx = event.getActionIndex();
                NativeBridge.touchUp(event.getPointerId(idx), event.getX(idx) / w, event.getY(idx) / h,
                        event.getEventTime());
                break;
            }
            case MotionEvent.ACTION_CANCEL: {
                for (int i = 0; i < event.getPointerCount(); i++) {
                    NativeBridge.touchUp(event.getPointerId(i), event.getX(i) / w, event.getY(i) / h,
                            event.getEventTime());
                }
                break;
            }
            default:
                break;
        }
        return true;
    }

    public void requestMicPermission() {
        main.post(() -> {
            try {
                ((NexoraActivity) getContext()).requestMic();
            } catch (Throwable t) {
                Log.w(TAG, "mic request failed", t);
            }
        });
    }
}
