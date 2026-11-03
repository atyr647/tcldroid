package dev.tcldroid.host;

import android.app.Activity;
import android.graphics.Insets;
import android.os.Build;
import android.os.Bundle;
import android.os.Process;
import android.util.Log;
import android.view.View;
import android.view.WindowInsets;
import android.widget.FrameLayout;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;

/**
 * Hosts Tcl/Tk. Runs assets/demo.tcl, which builds a Tk interface that draws into a
 * {@link TkView}. The interpreter is process-lifetime state (Activity recreation on
 * rotation is avoided with configChanges in the manifest).
 */
public class MainActivity extends Activity implements TclRuntime.Listener, TkView.SizeListener {
    private static final String TAG = "tcldroid";
    private static boolean started;

    private TkView tkView;

    @Override
    protected void onCreate(Bundle state) {
        super.onCreate(state);
        FrameLayout root = new FrameLayout(this);
        tkView = new TkView(this);
        tkView.setSizeListener(this);
        root.addView(tkView, new FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT, FrameLayout.LayoutParams.MATCH_PARENT));
        // Keep Tk clear of the status/navigation bars and the soft keyboard.
        root.setOnApplyWindowInsetsListener(new View.OnApplyWindowInsetsListener() {
            @Override
            public WindowInsets onApplyWindowInsets(View v, WindowInsets insets) {
                int l, t, r, b;
                if (Build.VERSION.SDK_INT >= 30) {
                    Insets i = insets.getInsets(WindowInsets.Type.systemBars() | WindowInsets.Type.ime());
                    l = i.left; t = i.top; r = i.right; b = i.bottom;
                } else {
                    l = insets.getSystemWindowInsetLeft(); t = insets.getSystemWindowInsetTop();
                    r = insets.getSystemWindowInsetRight(); b = insets.getSystemWindowInsetBottom();
                }
                v.setPadding(l, t, r, b);
                return insets;
            }
        });
        setContentView(root);
        root.requestApplyInsets();
    }

    @Override
    public void onSizeKnown(int w, int h) {
        if (!started) {
            started = true;
            startTcl(w, h);
        } else {
            TclRuntime.resize(w, h, tkView.dpi());
        }
    }

    private void startTcl(int w, int h) {
        String script;
        try {
            copyAsset("selftest.tcl");
            script = new String(readAsset("demo.tcl"), StandardCharsets.UTF_8);
        } catch (IOException e) {
            Log.e(TAG, "cannot read assets", e);
            return;
        }
        if (!TclRuntime.start(this, script, this, w, h, tkView.dpi())) {
            Log.e(TAG, "failed to start Tcl thread");
        }
    }

    @Override
    public void onBackPressed() {
        TclRuntime.back();	// Tk runs WM_DELETE_WINDOW, or calls onBack() if nobody handles it
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        if (isFinishing()) {
            // A Tcl interpreter cannot be restarted inside a live process.
            Process.killProcess(Process.myPid());
        }
    }

    // ---- TclRuntime.Listener (Tcl thread) -------------------------------------------------

    @Override
    public void onOutput(byte[] utf8) {
        // stdout/stderr already go to logcat (tag tcldroid) from the native side.
    }

    @Override
    public void onDone(boolean ok, byte[] result) {
        Log.i(TAG, "script " + (ok ? "ended" : "ERROR") + ": " + new String(result, StandardCharsets.UTF_8));
        if (!ok) {
            runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    android.widget.Toast.makeText(MainActivity.this,
                            "Tcl error (see logcat: tcldroid)", android.widget.Toast.LENGTH_LONG).show();
                }
            });
        }
    }

    @Override
    public int[] allocFrame(int w, int h) {
        return tkView.allocFrame(w, h);
    }

    @Override
    public void onFrame(int x1, int y1, int x2, int y2) {
        tkView.onFrame(x1, y1, x2, y2);
    }

    @Override
    public void onTitle(final byte[] utf8) {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                setTitle(new String(utf8, StandardCharsets.UTF_8));
            }
        });
    }

    @Override
    public void onBack() {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                finish();
            }
        });
    }

    @Override
    public void onMoveToBack() {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                moveTaskToBack(true);
            }
        });
    }

    @Override
    public void onCaret(int x, int y, int height) {
        tkView.showKeyboard();
    }

    // ---- assets --------------------------------------------------------------------------------

    private byte[] readAsset(String name) throws IOException {
        try (InputStream in = getAssets().open(name)) {
            ByteArrayOutputStream out = new ByteArrayOutputStream();
            byte[] buf = new byte[8192];
            for (int n; (n = in.read(buf)) > 0; ) {
                out.write(buf, 0, n);
            }
            return out.toByteArray();
        }
    }

    /** Copies an asset to filesDir/assets/ so Tcl scripts can read it as a plain file. */
    private void copyAsset(String name) throws IOException {
        File dir = new File(getFilesDir(), "assets");
        if (!dir.isDirectory() && !dir.mkdirs()) {
            throw new IOException("cannot create " + dir);
        }
        try (FileOutputStream out = new FileOutputStream(new File(dir, name))) {
            out.write(readAsset(name));
        }
    }
}
