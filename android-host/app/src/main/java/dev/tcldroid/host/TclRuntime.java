package dev.tcldroid.host;

import android.content.Context;

import java.nio.charset.StandardCharsets;
import java.util.TimeZone;

/**
 * Starts an unmodified Tcl 9 interpreter, with Tk, on its own native thread.
 *
 * <p>The interpreter belongs to the process, not to an Activity: callers must
 * not start it again on Activity recreation.
 */
final class TclRuntime {
    /** All callbacks arrive on the Tcl thread, not the UI thread. */
    interface Listener {
        /** Raw UTF-8 bytes written to Tcl's stdout/stderr. */
        void onOutput(byte[] utf8);

        /** The script ended; {@code result} is the interpreter result (UTF-8). */
        void onDone(boolean ok, byte[] result);

        /** Tk needs a frame buffer of w*h pixels (0xAARRGGBB). Called at start and on resize. */
        int[] allocFrame(int w, int h);

        /** The rectangle [x1,x2) x [y1,y2) of the frame buffer changed. */
        void onFrame(int x1, int y1, int x2, int y2);

        /** {@code wm title .} */
        void onTitle(byte[] utf8);

        /** Back was pressed and no Tk window handles WM_DELETE_WINDOW. */
        void onBack();

        /** {@code wm iconify .} */
        void onMoveToBack();

        /** A text widget placed its insertion caret (x, y, height in pixels). */
        void onCaret(int x, int y, int height);

        /** Show or hide the soft keyboard (a text field was touched / lost focus). */
        void onKeyboard(boolean show);
    }

    static {
        // libtk9.0.so and libtcl9.0.so are DT_NEEDED dependencies of libtcldroid.so and
        // are loaded by the dynamic linker from the same directory.
        System.loadLibrary("tcldroid");
    }

    private static native boolean nativeStart(byte[] libDir, byte[] filesDir, byte[] cacheDir,
            byte[] tz, byte[] script, Listener listener, int width, int height, int dpi);

    private static native void nativeTouch(int action, int x, int y);

    private static native void nativeKey(boolean down, int keysym, int unicode, int state);

    private static native void nativeBack();

    private static native void nativeResize(int width, int height, int dpi);

    private static byte[] b(String s) {
        return s.getBytes(StandardCharsets.UTF_8);
    }

    static boolean start(Context context, String script, Listener listener, int width,
            int height, int dpi) {
        return nativeStart(
                b(context.getApplicationInfo().nativeLibraryDir),
                b(context.getFilesDir().getAbsolutePath()),
                b(context.getCacheDir().getAbsolutePath()),
                b(TimeZone.getDefault().getID()),
                b(script),
                listener, width, height, dpi);
    }

    static void touch(int action, int x, int y) { nativeTouch(action, x, y); }

    static void key(boolean down, int keysym, int unicode, int state) {
        nativeKey(down, keysym, unicode, state);
    }

    static void back() { nativeBack(); }

    static void resize(int width, int height, int dpi) { nativeResize(width, height, dpi); }

    private TclRuntime() {}
}
