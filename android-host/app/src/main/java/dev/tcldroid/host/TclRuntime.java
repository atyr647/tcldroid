package dev.tcldroid.host;

import android.content.Context;

import java.nio.charset.StandardCharsets;
import java.util.TimeZone;

/**
 * Starts an unmodified Tcl 9 interpreter on its own native thread.
 *
 * <p>The interpreter belongs to the process, not to an Activity: callers must
 * not start it again on Activity recreation (rotation etc.).
 */
final class TclRuntime {
    /** Callbacks arrive on the Tcl thread, not the UI thread. */
    interface Listener {
        /** Raw UTF-8 bytes written to Tcl's stdout/stderr. */
        void onOutput(byte[] utf8);

        /** Script finished; {@code result} is the interpreter result (UTF-8). */
        void onDone(boolean ok, byte[] result);
    }

    static {
        // libtcl9.0.so is a DT_NEEDED dependency of libtcldroid.so and is
        // loaded by the dynamic linker from the same directory.
        System.loadLibrary("tcldroid");
    }

    private static native boolean nativeStart(byte[] libDir, byte[] filesDir,
            byte[] cacheDir, byte[] tz, byte[] script, Listener listener);

    private static byte[] b(String s) {
        return s.getBytes(StandardCharsets.UTF_8);
    }

    static boolean start(Context context, String script, Listener listener) {
        return nativeStart(
                b(context.getApplicationInfo().nativeLibraryDir),
                b(context.getFilesDir().getAbsolutePath()),
                b(context.getCacheDir().getAbsolutePath()),
                b(TimeZone.getDefault().getID()),
                b(script),
                listener);
    }

    private TclRuntime() {}
}
