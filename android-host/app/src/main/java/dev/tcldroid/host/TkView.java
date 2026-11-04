package dev.tcldroid.host;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Rect;
import android.text.InputType;
import android.view.ActionMode;
import android.view.GestureDetector;
import android.view.HapticFeedbackConstants;
import android.view.KeyEvent;
import android.view.Menu;
import android.view.MenuItem;
import android.view.MotionEvent;
import android.view.ScaleGestureDetector;
import android.view.View;
import android.view.accessibility.AccessibilityEvent;
import android.view.accessibility.AccessibilityManager;
import android.view.accessibility.AccessibilityNodeInfo;
import android.view.accessibility.AccessibilityNodeProvider;
import android.os.Build;
import android.os.Bundle;
import android.view.inputmethod.BaseInputConnection;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;
import android.view.inputmethod.InputMethodManager;

/**
 * The surface Tk draws on. Tk renders into an int[] frame buffer in C; this view copies
 * changed rectangles into a Bitmap and shows it, and forwards touch, key and soft-keyboard
 * input to the Tcl thread.
 */
final class TkView extends View {
    interface SizeListener {
        void onSizeKnown(int w, int h);
    }

    // X keysyms for the special keys Tk's default bindings use.
    private static final int XK_BACKSPACE = 0xFF08, XK_TAB = 0xFF09, XK_RETURN = 0xFF0D,
            XK_ESCAPE = 0xFF1B, XK_HOME = 0xFF50, XK_LEFT = 0xFF51, XK_UP = 0xFF52,
            XK_RIGHT = 0xFF53, XK_DOWN = 0xFF54, XK_PRIOR = 0xFF55, XK_NEXT = 0xFF56,
            XK_END = 0xFF57, XK_INSERT = 0xFF63, XK_DELETE = 0xFFFF;

    private final Object lock = new Object();
    private Bitmap bitmap;
    private int[] frame;
    private int frameW, frameH;
    private SizeListener sizeListener;

    // Gestures (see tk-android/library/android.tcl for what Tk does with them).
    private final GestureDetector longPress;
    private final ScaleGestureDetector pinch;
    private boolean multiTouch;		// two or more fingers down: Tk's pointer is not used
    private float pinchScale = 1f;
    private ActionMode textMenu;
    private int menuX, menuY, menuFlags;

    TkView(Context context) {
        super(context);
        setFocusable(true);
        setFocusableInTouchMode(true);
        longPress = new GestureDetector(context, new GestureDetector.SimpleOnGestureListener() {
            @Override
            public void onLongPress(MotionEvent e) {
                performHapticFeedback(HapticFeedbackConstants.LONG_PRESS);
                TclRuntime.longPress((int) e.getX(), (int) e.getY());
            }
        });
        pinch = new ScaleGestureDetector(context, new ScaleGestureDetector.SimpleOnScaleGestureListener() {
            @Override
            public boolean onScaleBegin(ScaleGestureDetector d) {
                pinchScale = 1f;
                TclRuntime.pinch(0, (int) d.getFocusX(), (int) d.getFocusY(), 1f);
                return true;
            }

            @Override
            public boolean onScale(ScaleGestureDetector d) {
                pinchScale *= d.getScaleFactor();
                TclRuntime.pinch(1, (int) d.getFocusX(), (int) d.getFocusY(), pinchScale);
                return true;
            }

            @Override
            public void onScaleEnd(ScaleGestureDetector d) {
                TclRuntime.pinch(2, (int) d.getFocusX(), (int) d.getFocusY(), pinchScale);
            }
        });
        pinch.setQuickScaleEnabled(false);
    }

    // ---- the text-editing toolbar (Cut / Copy / Paste / Select all) -----------------------

