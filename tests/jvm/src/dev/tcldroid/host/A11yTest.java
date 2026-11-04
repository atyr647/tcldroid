package dev.tcldroid.host;

public class A11yTest {
    static int failures;

    static void check(String name, boolean ok) {
        System.out.println((ok ? "PASS a11y-" : "FAIL a11y-") + name);
        if (!ok) failures++;
    }

    public static void main(String[] a) {
        String s = "1\t0\t0\t0\t1080\t200\twindow\te\tMy app\t\n"
                + "2\t1\t10\t10\t500\t100\tbutton\txe\tOK\t\n"
                + "3\t1\t10\t110\t500\t190\tcheckbox\txcek\tRemember me\ton\n"
                + "4\t0\t0\t300\t1080\t400\tedit\ttef\t\thello\n"
                + "garbage line\n"
                + "5\t99\t1\t1\t2\t2\ttext\te\torphan\t\n";
        A11ySnapshot snap = A11ySnapshot.parse(s);
        check("node-count-ignores-garbage", snap.nodes.size() == 5);
        check("roots-are-parentless-and-orphans", snap.roots.equals(java.util.Arrays.asList(1, 4, 5)));
        check("children-linked", snap.nodes.get(1).children.equals(java.util.Arrays.asList(2, 3)));
        A11ySnapshot.Node cb = snap.nodes.get(3);
        check("flags", cb.has('c') && cb.has('k') && cb.has('x') && !cb.has('f'));
        check("checkbox-label-is-just-the-name", cb.label().equals("Remember me"));
        check("edit-focus-and-value", snap.nodes.get(4).has('f') && snap.nodes.get(4).has('t') && snap.nodes.get(4).value.equals("hello"));
        check("bounds", snap.nodes.get(2).x2 == 500 && snap.nodes.get(2).y1 == 10);
        A11ySnapshot.Node sl = A11ySnapshot.parse("7\t0\t0\t0\t9\t9\tslider\tel\tVolume\t40").nodes.get(7);
        check("slider-label-has-value", sl.label().equals("Volume, 40"));
        check("empty-snapshot", A11ySnapshot.parse("").nodes.isEmpty());
        System.exit(failures == 0 ? 0 : 1);
    }
}
