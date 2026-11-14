package com.nexora.game;

import android.Manifest;
import android.app.Activity;
import android.content.pm.PackageManager;
import android.content.res.Configuration;
import android.os.Build;
import android.os.Bundle;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import android.widget.FrameLayout;

/**
 * NEXORA — single activity host.
 *
 * The game surface is a native EGL/OpenGL ES 3.0 view. No WebView, no HTML/CSS/JS: this is the
 * Android equivalent of the Unreal Engine GameActivity shell (native_app_glue), hosting a C++
 * engine that renders the entire product.
 */
public class NexoraActivity extends Activity {
    static {
        System.loadLibrary("nexora");
    }

    private NexoraSurfaceView surfaceView;
    private static final int REQ_MIC = 7101;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        requestWindowFeature(Window.FEATURE_NO_TITLE);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        getWindow().getDecorView().setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                        | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                        | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                        | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                        | View.SYSTEM_UI_FLAG_FULLSCREEN
                        | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY);

        FrameLayout root = new FrameLayout(this);
        surfaceView = new NexoraSurfaceView(this);
        root.addView(surfaceView, new FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT, FrameLayout.LayoutParams.MATCH_PARENT));
        setContentView(root);

        // Pass the asset root so the engine can load shaders/fonts/audio from the APK.
        NativeBridge.setAssetManager(getAssets());
        NativeBridge.setCacheDir(getCacheDir().getAbsolutePath());
        NativeBridge.setFilesDir(getFilesDir().getAbsolutePath());
        NativeBridge.setActivity(this);
    }

    @Override
    protected void onResume() {
        super.onResume();
        surfaceView.onResumeHost();
    }

    @Override
    protected void onPause() {
        surfaceView.onPauseHost();
        super.onPause();
    }

    @Override
    protected void onDestroy() {
        surfaceView.onDestroyHost();
        super.onDestroy();
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        surfaceView.onConfigChanged(newConfig.orientation);
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        if (requestCode == REQ_MIC) {
            boolean granted = grantResults.length > 0 && grantResults[0] == PackageManager.PERMISSION_GRANTED;
            NativeBridge.onPermissionResult("microphone", granted);
        }
    }

    /** Called from native code (UI thread) when the player enables voice chat. */
    public void requestMic() {
        if (Build.VERSION.SDK_INT >= 23 && checkSelfPermission(Manifest.permission.RECORD_AUDIO)
                != PackageManager.PERMISSION_GRANTED) {
            requestPermissions(new String[]{Manifest.permission.RECORD_AUDIO}, REQ_MIC);
        } else {
            NativeBridge.onPermissionResult("microphone", true);
        }
    }
}