    /** Show the platform's floating text toolbar near (x, y); UI thread. */
    void showTextMenu(int x, int y, int flags) {
        menuX = x;
        menuY = y;
        menuFlags = flags;
        if (textMenu != null) {
            textMenu.finish();
        }
        textMenu = startActionMode(new ActionMode.Callback2() {
            @Override
            public boolean onCreateActionMode(ActionMode mode, Menu menu) {
                if ((menuFlags & TclRuntime.MENU_CUT) != 0) {
                    menu.add(0, TclRuntime.MENU_CUT, 0, android.R.string.cut);
                }
                if ((menuFlags & TclRuntime.MENU_COPY) != 0) {
                    menu.add(0, TclRuntime.MENU_COPY, 1, android.R.string.copy);
                }
                if ((menuFlags & TclRuntime.MENU_PASTE) != 0) {
                    menu.add(0, TclRuntime.MENU_PASTE, 2, android.R.string.paste);
                }
                if ((menuFlags & TclRuntime.MENU_SELECT_ALL) != 0) {
                    menu.add(0, TclRuntime.MENU_SELECT_ALL, 3, android.R.string.selectAll);
                }
                return menu.size() > 0;
            }

            @Override
            public boolean onPrepareActionMode(ActionMode mode, Menu menu) {
                return false;
            }

            @Override
            public boolean onActionItemClicked(ActionMode mode, MenuItem item) {
                TclRuntime.textAction(item.getItemId());
                mode.finish();
                return true;
            }

            @Override
            public void onDestroyActionMode(ActionMode mode) {
                if (textMenu == mode) {
                    textMenu = null;
                }
            }

            @Override
            public void onGetContentRect(ActionMode mode, View view, Rect outRect) {
                int r = (int) (24 * getResources().getDisplayMetrics().density);
                outRect.set(menuX - r, menuY - 2 * r, menuX + r, menuY);
            }
        }, ActionMode.TYPE_FLOATING);
    }

    void hideTextMenu() {
        if (textMenu != null) {
            textMenu.finish();
        }
    }

    void setSizeListener(SizeListener l) {
        sizeListener = l;
    }

    int dpi() {
        return getResources().getDisplayMetrics().densityDpi;
    }

    /** Called on the Tcl thread through the listener. */
    int[] allocFrame(int w, int h) {
        synchronized (lock) {
            bitmap = Bitmap.createBitmap(w, h, Bitmap.Config.ARGB_8888);
            frame = new int[w * h];
            frameW = w;
            frameH = h;
            return frame;
        }
    }

    /** Called on the Tcl thread through the listener. */
    void onFrame(int x1, int y1, int x2, int y2) {
        synchronized (lock) {
            if (bitmap == null || x2 <= x1 || y2 <= y1) {
                return;
            }
            bitmap.setPixels(frame, y1 * frameW + x1, frameW, x1, y1, x2 - x1, y2 - y1);
        }
        postInvalidate(x1, y1, x2, y2);
    }

    void showKeyboard() {
        post(new Runnable() {
            @Override
            public void run() {
                requestFocus();
                InputMethodManager imm = (InputMethodManager)
                        getContext().getSystemService(Context.INPUT_METHOD_SERVICE);
                if (imm != null) {
                    imm.showSoftInput(TkView.this, 0);
                }
            }
        });
    }

