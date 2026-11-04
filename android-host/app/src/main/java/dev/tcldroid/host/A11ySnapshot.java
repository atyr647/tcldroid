package dev.tcldroid.host;

import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

/**
 * The screen as the Tcl side describes it for accessibility (tk-android/library/a11y.tcl): one node per
 * line, tab separated: id parent x1 y1 x2 y2 role flags text value. No Android classes, so it is tested
 * on a plain JVM.
 */
final class A11ySnapshot {
    static final class Node {
        int id, parent, x1, y1, x2, y2;
        String role = "view", flags = "", text = "", value = "";
        final List<Integer> children = new ArrayList<>();

        boolean has(char flag) {
            return flags.indexOf(flag) >= 0;
        }

        /** What a screen reader should speak for this node. */
        String label() {
            if (!text.isEmpty() && !value.isEmpty() && !role.equals("edit") && !role.equals("checkbox")
                    && !role.equals("radio")) {
                return text + ", " + value;
            }
            return text;
        }
    }

    final Map<Integer, Node> nodes = new LinkedHashMap<>();
    final List<Integer> roots = new ArrayList<>();

    static A11ySnapshot parse(String s) {
        A11ySnapshot snap = new A11ySnapshot();
        for (String line : s.split("\n")) {
            String[] f = line.split("\t", -1);
            if (f.length < 8) {
                continue;
            }
            try {
                Node n = new Node();
                n.id = Integer.parseInt(f[0]);
                n.parent = Integer.parseInt(f[1]);
                n.x1 = Integer.parseInt(f[2]);
                n.y1 = Integer.parseInt(f[3]);
                n.x2 = Integer.parseInt(f[4]);
                n.y2 = Integer.parseInt(f[5]);
                n.role = f[6];
                n.flags = f[7];
                n.text = f.length > 8 ? f[8] : "";
                n.value = f.length > 9 ? f[9] : "";
                snap.nodes.put(n.id, n);
            } catch (NumberFormatException ignored) {
                // a malformed line is dropped, not fatal
            }
        }
        for (Node n : snap.nodes.values()) {
            Node p = n.parent == 0 ? null : snap.nodes.get(n.parent);
            if (p != null) {
                p.children.add(n.id);
            } else {
                snap.roots.add(n.id);
            }
        }
        return snap;
    }
}
