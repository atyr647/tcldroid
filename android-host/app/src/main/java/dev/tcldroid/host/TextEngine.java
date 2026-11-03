package dev.tcldroid.host;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Typeface;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Locale;

/**
 * Text for Tk, on the platform's own text stack (android.graphics.Paint). Called
 * from the Tcl thread through JNI (tcldroid_jni.c); Tk asks for fonts by name,
 * size and style, measures advances, and renders glyph runs into alpha masks
 * that the C rasteriser blends.
 */
final class TextEngine {
    private static final ArrayList<Paint> FONTS = new ArrayList<>();

    private TextEngine() {}

    /** Tk family names (Helvetica, Courier, ...) mapped to Android's generic families. */
    private static String androidFamily(String family) {
        if (family == null) {
            return "sans-serif";
        }
        String f = family.toLowerCase(Locale.ROOT);
        if (f.contains("mono") || f.contains("courier") || f.contains("fixed") || f.contains("typewriter")) {
            return "monospace";
        }
        if ((f.contains("serif") && !f.contains("sans")) || f.contains("times") || f.contains("georgia")) {
            return "serif";
        }
        return "sans-serif";
    }

    /** Returns a font handle (&gt; 0) and fills metrics = {ascent, descent, maxWidth, fixed}. */
    static synchronized long open(String family, int px, boolean bold, boolean italic, int[] metrics) {
        int style = (bold ? Typeface.BOLD : 0) | (italic ? Typeface.ITALIC : 0);
        Paint p = new Paint(Paint.ANTI_ALIAS_FLAG | Paint.SUBPIXEL_TEXT_FLAG);
        p.setTypeface(Typeface.create(androidFamily(family), style));
        p.setTextSize(px);
        Paint.FontMetrics fm = p.getFontMetrics();
        metrics[0] = (int) Math.ceil(-fm.ascent);
        metrics[1] = (int) Math.ceil(fm.descent);
        metrics[2] = (int) Math.ceil(p.measureText("W"));
        metrics[3] = Math.abs(p.measureText("i") - p.measureText("W")) < 0.5f ? 1 : 0;
        FONTS.add(p);
        return FONTS.size();
    }

    private static Paint paint(long handle) {
        synchronized (TextEngine.class) {
            return FONTS.get((int) handle - 1);
        }
    }

    static int measure(long handle, byte[] utf8) {
        return Math.round(paint(handle).measureText(new String(utf8, StandardCharsets.UTF_8)));
    }

    /**
     * Renders a string. Returns {w, h, baseline} as three little-endian ints followed
     * by w*h bytes of alpha (row-major), or null if there is nothing to draw.
     */
    static byte[] render(long handle, byte[] utf8) {
        Paint p = paint(handle);
        String s = new String(utf8, StandardCharsets.UTF_8);
        Paint.FontMetrics fm = p.getFontMetrics();
        int w = (int) Math.ceil(p.measureText(s)) + 1;
        int base = (int) Math.ceil(-fm.ascent);
        int h = base + (int) Math.ceil(fm.descent) + 1;
        if (w <= 1 || h <= 1) {
            return null;
        }
        Bitmap bm = Bitmap.createBitmap(w, h, Bitmap.Config.ALPHA_8);
        Canvas c = new Canvas(bm);
        c.drawText(s, 0, base, p);
        ByteBuffer raw = ByteBuffer.allocate(bm.getRowBytes() * h);
        bm.copyPixelsToBuffer(raw);
        bm.recycle();
        byte[] out = new byte[12 + w * h];
        ByteBuffer hdr = ByteBuffer.wrap(out).order(ByteOrder.LITTLE_ENDIAN);
        hdr.putInt(w).putInt(h).putInt(base);
        int stride = raw.capacity() / h;
        for (int y = 0; y < h; y++) {
            raw.position(y * stride);
            raw.get(out, 12 + y * w, w);
        }
        return out;
    }

    static String[] families() {
        return new String[] {"sans-serif", "serif", "monospace", "sans-serif-condensed",
                "sans-serif-medium", "cursive"};
    }
}
