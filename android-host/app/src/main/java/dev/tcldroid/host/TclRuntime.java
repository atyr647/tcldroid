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

        /** The system clipboard's text as UTF-8, or null if it holds no text. */
        byte[] getClipboard();

        /** Replace the system clipboard's text (null: empty it). */
        void setClipboard(byte[] utf8);

        /** Show the native Cut/Copy/Paste/Select-all toolbar; flags are TKA_MENU_* bits. */
        void onTextMenu(int x, int y, int flags);

        void onFullscreen(boolean on);

        void onVibrate(int ms, int amplitude);

        /** kind: HAPTIC_* */
        void onHaptic(int kind);

        void onSoundPreload(byte[] path);

        void onSoundPlay(byte[] path, int volume1000, int rate1000);

        void onSoundStop();

        /** Tk's {@code bell}. */
        void onBell();

        /** The script has handled lifecycle event {@code kind} (LIFE_*). */
        void onLifecycleDone(int kind);

        /** Device/app facts as "key TAB value LF" lines (UTF-8). */
        byte[] systemInfo();

        /**
         * Start an asynchronous platform service (kind: pickfile, savefile, share, openurl,
         * permission, notify; args: "key TAB value LF" lines). Answer later with
         * {@link TclRuntime#result}, from any thread. Called on the Tcl thread: return quickly.
         */
        void onRequest(int id, byte[] kind, byte[] args);

        /** 1 if the Android permission is granted, else 0. */
        int checkPermission(byte[] permission);

        /** What the soft keyboard should offer: 0 code, 1 text, 2 number, 3 email, 4 url, 5 password. */
        void onInputMode(int mode);

        /** The screen as an accessibility snapshot (UTF-8 lines; see tk-android/library/a11y.tcl). */
        void onA11yTree(byte[] snapshot);
    }

    static final int LIFE_PAUSE = 0, LIFE_RESUME = 1, LIFE_LOWMEMORY = 2;

    static final int MENU_CUT = 1, MENU_COPY = 2, MENU_PASTE = 4, MENU_SELECT_ALL = 8;

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

    private static native void nativeClipboardChanged();

    private static native void nativeLifecycle(int kind);

    private static native void nativeA11yEnable(boolean on);

    private static native void nativeA11yAction(int id, int action, byte[] text);

    private static native void nativeResult(int id, byte[] status, byte[] payload);

    private static native void nativeLongPress(int x, int y);

    private static native void nativePinch(int phase, int x, int y, int scale1000);

    private static native void nativeTextAction(int action);

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

    static void clipboardChanged() { nativeClipboardChanged(); }

    static void lifecycle(int kind) { nativeLifecycle(kind); }

    /** A screen reader (or other accessibility service) started or stopped. */
    static void a11yEnable(boolean on) { nativeA11yEnable(on); }

    /** The screen reader asked for an action on a node; action is an AccessibilityNodeInfo constant. */
    static void a11yAction(int id, int action, String text) { nativeA11yAction(id, action, b(text == null ? "" : text)); }

    /** Answer to Listener.onRequest. status: ok, error, cancelled or denied; payload: newline-separated items. */
    static void result(int id, String status, String payload) {
        nativeResult(id, b(status), b(payload == null ? "" : payload));
    }

    static void longPress(int x, int y) { nativeLongPress(x, y); }

    /** phase 0 start, 1 update, 2 end; scale is cumulative (1.0 = unchanged). */
    static void pinch(int phase, int x, int y, float scale) {
        nativePinch(phase, x, y, Math.round(scale * 1000f));
    }

    static void textAction(int action) { nativeTextAction(action); }

    private TclRuntime() {}
}
