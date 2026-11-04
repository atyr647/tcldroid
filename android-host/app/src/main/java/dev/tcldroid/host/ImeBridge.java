package dev.tcldroid.host;

/**
 * Turns the soft keyboard's text editing (composition, suggestions, swipe typing, CJK input) into the
 * key events Tk understands. Tk's X11 widgets have no pre-edit text, so composition is done inline: while
 * the keyboard is composing, its current candidate is real text in the widget, and each update replaces the
 * previous one (Backspace the old candidate, type the new one); committing keeps what is there.
 *
 * No Android classes here, so it is tested on a plain JVM (tests/jvm-jni.sh).
 */
final class ImeBridge {
    interface Sink {
        void chars(String s);	// type these characters

        void backspace(int n);	// Backspace n times

        void deleteForward(int n);

        void enter();
    }

    private final Sink sink;
    private boolean composingEnabled;
    private int composingLen;		// code points of the candidate currently in the widget

    ImeBridge(Sink sink) {
        this.sink = sink;
    }

    /** Composition on: ordinary text keyboards. Off: every update is final (code mode, passwords, numbers). */
    void setComposingEnabled(boolean on) {
        if (!on) {
            composingLen = 0;
        }
        composingEnabled = on;
    }

    private static int cp(String s) {
        return s.codePointCount(0, s.length());
    }

    private void type(String text) {
        int start = 0;
        for (int i = 0; i < text.length(); i++) {
            if (text.charAt(i) == '\n') {
                if (i > start) {
                    sink.chars(text.substring(start, i));
                }
                sink.enter();
                start = i + 1;
            }
        }
        if (start < text.length()) {
            sink.chars(text.substring(start));
        }
    }

    void setComposing(String text) {
        if (!composingEnabled) {
            commit(text);
            return;
        }
        if (composingLen > 0) {
            sink.backspace(composingLen);
        }
        type(text);
        composingLen = cp(text);
    }

    void commit(String text) {
        if (composingLen > 0) {
            sink.backspace(composingLen);
            composingLen = 0;
        }
        type(text);
    }

    /** The keyboard stops composing and keeps the text as it is. */
    void finish() {
        composingLen = 0;
    }

    void deleteSurrounding(int before, int after) {
        if (before > 0) {
            sink.backspace(before);
            composingLen = Math.max(0, composingLen - before);
        }
        if (after > 0) {
            sink.deleteForward(after);
        }
    }
}