    void hideKeyboard() {
        post(new Runnable() {
            @Override
            public void run() {
                InputMethodManager imm = (InputMethodManager)
                        getContext().getSystemService(Context.INPUT_METHOD_SERVICE);
                if (imm != null) {
                    imm.hideSoftInputFromWindow(getWindowToken(), 0);
                }
            }
        });
    }

    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        super.onSizeChanged(w, h, oldw, oldh);
        if (w > 0 && h > 0 && sizeListener != null) {
            sizeListener.onSizeKnown(w, h);
        }
    }

    @Override
    protected void onDraw(Canvas canvas) {
        synchronized (lock) {
            if (bitmap != null) {
                canvas.drawBitmap(bitmap, 0, 0, null);
            }
        }
    }

    // ---- touch -------------------------------------------------------------------------

    @Override
    public boolean onTouchEvent(MotionEvent e) {
        int masked = e.getActionMasked();
        if (masked == MotionEvent.ACTION_DOWN && textMenu != null) {
            textMenu.finish();		// touching anything else dismisses the toolbar
        }
        longPress.onTouchEvent(e);
        pinch.onTouchEvent(e);
        if (masked == MotionEvent.ACTION_POINTER_DOWN && !multiTouch) {
            // A second finger: take the first one away from Tk (a cancel, not a click)
            // and report only pinch gestures until every finger is up.
            multiTouch = true;
            TclRuntime.touch(3, (int) e.getX(0), (int) e.getY(0));
            return true;
        }
        if (multiTouch) {
            if (masked == MotionEvent.ACTION_UP || masked == MotionEvent.ACTION_CANCEL) {
                multiTouch = false;
            }
            return true;
        }
        int action;
        switch (masked) {
            case MotionEvent.ACTION_DOWN: action = 0; break;
            case MotionEvent.ACTION_UP: action = 1; break;
            case MotionEvent.ACTION_MOVE: action = 2; break;
            case MotionEvent.ACTION_CANCEL: action = 3; break;
            default: return true;	// secondary pointers are ignored for now
        }
        TclRuntime.touch(action, (int) e.getX(), (int) e.getY());
        return true;
    }

    // ---- keys -------------------------------------------------------------------------------

    private static int specialKeysym(int keyCode) {
        switch (keyCode) {
            case KeyEvent.KEYCODE_DEL: return XK_BACKSPACE;
            case KeyEvent.KEYCODE_FORWARD_DEL: return XK_DELETE;
            case KeyEvent.KEYCODE_ENTER:
            case KeyEvent.KEYCODE_NUMPAD_ENTER: return XK_RETURN;
            case KeyEvent.KEYCODE_TAB: return XK_TAB;
            case KeyEvent.KEYCODE_ESCAPE: return XK_ESCAPE;
            case KeyEvent.KEYCODE_DPAD_LEFT: return XK_LEFT;
            case KeyEvent.KEYCODE_DPAD_RIGHT: return XK_RIGHT;
            case KeyEvent.KEYCODE_DPAD_UP: return XK_UP;
            case KeyEvent.KEYCODE_DPAD_DOWN: return XK_DOWN;
            case KeyEvent.KEYCODE_MOVE_HOME: return XK_HOME;
            case KeyEvent.KEYCODE_MOVE_END: return XK_END;
            case KeyEvent.KEYCODE_PAGE_UP: return XK_PRIOR;
            case KeyEvent.KEYCODE_PAGE_DOWN: return XK_NEXT;
            case KeyEvent.KEYCODE_INSERT: return XK_INSERT;
            case KeyEvent.KEYCODE_F1: case KeyEvent.KEYCODE_F2: case KeyEvent.KEYCODE_F3: case KeyEvent.KEYCODE_F4:
            case KeyEvent.KEYCODE_F5: case KeyEvent.KEYCODE_F6: case KeyEvent.KEYCODE_F7: case KeyEvent.KEYCODE_F8:
            case KeyEvent.KEYCODE_F9: case KeyEvent.KEYCODE_F10: case KeyEvent.KEYCODE_F11: case KeyEvent.KEYCODE_F12:
                return 0xFFBE + (keyCode - KeyEvent.KEYCODE_F1);
            default: return 0;
        }
    }

    /** X modifier mask for the event: Shift 1, Control 4, Alt (Mod1) 8. Hardware keyboards only. */
    private static int xstate(KeyEvent e) {
        int m = e.getMetaState();
        return ((m & KeyEvent.META_SHIFT_ON) != 0 ? 1 : 0) | ((m & KeyEvent.META_CTRL_ON) != 0 ? 4 : 0)
                | ((m & KeyEvent.META_ALT_ON) != 0 ? 8 : 0);
    }

    private boolean forward(KeyEvent e, boolean down) {
        int keyCode = e.getKeyCode();
        if (keyCode == KeyEvent.KEYCODE_BACK) {
            return false;	// handled by the Activity
        }
        int state = xstate(e);
        int sym = specialKeysym(keyCode);
        if (sym == XK_TAB && (state & 1) != 0) {
            sym = 0xFE20;	// ISO_Left_Tab, what X sends for Shift+Tab (Tk binds <<PrevWindow>> to it)
        }
        if (sym != 0) {
            TclRuntime.key(down, sym, 0, state);
            return true;
        }
        // Ctrl+C etc.: the character WITHOUT Ctrl/Alt/Meta (getUnicodeChar() would give a control code), so
        // that <Control-c>, <<Copy>>, <<Paste>>, <<SelectAll>>, <<Undo>> and friends fire as on a desktop.
        int plain = e.getMetaState() & ~(KeyEvent.META_CTRL_MASK | KeyEvent.META_ALT_MASK | KeyEvent.META_META_MASK);
        int cp = e.getUnicodeChar(plain);
        if (cp > 0 && !Character.isISOControl(cp)) {
            TclRuntime.key(down, 0, cp, state);
            return true;
        }
        return false;
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent e) {
        return forward(e, true) || super.onKeyDown(keyCode, e);
    }

    @Override
    public boolean onKeyUp(int keyCode, KeyEvent e) {
        return forward(e, false) || super.onKeyUp(keyCode, e);
    }

    // ---- soft keyboard ---------------------------------------------------------------------------

    // ---- accessibility (TalkBack and other screen readers) -------------------------------------------------------
    //
    // Tk draws one bitmap, so to a screen reader the view would be a single blank rectangle. The Tcl side
    // (tk-android/library/a11y.tcl) describes the widgets; this exposes that description as virtual
    // accessibility nodes and sends the reader's actions back to Tk.

    private volatile A11ySnapshot a11y = new A11ySnapshot();
    private volatile boolean a11yOn;
    private int a11yFocusedId = -2;		// the node TalkBack has put its accessibility focus on
    private int a11yInputFocusId;
    private AccessibilityManager accessibilityManager;

    private final AccessibilityManager.AccessibilityStateChangeListener a11yStateListener =
            new AccessibilityManager.AccessibilityStateChangeListener() {
                @Override
                public void onAccessibilityStateChanged(boolean enabled) {
                    reportAccessibilityState();
                }
            };

    private void reportAccessibilityState() {
        boolean on = accessibilityManager != null && accessibilityManager.isEnabled();
        if (on != a11yOn) {
            a11yOn = on;
            TclRuntime.a11yEnable(on);
        }
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        accessibilityManager = (AccessibilityManager) getContext().getSystemService(Context.ACCESSIBILITY_SERVICE);
        if (accessibilityManager != null) {
            accessibilityManager.addAccessibilityStateChangeListener(a11yStateListener);
        }
        reportAccessibilityState();
    }

    @Override
    protected void onDetachedFromWindow() {
        if (accessibilityManager != null) {
            accessibilityManager.removeAccessibilityStateChangeListener(a11yStateListener);
        }
        super.onDetachedFromWindow();
    }

    /** From the Tcl thread: the screen changed. */
    void onA11yTree(final String snapshot) {
        final A11ySnapshot snap = A11ySnapshot.parse(snapshot);
        a11y = snap;
        post(new Runnable() {
            @Override
            public void run() {
                if (accessibilityManager == null || !accessibilityManager.isEnabled()) {
                    return;
                }
                // which node holds Tk's keyboard focus now
                int focused = 0;
                for (A11ySnapshot.Node n : snap.nodes.values()) {
                    if (n.has('f')) {
                        focused = n.id;
                    }
                }
                if (getParent() != null) {
                    getParent().notifySubtreeAccessibilityStateChanged(TkView.this, TkView.this,
                            AccessibilityEvent.CONTENT_CHANGE_TYPE_SUBTREE);
                }
                if (focused != 0 && focused != a11yInputFocusId) {
                    a11yInputFocusId = focused;
                    sendVirtualEvent(focused, AccessibilityEvent.TYPE_VIEW_FOCUSED);
                }
            }
        });
    }

    private void sendVirtualEvent(int id, int type) {
        if (accessibilityManager == null || !accessibilityManager.isEnabled() || getParent() == null) {
            return;
        }
        AccessibilityEvent ev = AccessibilityEvent.obtain(type);
        onInitializeAccessibilityEvent(ev);
        ev.setSource(this, id);
        getParent().requestSendAccessibilityEvent(this, ev);
    }

    private static String classFor(String role) {
        switch (role) {
            case "button": return "android.widget.Button";
            case "checkbox": return "android.widget.CheckBox";
            case "radio": return "android.widget.RadioButton";
            case "edit": return "android.widget.EditText";
            case "text": return "android.widget.TextView";
            case "list": return "android.widget.ListView";
            case "slider": return "android.widget.SeekBar";
            case "progress": return "android.widget.ProgressBar";
            default: return "android.view.View";
        }
    }

    private final AccessibilityNodeProvider a11yProvider = new AccessibilityNodeProvider() {
        @Override
        public AccessibilityNodeInfo createAccessibilityNodeInfo(int virtualId) {
            A11ySnapshot snap = a11y;
            if (virtualId == View.NO_ID) {
                AccessibilityNodeInfo info = AccessibilityNodeInfo.obtain(TkView.this);
                onInitializeAccessibilityNodeInfo(info);
                // The view itself is only a container for the nodes below it. If it were focusable or
                // clickable, TalkBack could put its focus here and a double-tap would do nothing.
                info.setFocusable(false);
                info.setClickable(false);
                info.setLongClickable(false);
                if (Build.VERSION.SDK_INT >= 28) {
                    info.setScreenReaderFocusable(false);
                }
                info.removeAction(AccessibilityNodeInfo.AccessibilityAction.ACTION_CLICK);
                info.removeAction(AccessibilityNodeInfo.AccessibilityAction.ACTION_FOCUS);
                info.removeAction(AccessibilityNodeInfo.AccessibilityAction.ACTION_LONG_CLICK);
                for (int id : snap.roots) {
                    info.addChild(TkView.this, id);
                }
                return info;
            }
            A11ySnapshot.Node n = snap.nodes.get(virtualId);
            if (n == null) {
                return null;
            }
            AccessibilityNodeInfo info = AccessibilityNodeInfo.obtain(TkView.this, virtualId);
            info.setPackageName(getContext().getPackageName());
            info.setClassName(classFor(n.role));
            info.setParent(TkView.this, snap.nodes.containsKey(n.parent) ? n.parent : View.NO_ID);
            for (int c : n.children) {
                info.addChild(TkView.this, c);
            }
            int[] loc = new int[2];
            getLocationOnScreen(loc);
            info.setBoundsInScreen(new Rect(loc[0] + n.x1, loc[1] + n.y1, loc[0] + n.x2, loc[1] + n.y2));
            info.setVisibleToUser(true);
            info.setEnabled(n.has('e'));
            info.setFocusable(n.has('x') || n.has('t'));
            info.setFocused(n.has('f'));
            info.setClickable(n.has('x'));
            info.setCheckable(n.has('c'));
            info.setChecked(n.has('k'));
            info.setSelected(n.has('s'));
            info.setScrollable(n.has('l'));
            info.setPassword(n.has('p'));
            info.setEditable(n.has('t'));
            info.setMultiLine(n.has('m'));
            if (n.role.equals("edit")) {
                info.setText(n.value);
                info.setContentDescription(n.text.isEmpty() ? null : n.text);
            } else if (n.role.equals("checkbox") || n.role.equals("radio")) {
                info.setText(n.text);
            } else {
                info.setText(n.label());
            }
            if (n.role.equals("window") && Build.VERSION.SDK_INT >= 28) {
                info.setPaneTitle(n.text);
            }
            if (n.has('x')) {
                info.addAction(AccessibilityNodeInfo.AccessibilityAction.ACTION_CLICK);
            }
            if (n.has('x') || n.has('t')) {
                info.addAction(AccessibilityNodeInfo.AccessibilityAction.ACTION_FOCUS);
            }
            if (n.has('t')) {
                info.addAction(AccessibilityNodeInfo.AccessibilityAction.ACTION_SET_TEXT);
            }
            if (n.has('l')) {
                info.addAction(AccessibilityNodeInfo.AccessibilityAction.ACTION_SCROLL_FORWARD);
                info.addAction(AccessibilityNodeInfo.AccessibilityAction.ACTION_SCROLL_BACKWARD);
            }
            info.addAction(virtualId == a11yFocusedId
                    ? AccessibilityNodeInfo.AccessibilityAction.ACTION_CLEAR_ACCESSIBILITY_FOCUS
                    : AccessibilityNodeInfo.AccessibilityAction.ACTION_ACCESSIBILITY_FOCUS);
            return info;
        }

        @Override
        public boolean performAction(int virtualId, int action, Bundle args) {
            if (virtualId == View.NO_ID) {
                return TkView.this.performAccessibilityAction(action, args);
            }
            A11ySnapshot.Node n = a11y.nodes.get(virtualId);
            if (n == null) {
                return false;
            }
            switch (action) {
                case AccessibilityNodeInfo.ACTION_ACCESSIBILITY_FOCUS:
                    a11yFocusedId = virtualId;
                    sendVirtualEvent(virtualId, AccessibilityEvent.TYPE_VIEW_ACCESSIBILITY_FOCUSED);
                    invalidate();
                    return true;
                case AccessibilityNodeInfo.ACTION_CLEAR_ACCESSIBILITY_FOCUS:
                    if (a11yFocusedId == virtualId) {
                        a11yFocusedId = -2;
                    }
                    sendVirtualEvent(virtualId, AccessibilityEvent.TYPE_VIEW_ACCESSIBILITY_FOCUS_CLEARED);
                    invalidate();
                    return true;
                case AccessibilityNodeInfo.ACTION_CLICK:
                    clickNode(n);
                    return true;
                case AccessibilityNodeInfo.ACTION_FOCUS:
                case AccessibilityNodeInfo.ACTION_SCROLL_FORWARD:
                case AccessibilityNodeInfo.ACTION_SCROLL_BACKWARD:
                    TclRuntime.a11yAction(virtualId, action, null);
                    return true;
                case AccessibilityNodeInfo.ACTION_SET_TEXT: {
                    CharSequence t = args == null ? null
                            : args.getCharSequence(AccessibilityNodeInfo.ACTION_ARGUMENT_SET_TEXT_CHARSEQUENCE);
                    TclRuntime.a11yAction(virtualId, action, t == null ? "" : t.toString());
                    return true;
                }
                default:
                    return false;
            }
        }
    };

    /**
     * Click a node. The centre comes from the snapshot this side holds, so the click does not depend on the
     * Tcl side still knowing the node's id. A tap there is what Tk sees, exactly as for a finger.
     */
    private void clickNode(A11ySnapshot.Node n) {
        int cx = (n.x1 + n.x2) / 2, cy = (n.y1 + n.y2) / 2;
        TclRuntime.a11yAction(n.id, AccessibilityNodeInfo.ACTION_CLICK, cx + " " + cy);
        sendVirtualEvent(n.id, AccessibilityEvent.TYPE_VIEW_CLICKED);
    }

    /** A click on the view itself (some services do this): click what has screen-reader focus. */
    @Override
    public boolean performClick() {
        A11ySnapshot.Node n = a11y.nodes.get(a11yFocusedId);
        if (n != null && a11yOn) {
            clickNode(n);
            return true;
        }
        return super.performClick();
    }

    @Override
    public AccessibilityNodeProvider getAccessibilityNodeProvider() {
        return a11yProvider;
    }

    // ---- what kind of text the soft keyboard offers (::tkdroid::inputmode) ---------------------------------------

    /** TKA_INPUT_*: 0 code (no suggestions or autocorrect), 1 text (composition on), 2 number, 3 email, 4 url, 5 password. */
    private volatile int inputMode;

    private final ImeBridge ime = new ImeBridge(new ImeBridge.Sink() {
        @Override
        public void chars(String str) {
            for (int i = 0; i < str.length(); ) {
                int c = str.codePointAt(i);
                i += Character.charCount(c);
                TclRuntime.key(true, 0, c, 0);
                TclRuntime.key(false, 0, c, 0);
            }
        }

        @Override
        public void backspace(int n) {
            for (int i = 0; i < n; i++) {
                TclRuntime.key(true, XK_BACKSPACE, 0, 0);
                TclRuntime.key(false, XK_BACKSPACE, 0, 0);
            }
        }

        @Override
        public void deleteForward(int n) {
            for (int i = 0; i < n; i++) {
                TclRuntime.key(true, XK_DELETE, 0, 0);
                TclRuntime.key(false, XK_DELETE, 0, 0);
            }
        }

        @Override
        public void enter() {
            TclRuntime.key(true, XK_RETURN, 0, 0);
            TclRuntime.key(false, XK_RETURN, 0, 0);
        }
    });

    void setInputMode(final int mode) {
        inputMode = mode;
        post(new Runnable() {
            @Override
            public void run() {
                InputMethodManager imm = (InputMethodManager) getContext().getSystemService(Context.INPUT_METHOD_SERVICE);
                if (imm != null) {
                    imm.restartInput(TkView.this);	// asks onCreateInputConnection again
                }
            }
        });
    }

    @Override
    public boolean onCheckIsTextEditor() {
        return true;
    }

    @Override
    public InputConnection onCreateInputConnection(EditorInfo out) {
        int mode = inputMode;
        switch (mode) {
            case 1:	// ordinary text: suggestions, swipe typing and CJK composition on
                out.inputType = InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_FLAG_CAP_SENTENCES
                        | InputType.TYPE_TEXT_FLAG_MULTI_LINE;
                break;
            case 2:
                out.inputType = InputType.TYPE_CLASS_NUMBER | InputType.TYPE_NUMBER_FLAG_DECIMAL
                        | InputType.TYPE_NUMBER_FLAG_SIGNED;
                break;
            case 3:
                out.inputType = InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_EMAIL_ADDRESS;
                break;
            case 4:
                out.inputType = InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_URI;
                break;
            case 5:
                out.inputType = InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_PASSWORD;
                break;
            default:	// code: visible-password mode asks the keyboard to commit each character at once
                out.inputType = InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS
                        | InputType.TYPE_TEXT_VARIATION_VISIBLE_PASSWORD;
        }
        out.imeOptions = EditorInfo.IME_ACTION_DONE | EditorInfo.IME_FLAG_NO_EXTRACT_UI
                | EditorInfo.IME_FLAG_NO_FULLSCREEN;
        ime.finish();
        ime.setComposingEnabled(mode == 1);
        return new BaseInputConnection(this, false) {
            @Override
            public boolean commitText(CharSequence text, int newCursorPosition) {
                ime.commit(text.toString());
                return true;
            }

            @Override
            public boolean setComposingText(CharSequence text, int newCursorPosition) {
                ime.setComposing(text.toString());
                return true;
            }

            @Override
            public boolean finishComposingText() {
                ime.finish();
                return true;
            }

            @Override
            public boolean setComposingRegion(int start, int end) {
                return true;
            }

            @Override
            public boolean deleteSurroundingText(int before, int after) {
                ime.deleteSurrounding(before, after);
                return true;
            }

            @Override
            public boolean sendKeyEvent(KeyEvent e) {
                return forward(e, e.getAction() == KeyEvent.ACTION_DOWN);
            }

            @Override
            public boolean performEditorAction(int actionCode) {
                ime.finish();
                ime.commit("\n");
                return true;
            }
        };
    }
}
