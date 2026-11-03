package dev.tcldroid.host;
import java.awt.*;
import java.awt.image.BufferedImage;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
/**
 * HOST TEST DOUBLE for the real TextEngine (which uses android.graphics.Paint): same
 * signatures and byte layout, implemented with java.awt so the JNI marshalling in
 * tcldroid_jni.c can be exercised on the build machine.
 */
final class TextEngine {
    private static final ArrayList<Font> FONTS = new ArrayList<>();
    private static final BufferedImage SCRATCH = new BufferedImage(1, 1, BufferedImage.TYPE_BYTE_GRAY);
    static synchronized long open(String family, int px, boolean bold, boolean italic, int[] m) {
        Font f = new Font(family == null ? "SansSerif" : family.equals("monospace") ? "Monospaced" : family.equals("serif") ? "Serif" : "SansSerif",
                (bold ? Font.BOLD : 0) | (italic ? Font.ITALIC : 0), px);
        FontMetrics fm = SCRATCH.createGraphics().getFontMetrics(f);
        m[0] = fm.getAscent(); m[1] = fm.getDescent(); m[2] = fm.getMaxAdvance(); m[3] = 0;
        FONTS.add(f);
        return FONTS.size();
    }
    private static synchronized Font font(long h) { return FONTS.get((int) h - 1); }
    static int measure(long h, byte[] utf8) {
        return SCRATCH.createGraphics().getFontMetrics(font(h)).stringWidth(new String(utf8, StandardCharsets.UTF_8));
    }
    static byte[] render(long h, byte[] utf8) {
        Font f = font(h); String s = new String(utf8, StandardCharsets.UTF_8);
        FontMetrics fm = SCRATCH.createGraphics().getFontMetrics(f);
        int w = fm.stringWidth(s) + 1, base = fm.getAscent(), hh = base + fm.getDescent() + 1;
        if (w <= 1 || hh <= 1) return null;
        BufferedImage bi = new BufferedImage(w, hh, BufferedImage.TYPE_BYTE_GRAY);
        Graphics2D g = bi.createGraphics();
        g.setRenderingHint(RenderingHints.KEY_TEXT_ANTIALIASING, RenderingHints.VALUE_TEXT_ANTIALIAS_ON);
        g.setFont(f); g.setColor(Color.WHITE); g.drawString(s, 0, base);
        byte[] out = new byte[12 + w * hh];
        ByteBuffer.wrap(out).order(ByteOrder.LITTLE_ENDIAN).putInt(w).putInt(hh).putInt(base);
        byte[] px = ((java.awt.image.DataBufferByte) bi.getRaster().getDataBuffer()).getData();
        System.arraycopy(px, 0, out, 12, w * hh);
        return out;
    }
    static String[] families() { return new String[] {"sans-serif", "serif", "monospace"}; }
}
