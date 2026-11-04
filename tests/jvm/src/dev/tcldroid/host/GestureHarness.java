package dev.tcldroid.host;
import android.content.Context;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;
/** Clipboard, long-press, toolbar action and pinch through the real JNI layer (see tests/jvm-jni.sh). */
public class GestureHarness {
    public static void main(String[] a) throws Exception {
        String lib = a[0], files = a[1], script = new String(Files.readAllBytes(Paths.get(a[2])), StandardCharsets.UTF_8);
        Context.nativeLibDir = lib; Context.filesDir = files; Context.cacheDir = files;
        Harness h = new Harness();
        if (!TclRuntime.start(new Context(), script, h, 1080, 2200, 420)) throw new RuntimeException("start failed");
        Thread.sleep(2500);
        System.err.println("[java] --- platform services: answer the requests the script made");
        TclRuntime.result(1, "ok", "/cache/picked/1/a.png\n/cache/picked/1/b b.png"); Thread.sleep(150);
        TclRuntime.result(2, "error", "no app can open this link"); Thread.sleep(150);
        TclRuntime.result(3, "denied", "android.permission.VIBRATE"); Thread.sleep(150);
        TclRuntime.result(4, "ok", ""); Thread.sleep(150);
        TclRuntime.result(5, "cancelled", ""); Thread.sleep(150);
        TclRuntime.result(99, "ok", "nobody asked"); Thread.sleep(150);	// unknown id: ignored
        System.err.println("[java] --- tap Probe: Tk's own clipboard text must be on the system clipboard");
        tap(160, 450);
        System.err.println("[java] --- another app copies: Tk must see it");
        h.clip = "other app";
        TclRuntime.clipboardChanged(); Thread.sleep(300);
        tap(160, 450);
        System.err.println("[java] --- long press in the entry: word selected, toolbar after the lift");
        TclRuntime.touch(0, 120, 150); Thread.sleep(80);
        TclRuntime.longPress(120, 150); Thread.sleep(200);
        TclRuntime.touch(1, 120, 150); Thread.sleep(500);
        System.err.println("[java] --- toolbar: Copy, then Probe shows what is on the clipboard");
        TclRuntime.textAction(TclRuntime.MENU_COPY); Thread.sleep(300);
        tap(160, 450);
        System.err.println("[java] --- pinch");
        TclRuntime.pinch(0, 540, 800, 1f); TclRuntime.pinch(1, 540, 800, 1.5f); TclRuntime.pinch(2, 540, 800, 2f);
        Thread.sleep(400);
        System.err.println("[java] --- two fingers cancel the first (no click on Probe)");
        TclRuntime.touch(0, 160, 450); Thread.sleep(50); TclRuntime.touch(3, 160, 450); Thread.sleep(300);
        System.err.println("[java] --- lifecycle");
        TclRuntime.lifecycle(TclRuntime.LIFE_PAUSE); Thread.sleep(300);
        TclRuntime.lifecycle(TclRuntime.LIFE_RESUME); Thread.sleep(300);
        TclRuntime.lifecycle(TclRuntime.LIFE_LOWMEMORY); Thread.sleep(300);
        System.err.println("[java] done");
        System.exit(0);
    }
    static void tap(int x, int y) throws Exception {
        TclRuntime.touch(0, x, y); Thread.sleep(60); TclRuntime.touch(1, x, y); Thread.sleep(400);
    }
}
