package dev.tcldroid.host;

import android.app.Activity;
import android.app.ActivityManager;
import android.app.AlertDialog;
import android.app.ApplicationExitInfo;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.content.DialogInterface;
import android.content.SharedPreferences;
import android.graphics.Insets;
import android.os.Build;
import android.os.VibrationEffect;
import android.os.Vibrator;
import android.view.HapticFeedbackConstants;
import android.view.SoundEffectConstants;
import android.view.WindowManager;
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
import java.io.PrintWriter;
import java.io.StringWriter;
import java.nio.charset.StandardCharsets;
import java.util.List;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/**
 * Hosts Tcl/Tk. Runs the app in assets/app/ (main.tcl), which builds a Tk interface that draws into a
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
        installCrashRecorder();
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
        clipboard().addPrimaryClipChangedListener(clipListener);
        root.requestApplyInsets();
        reportPreviousFailure();
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

    /**
     * The Tcl/Tk app is the contents of assets/app/ (main.tcl and anything it needs). It is
     * copied to filesDir/app on every launch (so do NOT keep user data there: use $env(HOME),
     * which is filesDir), then main.tcl runs with that directory current and on auto_path.
     * Like wish, the app lives in the Tk event loop until "." is destroyed or [exit] is called.
     */
    private void startTcl(int w, int h) {
        File appDir = new File(getFilesDir(), "app");
        try {
            copyAsset("selftest.tcl");
            // Unpack only when this build of the app is new: copying a large app on every
            // launch is wasted start-up time. (The folder is the app's code; it is not for data.)
            File stamp = new File(getFilesDir(), "app.stamp");
            String now = buildStamp();
            if (!appDir.isDirectory() || !now.equals(readText(stamp))) {
                deleteRecursively(appDir);
                copyAssetTree("app", appDir);
                try (FileOutputStream out = new FileOutputStream(stamp)) {
                    out.write(now.getBytes(StandardCharsets.UTF_8));
                }
            }
        } catch (IOException e) {
            Log.e(TAG, "cannot unpack the app", e);
            showReport("Cannot start", "Unpacking the app failed:\n" + e);
            return;
        }
        String dir = appDir.getAbsolutePath();
        String script = "namespace eval ::tkdroid {variable appdir {" + dir + "}}\n"
                + "cd {" + dir + "}\n"
                + "lappend ::auto_path {" + dir + "} {" + dir + "/lib}\n"
                + "if {[catch {source main.tcl} err]} { return -code error $::errorInfo }\n"
                + "catch {tkwait window .}\n"
                + "exit\n";
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
        clipboard().removePrimaryClipChangedListener(clipListener);
        if (isFinishing()) {
            // A Tcl interpreter cannot be restarted inside a live process. Leave a marker so the
            // next launch knows this kill was deliberate and does not report it as a crash.
            try {
                new File(getFilesDir(), "clean_exit").createNewFile();
            } catch (IOException ignored) {
            }
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
            final String text = new String(result, StandardCharsets.UTF_8);
            runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    showReport("Tcl error", text);
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
        // The IME cursor position is not used yet.
    }

    @Override
    public byte[] getClipboard() {
        try {
            ClipData clip = clipboard().getPrimaryClip();
            if (clip == null || clip.getItemCount() == 0) {
                return null;
            }
            CharSequence text = clip.getItemAt(0).coerceToText(this);
            return text == null ? null : text.toString().getBytes(StandardCharsets.UTF_8);
        } catch (RuntimeException e) {	// e.g. SecurityException when not in the foreground
            Log.w(TAG, "cannot read the clipboard", e);
            return null;
        }
    }

    @Override
    public void setClipboard(byte[] utf8) {
        try {
            if (utf8 == null) {
                if (Build.VERSION.SDK_INT >= 28) {
                    clipboard().clearPrimaryClip();
                } else {
                    clipboard().setPrimaryClip(ClipData.newPlainText("", ""));
                }
            } else {
                clipboard().setPrimaryClip(
                        ClipData.newPlainText("Tk", new String(utf8, StandardCharsets.UTF_8)));
            }
        } catch (RuntimeException e) {
            Log.w(TAG, "cannot write the clipboard", e);
        }
    }

    @Override
    public void onTextMenu(final int x, final int y, final int flags) {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                tkView.showTextMenu(x, y, flags);
            }
        });
    }

    /** Changes whenever a different build of the app is installed. */
    private String buildStamp() {
        try {
            android.content.pm.PackageInfo pi = getPackageManager().getPackageInfo(getPackageName(), 0);
            return pi.lastUpdateTime + ":" + (Build.VERSION.SDK_INT >= 28 ? pi.getLongVersionCode()
                    : pi.versionCode);
        } catch (Exception e) {
            return "unknown:" + System.nanoTime();	// never equal: always unpack
        }
    }

    private static String readText(File f) {
        try (InputStream in = new java.io.FileInputStream(f)) {
            ByteArrayOutputStream b = new ByteArrayOutputStream();
            byte[] buf = new byte[256];
            for (int n; (n = in.read(buf)) > 0; ) b.write(buf, 0, n);
            return new String(b.toByteArray(), StandardCharsets.UTF_8);
        } catch (IOException e) {
            return "";
        }
    }

    private ClipboardManager clipboard() {
        return (ClipboardManager) getSystemService(CLIPBOARD_SERVICE);
    }

    private final ClipboardManager.OnPrimaryClipChangedListener clipListener =
            new ClipboardManager.OnPrimaryClipChangedListener() {
                @Override
                public void onPrimaryClipChanged() {
                    TclRuntime.clipboardChanged();
                }
            };

    @Override
    protected void onResume() {
        super.onResume();
        TclRuntime.clipboardChanged();	// it may have changed while we were away
        if (started) {
            TclRuntime.lifecycle(TclRuntime.LIFE_RESUME);
        }
    }

    // ---- feedback: vibration, haptics, sound -------------------------------------------------

    private Sounds sounds;

    private synchronized Sounds sounds() {
        if (sounds == null) {
            sounds = new Sounds();
        }
        return sounds;
    }

    private static String path(byte[] utf8) {
        return new String(utf8, StandardCharsets.UTF_8);
    }

    @Override
    public void onVibrate(int ms, int amplitude) {
        try {
            Vibrator v = (Vibrator) getSystemService(VIBRATOR_SERVICE);
            if (v != null && v.hasVibrator()) {
                int amp = amplitude > 0 && v.hasAmplitudeControl() ? amplitude
                        : VibrationEffect.DEFAULT_AMPLITUDE;
                v.vibrate(VibrationEffect.createOneShot(ms, amp));
            }
        } catch (RuntimeException e) {
            Log.w(TAG, "vibrate failed", e);
        }
    }

    @Override
    public void onHaptic(final int kind) {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                int c;
                switch (kind) {
                    case 0: c = HapticFeedbackConstants.CLOCK_TICK; break;
                    case 2: c = HapticFeedbackConstants.LONG_PRESS; break;
                    case 3: c = Build.VERSION.SDK_INT >= 30 ? HapticFeedbackConstants.CONFIRM
                            : HapticFeedbackConstants.CONTEXT_CLICK; break;
                    case 4: c = Build.VERSION.SDK_INT >= 30 ? HapticFeedbackConstants.REJECT
                            : HapticFeedbackConstants.LONG_PRESS; break;
                    default: c = HapticFeedbackConstants.VIRTUAL_KEY; break;
                }
                tkView.performHapticFeedback(c);
            }
        });
    }

    @Override
    public void onSoundPreload(byte[] p) {
        sounds().preload(path(p));
    }

    @Override
    public void onSoundPlay(byte[] p, int volume1000, int rate1000) {
        sounds().play(path(p), volume1000 / 1000f, rate1000 / 1000f);
    }

    @Override
    public void onSoundStop() {
        sounds().stopAll();
    }

    @Override
    public void onBell() {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                tkView.playSoundEffect(SoundEffectConstants.CLICK);
            }
        });
    }

    // ---- fullscreen, device facts, lifecycle ------------------------------------------------------

    @Override
    public void onFullscreen(final boolean on) {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                if (Build.VERSION.SDK_INT >= 28) {
                    getWindow().getAttributes().layoutInDisplayCutoutMode =
                            WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
                }
                if (Build.VERSION.SDK_INT >= 30) {
                    android.view.WindowInsetsController c = getWindow().getInsetsController();
                    if (c != null) {
                        if (on) {
                            c.hide(WindowInsets.Type.systemBars());
                            c.setSystemBarsBehavior(android.view.WindowInsetsController
                                    .BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
                        } else {
                            c.show(WindowInsets.Type.systemBars());
                        }
                    }
                } else {
                    getWindow().getDecorView().setSystemUiVisibility(on
                            ? View.SYSTEM_UI_FLAG_FULLSCREEN | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                            | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY | View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                            : View.SYSTEM_UI_FLAG_VISIBLE);
                }
            }
        });
    }

    /** ::tkdroid::info: "key TAB value LF" lines. */
    @Override
    public byte[] systemInfo() {
        StringBuilder sb = new StringBuilder();
        info(sb, "sdk", String.valueOf(Build.VERSION.SDK_INT));
        info(sb, "release", Build.VERSION.RELEASE);
        info(sb, "manufacturer", Build.MANUFACTURER);
        info(sb, "model", Build.MODEL);
        info(sb, "abi", Build.SUPPORTED_ABIS.length > 0 ? Build.SUPPORTED_ABIS[0] : "");
        info(sb, "appId", getPackageName());
        info(sb, "versionName", BuildInfo.VERSION);
        try {
            android.content.pm.PackageInfo pi = getPackageManager().getPackageInfo(getPackageName(), 0);
            info(sb, "appVersionName", pi.versionName == null ? "" : pi.versionName);
            info(sb, "appVersionCode", String.valueOf(Build.VERSION.SDK_INT >= 28
                    ? pi.getLongVersionCode() : pi.versionCode));
        } catch (Exception ignored) {
        }
        info(sb, "filesDir", getFilesDir().getAbsolutePath());
        info(sb, "cacheDir", getCacheDir().getAbsolutePath());
        info(sb, "appDir", new File(getFilesDir(), "app").getAbsolutePath());
        info(sb, "dpi", String.valueOf(getResources().getDisplayMetrics().densityDpi));
        info(sb, "locale", java.util.Locale.getDefault().toLanguageTag());
        info(sb, "darkMode", (getResources().getConfiguration().uiMode
                & android.content.res.Configuration.UI_MODE_NIGHT_MASK)
                == android.content.res.Configuration.UI_MODE_NIGHT_YES ? "1" : "0");
        return sb.toString().getBytes(StandardCharsets.UTF_8);
    }

    private static void info(StringBuilder sb, String k, String v) {
        sb.append(k).append('\t').append(v.replace('\n', ' ')).append('\n');
    }

    private volatile CountDownLatch pauseAck;

    @Override
    public void onLifecycleDone(int kind) {
        CountDownLatch l = pauseAck;
        if (kind == TclRuntime.LIFE_PAUSE && l != null) {
            l.countDown();
        }
    }

    /**
     * Tell the script it is leaving the foreground (<<AndroidPause>>/<<AndroidSaveState>>) and wait
     * briefly for it to finish saving: after onPause Android may kill the process at any time.
     */
    @Override
    protected void onPause() {
        if (started) {
            CountDownLatch l = new CountDownLatch(1);
            pauseAck = l;
            TclRuntime.lifecycle(TclRuntime.LIFE_PAUSE);
            try {
                l.await(500, TimeUnit.MILLISECONDS);
            } catch (InterruptedException ignored) {
                Thread.currentThread().interrupt();
            }
        }
        super.onPause();
    }

    @Override
    public void onTrimMemory(int level) {
        super.onTrimMemory(level);
        if (started && level >= TRIM_MEMORY_RUNNING_LOW) {
            TclRuntime.lifecycle(TclRuntime.LIFE_LOWMEMORY);
        }
    }

    @Override
    public void onKeyboard(boolean show) {
        if (show) {
            tkView.showKeyboard();
        } else {
            tkView.hideKeyboard();
        }
    }

    // ---- crash reporting (so failures can be diagnosed without adb) ------------------------------

    private void installCrashRecorder() {
        final File file = new File(getFilesDir(), "last_exception.txt");
        final Thread.UncaughtExceptionHandler previous = Thread.getDefaultUncaughtExceptionHandler();
        Thread.setDefaultUncaughtExceptionHandler(new Thread.UncaughtExceptionHandler() {
            @Override
            public void uncaughtException(Thread t, Throwable e) {
                try (FileOutputStream out = new FileOutputStream(file)) {
                    StringWriter sw = new StringWriter();
                    e.printStackTrace(new PrintWriter(sw));
                    out.write(("thread " + t.getName() + "\n" + sw).getBytes(StandardCharsets.UTF_8));
                } catch (IOException ignored) {
                }
                if (previous != null) {
                    previous.uncaughtException(t, e);
                }
            }
        });
    }

    /** Printable runs from a binary tombstone, enough to see the crash and the signal. */
    private static String printable(InputStream in, int limit) throws IOException {
        StringBuilder out = new StringBuilder(), run = new StringBuilder();
        byte[] buf = new byte[4096];
        for (int n; (n = in.read(buf)) > 0 && out.length() < limit; ) {
            for (int i = 0; i < n; i++) {
                char c = (char) (buf[i] & 0xFF);
                if (c >= 32 && c < 127) {
                    run.append(c);
                } else {
                    if (run.length() >= 6) out.append(run).append('\n');
                    run.setLength(0);
                }
            }
        }
        return out.toString();
    }

    private void reportPreviousFailure() {
        final StringBuilder sb = new StringBuilder();
        File clean = new File(getFilesDir(), "clean_exit");
        boolean deliberate = clean.exists();
        clean.delete();

        File ex = new File(getFilesDir(), "last_exception.txt");
        if (ex.isFile()) {
            try (InputStream in = new java.io.FileInputStream(ex)) {
                ByteArrayOutputStream b = new ByteArrayOutputStream();
                byte[] buf = new byte[4096];
                for (int n; (n = in.read(buf)) > 0; ) b.write(buf, 0, n);
                sb.append("Uncaught Java exception on the last run:\n")
                        .append(new String(b.toByteArray(), StandardCharsets.UTF_8)).append('\n');
            } catch (IOException ignored) {
            }
            ex.delete();
        }
        if (Build.VERSION.SDK_INT >= 30 && !deliberate) {
            ActivityManager am = (ActivityManager) getSystemService(Context.ACTIVITY_SERVICE);
            List<ApplicationExitInfo> list = am.getHistoricalProcessExitReasons(getPackageName(), 0, 1);
            SharedPreferences prefs = getSharedPreferences("crash", MODE_PRIVATE);
            if (!list.isEmpty() && list.get(0).getTimestamp() > prefs.getLong("shown", 0)) {
                ApplicationExitInfo i = list.get(0);
                int r = i.getReason();
                if (r == ApplicationExitInfo.REASON_CRASH || r == ApplicationExitInfo.REASON_CRASH_NATIVE
                        || r == ApplicationExitInfo.REASON_ANR || r == ApplicationExitInfo.REASON_SIGNALED
                        || r == ApplicationExitInfo.REASON_INITIALIZATION_FAILURE
                        || r == ApplicationExitInfo.REASON_EXCESSIVE_RESOURCE_USAGE) {
                    sb.append("Android recorded the last run as reason ").append(r)
                            .append(" (").append(i.getDescription()).append("), status ")
                            .append(i.getStatus()).append('\n');
                    try (InputStream t = i.getTraceInputStream()) {
                        if (t != null) {
                            sb.append("\ntrace:\n").append(printable(t, 3500));
                        }
                    } catch (IOException ignored) {
                    }
                }
                prefs.edit().putLong("shown", i.getTimestamp()).apply();
            }
        }
        if (sb.length() == 0) {
            return;
        }
        showReport("The previous run ended abnormally", sb.toString());
    }

    /** A dialog with selectable text and a Copy button; used for crash and Tcl error reports. */
    private void showReport(String title, String body) {
        final String report = "tcldroid " + BuildInfo.VERSION + " on " + Build.MODEL + ", Android "
                + Build.VERSION.RELEASE + " (API " + Build.VERSION.SDK_INT + ")\n\n" + body;
        Log.e(TAG, report);
        android.widget.ScrollView sv = new android.widget.ScrollView(this);
        android.widget.TextView tv = new android.widget.TextView(this);
        tv.setText(report.length() > 12000 ? report.substring(0, 12000) : report);
        tv.setTextIsSelectable(true);
        tv.setTypeface(android.graphics.Typeface.MONOSPACE);
        tv.setTextSize(12);
        tv.setPadding(32, 16, 32, 16);
        sv.addView(tv);
        new AlertDialog.Builder(this)
                .setTitle(title)
                .setView(sv)
                .setPositiveButton("Copy", new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface d, int w) {
                        ClipboardManager cm = (ClipboardManager) getSystemService(Context.CLIPBOARD_SERVICE);
                        cm.setPrimaryClip(ClipData.newPlainText("tcldroid report", report));
                    }
                })
                .setNegativeButton("Close", null)
                .show();
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

    private static void deleteRecursively(File f) {
        File[] kids = f.listFiles();
        if (kids != null) {
            for (File k : kids) deleteRecursively(k);
        }
        f.delete();
    }

    /** Copies assets/<assetPath> (a file, or a directory tree) to dest. */
    private void copyAssetTree(String assetPath, File dest) throws IOException {
        String[] names = getAssets().list(assetPath);
        if (names != null && names.length > 0) {
            if (!dest.isDirectory() && !dest.mkdirs()) {
                throw new IOException("cannot create " + dest);
            }
            for (String n : names) {
                copyAssetTree(assetPath + "/" + n, new File(dest, n));
            }
        } else {
            File parent = dest.getParentFile();
            if (parent != null && !parent.isDirectory() && !parent.mkdirs()) {
                throw new IOException("cannot create " + parent);
            }
            try (FileOutputStream out = new FileOutputStream(dest)) {
                out.write(readAsset(assetPath));
            }
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
