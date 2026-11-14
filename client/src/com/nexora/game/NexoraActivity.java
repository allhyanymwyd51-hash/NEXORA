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
import android.view.inputmethod.InputMethodManager;
import android.widget.EditText;
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
    private EditText ime;
    private String capLast = "";
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
    /**
     * Soft keyboard. The engine draws its own text fields, so it owns focus; the activity
     * only raises/lowers the IME. Native input arrives through {@link EditText} (an invisible
     * 1x1 view) because a SurfaceView has no editable text connection of its own.
     */
    public void showKeyboard() {
        runOnUiThread(() -> {
            if (ime == null) {
                ime = new EditText(this);
                ime.setBackground(null);
                ime.setTextColor(0x00000000);
                ime.setAlpha(0.01f);
                ime.setSingleLine(true);
                ime.setInputType(android.text.InputType.TYPE_CLASS_TEXT
                        | android.text.InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS);
                ime.addTextChangedListener(new android.text.TextWatcher() {
                    private String last = "";
                    @Override public void beforeTextChanged(CharSequence s, int a, int b, int c) {}
                    @Override public void onTextChanged(CharSequence s, int a, int b, int c) {}
                    @Override public void afterTextChanged(android.text.Editable e) {
                        String now = e.toString();
                        if (now.length() > last.length()) {
                            capLast = now;
                            NativeBridge.textTyped(now.substring(last.length()));
                        } else if (now.length() < last.length()) {
                            NativeBridge.keyTyped(67);
                        }
                        if (now.length() > 256) {   // keep the IME buffer from growing forever
                            last = "";
                            e.clear();
                        } else {
                            last = now;
                        }
                    }
                });
                FrameLayout.LayoutParams lp = new FrameLayout.LayoutParams(1, 1);
                ime.setLayoutParams(lp);
                ((FrameLayout) ((View) surfaceView.getParent())).addView(ime);
            }
            ime.requestFocus();
            InputMethodManager imm = (InputMethodManager) getSystemService(INPUT_METHOD_SERVICE);
            if (imm != null) imm.showSoftInput(ime, InputMethodManager.SHOW_IMPLICIT);
        });
    }

    public void hideKeyboard() {
        runOnUiThread(() -> {
            InputMethodManager imm = (InputMethodManager) getSystemService(INPUT_METHOD_SERVICE);
            if (imm != null && ime != null) imm.hideSoftInputFromWindow(ime.getWindowToken(), 0);
            if (ime != null) ime.clearFocus();
        });
    }

    /** Back / enter from the IME go to the game. */
    public boolean dispatchKeyEvent(android.view.KeyEvent event) {
        if (event.getKeyCode() == android.view.KeyEvent.KEYCODE_DEL
                && event.getAction() == android.view.KeyEvent.ACTION_DOWN) {
            NativeBridge.keyTyped(67);
            return true;
        }
        if (event.getKeyCode() == android.view.KeyEvent.KEYCODE_ENTER
                && event.getAction() == android.view.KeyEvent.ACTION_DOWN) {
            NativeBridge.keyTyped(66);
            return true;
        }
        return super.dispatchKeyEvent(event);
    }

    public void requestMic() {
        if (Build.VERSION.SDK_INT >= 23 && checkSelfPermission(Manifest.permission.RECORD_AUDIO)
                != PackageManager.PERMISSION_GRANTED) {
            requestPermissions(new String[]{Manifest.permission.RECORD_AUDIO}, REQ_MIC);
        } else {
            NativeBridge.onPermissionResult("microphone", true);
        }
    }
}
