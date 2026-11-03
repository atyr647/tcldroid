package dev.tcldroid.host;

import android.app.Activity;
import android.graphics.Typeface;
import android.os.Build;
import android.os.Bundle;
import android.util.Log;
import android.widget.ScrollView;
import android.widget.TextView;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;

/**
 * Runs assets/selftest.tcl once per process and shows its output.
 *
 * <p>The log is kept in static state so that Activity recreation re-renders it
 * instead of restarting the interpreter.
 */
public class MainActivity extends Activity {
    private static final String TAG = "tcldroid";
    private static final StringBuilder LOG = new StringBuilder();
    private static boolean started;
    private static MainActivity current;

    private TextView view;
    private ScrollView scroll;

    @Override
    protected void onCreate(Bundle state) {
        super.onCreate(state);
        scroll = new ScrollView(this);
        view = new TextView(this);
        view.setTypeface(Typeface.MONOSPACE);
        view.setTextSize(12);
        view.setPadding(24, 24, 24, 24);
        view.setTextIsSelectable(true);
        scroll.addView(view);
        setContentView(scroll);

        synchronized (LOG) {
            current = this;
            view.setText(LOG);
            if (started) {
                return;
            }
            started = true;
        }
        run();
    }

    @Override
    protected void onDestroy() {
        synchronized (LOG) {
            if (current == this) {
                current = null;
            }
        }
        super.onDestroy();
    }

    private void run() {
        append("tcldroid host 0.1.0\n"
                + Build.MANUFACTURER + " " + Build.MODEL + ", Android " + Build.VERSION.RELEASE
                + " (API " + Build.VERSION.SDK_INT + "), ABIs " + java.util.Arrays.toString(Build.SUPPORTED_ABIS)
                + "\n\n");
        final String script;
        try {
            script = readAsset("selftest.tcl");
        } catch (IOException e) {
            append("cannot read selftest.tcl: " + e + "\n");
            return;
        }
        boolean ok = TclRuntime.start(this, script, new TclRuntime.Listener() {
            @Override
            public void onOutput(byte[] utf8) {
                append(new String(utf8, StandardCharsets.UTF_8));
            }

            @Override
            public void onDone(boolean ok, byte[] result) {
                String r = new String(result, StandardCharsets.UTF_8);
                Log.i(TAG, "script " + (ok ? "ok" : "ERROR") + ": " + r);
                append((ok ? "\n[done] " : "\n[script error] ") + r + "\n");
            }
        });
        if (!ok) {
            append("failed to start Tcl thread\n");
        }
    }

    private void append(final String s) {
        synchronized (LOG) {
            LOG.append(s);
            final MainActivity a = current;
            if (a != null) {
                a.runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        a.view.setText(LOG.toString());
                        a.scroll.post(new Runnable() {
                            @Override
                            public void run() {
                                a.scroll.fullScroll(ScrollView.FOCUS_DOWN);
                            }
                        });
                    }
                });
            }
        }
    }

    private String readAsset(String name) throws IOException {
        try (InputStream in = getAssets().open(name)) {
            ByteArrayOutputStream out = new ByteArrayOutputStream();
            byte[] buf = new byte[4096];
            for (int n; (n = in.read(buf)) > 0; ) {
                out.write(buf, 0, n);
            }
            return new String(out.toByteArray(), StandardCharsets.UTF_8);
        }
    }
}
