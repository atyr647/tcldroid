package dev.tcldroid.host;
import android.content.Context;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;
/** Drives the REAL TclRuntime + tcldroid_jni.c on a host JVM (no Android): see tests/jvm-jni.sh. */
public class Harness implements TclRuntime.Listener {
    int[] frame; int fw, fh, frames; volatile boolean done; boolean ok;
    public int[] allocFrame(int w, int h) { fw = w; fh = h; frame = new int[w * h]; System.err.println("[java] allocFrame " + w + "x" + h); return frame; }
    public void onFrame(int x1, int y1, int x2, int y2) { frames++;
        if (frames == 1) { try { ppm(System.getProperty("first.ppm", "/tmp/first.ppm")); } catch (IOException e) { throw new RuntimeException(e); } } if (frames <= 3) System.err.println("[java] onFrame " + x1 + "," + y1 + " " + x2 + "," + y2); }
    public void onOutput(byte[] b) { System.err.print("[tcl] " + new String(b, StandardCharsets.UTF_8)); }
    public void onDone(boolean ok, byte[] r) { this.ok = ok; done = true; System.err.println("[java] onDone ok=" + ok + " result=" + new String(r, StandardCharsets.UTF_8)); }
    public void onTitle(byte[] t) { System.err.println("[java] title=" + new String(t, StandardCharsets.UTF_8)); }
    public void onBack() { System.err.println("[java] onBack (unhandled)"); }
    public void onMoveToBack() { }
    public void onKeyboard(boolean show) { System.err.println("[java] KEYBOARD " + (show ? "SHOW" : "HIDE")); }
    String clip = null;
    public byte[] getClipboard() { return clip == null ? null : clip.getBytes(StandardCharsets.UTF_8); }
    public void setClipboard(byte[] b) { clip = b == null ? null : new String(b, StandardCharsets.UTF_8); System.err.println("[java] clipboard <- " + clip); }
    public void onTextMenu(int x, int y, int flags) { System.err.println("[java] TEXTMENU " + x + "," + y + " flags=" + flags); }
    public void onFullscreen(boolean on) { System.err.println("[java] FULLSCREEN " + on); }
    public void onVibrate(int ms, int amp) { System.err.println("[java] VIBRATE " + ms + " amp=" + amp); }
    public void onHaptic(int kind) { System.err.println("[java] HAPTIC " + kind); }
    public void onSoundPreload(byte[] p) { System.err.println("[java] SOUND preload " + new String(p, StandardCharsets.UTF_8)); }
    public void onSoundPlay(byte[] p, int v, int r) { System.err.println("[java] SOUND play " + new String(p, StandardCharsets.UTF_8) + " vol=" + v + " rate=" + r); }
    public void onSoundStop() { System.err.println("[java] SOUND stop"); }
    public void onBell() { System.err.println("[java] BELL"); }
    public void onLifecycleDone(int kind) { System.err.println("[java] LIFECYCLE done " + kind); }
    public byte[] systemInfo() { return "sdk\t35\nmodel\tHarness\nfilesDir\t/x y\n".getBytes(StandardCharsets.UTF_8); }
    public volatile String lastRequest = "";
    public void onRequest(int id, byte[] kind, byte[] args) {
        lastRequest = id + " " + new String(kind, StandardCharsets.UTF_8);
        System.err.println("[java] REQUEST " + id + " " + new String(kind, StandardCharsets.UTF_8) + " | " + new String(args, StandardCharsets.UTF_8).replace("\n", " ; ").replace("\t", "="));
    }
    public void onInputMode(int m) { System.err.println("[java] INPUTMODE " + m); }
    public int checkPermission(byte[] p) { return new String(p, StandardCharsets.UTF_8).endsWith("VIBRATE") ? 1 : 0; }
    public void onCaret(int x, int y, int h) { System.err.println("[java] caret " + x + "," + y + " h=" + h); }
    void ppm(String path) throws IOException {
        try (OutputStream o = new BufferedOutputStream(new FileOutputStream(path))) {
            o.write(("P6\n" + fw + " " + fh + "\n255\n").getBytes());
            for (int p : frame) { o.write((p >> 16) & 255); o.write((p >> 8) & 255); o.write(p & 255); }
        }
    }
    public static void main(String[] a) throws Exception {
        String lib = a[0], files = a[1], script = new String(Files.readAllBytes(Paths.get(a[2])), StandardCharsets.UTF_8), out = a[3];
        Context.nativeLibDir = lib; Context.filesDir = files; Context.cacheDir = files;
        Harness h = new Harness();
        if (!TclRuntime.start(new Context(), script, h, 1080, 2200, 420)) throw new RuntimeException("start failed");
        Thread.sleep(2500);
        System.err.println("[java] frames so far: " + h.frames);
        TclRuntime.touch(0, 540, 260); Thread.sleep(80); TclRuntime.touch(1, 540, 260);   // tap the first button
        Thread.sleep(800);
        System.err.println("[java] --- tap the canvas (no keyboard expected), then the entry (SHOW), then the canvas (HIDE)");
        TclRuntime.touch(0, 540, 800); Thread.sleep(60); TclRuntime.touch(1, 540, 800); Thread.sleep(500);
        TclRuntime.touch(0, 540, 395); Thread.sleep(60); TclRuntime.touch(1, 540, 395); Thread.sleep(700);
        TclRuntime.touch(0, 540, 800); Thread.sleep(60); TclRuntime.touch(1, 540, 800); Thread.sleep(700);
        TclRuntime.touch(0, 540, 395); Thread.sleep(60); TclRuntime.touch(1, 540, 395); Thread.sleep(500);
        TclRuntime.key(true, 0, 'h', 0); TclRuntime.key(false, 0, 'h', 0);
        TclRuntime.resize(1080, 1500, 420);                                                 // keyboard-style shrink
        Thread.sleep(1200);
        TclRuntime.resize(1080, 2200, 420);
        Thread.sleep(800);
        h.ppm(out);
        TclRuntime.back();
        Thread.sleep(500);
        System.err.println("[java] finished; frames=" + h.frames + " done=" + h.done);
        System.exit(0);
    }
}
