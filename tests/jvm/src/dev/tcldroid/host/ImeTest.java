package dev.tcldroid.host;

/** ImeBridge against a model text buffer: what the widget would contain after each keyboard action. */
public class ImeTest {
    static int failures;

    static final class Buf implements ImeBridge.Sink {
        final StringBuilder b = new StringBuilder();
        public void chars(String s) { b.append(s); }
        public void backspace(int n) { for (int i = 0; i < n && b.length() > 0; i++) b.delete(b.offsetByCodePoints(b.length(), -1), b.length()); }
        public void deleteForward(int n) { }
        public void enter() { b.append('\n'); }
        public String toString() { return b.toString().replace("\n", "|"); }
    }

    static void check(String name, String got, String want) {
        boolean ok = got.equals(want);
        System.out.println((ok ? "PASS ime-" : "FAIL ime-") + name + (ok ? "" : " got '" + got + "' want '" + want + "'"));
        if (!ok) failures++;
    }

    public static void main(String[] a) {
        Buf buf = new Buf();
        ImeBridge ime = new ImeBridge(buf);

        ime.setComposingEnabled(true);
        ime.setComposing("h"); ime.setComposing("he"); ime.setComposing("hel");
        check("composing-shows-candidate", buf.toString(), "hel");
        ime.commit("hello ");
        check("commit-replaces-candidate", buf.toString(), "hello ");

        ime.setComposing("wor"); ime.setComposing("world");
        ime.finish();
        ime.setComposing("!");
        check("finish-keeps-text", buf.toString(), "hello world!");

        ime.finish(); ime.commit("\nnext");
        check("newline-is-enter", buf.toString(), "hello world!|next");

        // CJK: the candidate is replaced, including surrogate pairs (one Backspace per character)
        Buf b2 = new Buf(); ImeBridge i2 = new ImeBridge(b2); i2.setComposingEnabled(true);
        i2.setComposing("n"); i2.setComposing("ni"); i2.setComposing("你"); i2.commit("你好");
        check("cjk", b2.toString(), "你好");
        i2.setComposing("😀"); i2.setComposing("😀😀");
        check("emoji-pairs", b2.toString(), "你好😀😀");

        // deleting while composing shortens the candidate, not what came before it
        Buf b3 = new Buf(); ImeBridge i3 = new ImeBridge(b3); i3.setComposingEnabled(true);
        i3.commit("ab "); i3.setComposing("xyz"); i3.deleteSurrounding(1, 0); i3.setComposing("xy"); i3.commit("xy");
        check("delete-in-composition", b3.toString(), "ab xy");

        // composition off (code mode): every update is final
        Buf b4 = new Buf(); ImeBridge i4 = new ImeBridge(b4);
        i4.setComposing("p"); i4.setComposing("u"); i4.commit("ts");
        check("code-mode-direct", b4.toString(), "puts");
        i4.setComposingEnabled(true); i4.setComposing("a"); i4.setComposingEnabled(false); i4.setComposing("b");
        check("switch-off-keeps-text", b4.toString(), "putsab");

        System.exit(failures == 0 ? 0 : 1);
    }
}
