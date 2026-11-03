#!/usr/bin/env python3
"""Turn the output of tools/survey-tk-android.sh into docs/tk-android-work-list.md.

Groups every missing platform symbol into a function family and, where a macOS
port counterpart exists, names the file that implements it (a model, and a size
proxy). Fails loudly if a symbol is not covered by any family rule, so nothing
is silently dropped.   usage: tools/gen-tk-worklist.py [abi]
"""
import os, re, sys, collections

root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
abi = sys.argv[1] if len(sys.argv) > 1 else 'arm64-v8a'
build = os.environ.get('TCLDROID_BUILD', os.path.join(root, 'build'))
survey = os.path.join(build, 'survey', abi)
tk = os.path.join(build, 'src', 'tk9.0.4')
miss = [l.strip() for l in open(os.path.join(survey, 'undefined.txt')) if l.strip()]
compiled = len([l for l in open(os.path.join(survey, 'compile-set.txt')) if l.strip()])
failed = open(os.path.join(survey, 'compile.log')).read().count('FAILED:')

# Ordered (first match wins): (family, regex)
FAMILIES = [
 ('Unsupported or trivial stubs (embedding, `send`, X-server queries)', r'^(Tk_MakeContainer|Tk_UseWindow|TkUnixContainerId|TkSendCleanup|TkpTestsendCmd|TkpTestembedCmd|TkGetInterpNames|XSetCommand|XSetWMClientMachine|XStringListToTextProperty|XListHosts)$'),
 ('Regions', r'^(X(Create|Destroy|Union|Intersect|Subtract|Offset)\w*Region|XRectInRegion|XClipBox|XUnionRectWithRegion|TkpBuildRegionFromAlphaData|TkpCopyRegion)$'),
 ('Drawing, images, scrolling', r'^(XDraw\w*|XFill\w*|XCopy\w*|XClearWindow|XGetImage|XPutImage|XCreateImage|_XInitImageFuncPtrs|XGContextFromGC|TkScrollWindow|TkpDrawFrameEx|Tk_DrawHighlightBorder|Tk_GetPixmap|Tk_FreePixmap|TkpCreateBusy|TkpHideBusyWindow|TkpShowBusyWindow|XGetGeometry)$'),
 ('Text: fonts, measuring, drawing', r'^(TkpFont\w*|TkpGet(Font\w*|NativeFont|SubFonts|FontFamilies)|TkpDeleteFont|Tk_(Measure|Draw)Chars\w*|TkDrawAngledChars|TkpGetFontAttrsForChar|TkpFontPkgInit)$'),
 ('Colors', r'^(XAllocColor|XQueryColors|XLookupColor|XFreeColors|XCreateColormap|XFreeColormap|XSetWindowColormap|XGetWMColormapWindows|TkpGetColor\w*|TkWm(Add|Remove)\w*ColormapWindows|TkpCmapStressed|XVisualIDFromVisual)$'),
 ('Keyboard and text input (IME)', r'^(X(Keycode|Keysym|StringToKeysym|Refresh|GetModifier|FreeModifier|mbLookup|CreateIC|FilterEvent|kb)\w*|XkbKeycodeToKeysym|XkbOpenDisplay|XGrabKeyboard|XUngrabKeyboard|TkpGetKeySym|TkpInitKeymapInfo|TkpSetKeycodeAndState|TkpRedirectKeyEvent|Tk_SetCaretPos|XKeysymToString|XKeysymToKeycode)$'),
 ('Pointer, focus and cursors', r'^(XQueryPointer|XWarpPointer|TkpWarpPointer|XGetInputFocus|XSetInputFocus|TkpChangeFocus|TkpClaimFocus|TkpSetCapture|TkpGetCapture|TkpSetCursor|TkGetPointerCoords|TkCreateCursorFromData|TkGetCursorByName|TkpFreeCursor|XFreeCursor|Tk_CoordsToWindow|Tk_GetRootCoords)$'),
 ('Selection, clipboard, properties, atoms', r'^(XSetSelectionOwner|XChangeProperty|XDeleteProperty|XGetWindowProperty|XGetAtomName|TkSel\w*|TkClipCleanup|TkSelUpdateClipboard)$'),
 ('Event loop, display connection, timing', r'^(XNextEvent|XPutBackEvent|XSelectInput|XSendEvent|XWindowEvent|XFlush|XSync|XNoOp|XSynchronize|XSetErrorHandler|XFree|XGrabServer|XUngrabServer|XForceScreenSaver|XBell|TkCreateXEventSource|TkUnixDoOneXEvent|TkpOpenDisplay|TkpCloseDisplay|TkpGetMS|TkpSync|TkpDisplayWarning|TkGetServerInfo|TkpInit|TkpGetWrapperWindow|TkGetDefaultScreenName|Tk_GetUserInactiveTime|Tk_ResetUserInactiveTime)$'),
 ('Window tree and geometry (the in-process "server")', r'^(X(Destroy|Map|Unmap|Configure|Move|Resize|MoveResize|Raise|Lower|Iconify|Withdraw|Root|QueryTree|TranslateCoordinates|GetWindowAttributes|ChangeWindowAttributes|SetWindow\w*)Window\w*|XRootWindow|XQueryTree|XTranslateCoordinates|XGetWindowAttributes|XChangeWindowAttributes|XSetWindow\w+|Tk_MakeWindow|Tk_GetOtherWindow|Tk_GetVRootGeometry|TkpScanWindowId|TkpMakeTransparentWindowExist|TkpGetAppName|Tk_SetAppName|Tk_GetSystemDefault)$'),
 ('Window manager (`wm`, toplevels, grid)', r'^(Tk_WmObjCmd|TkWm\w*|TkpWmSetState|Tk_(SetGrid|UnsetGrid|MoveToplevelWindow)|Tk_SetMainMenubar|TkUnixSetMenubar|XSetIconName)$'),
 ('Menus (platform side)', r'^(TkpMenu\w*|TkpNewMenu|TkpPostMenu|TkpPostTearoffMenu|TkpSetWindowMenuBar|TkpDestroyMenu\w*|TkpInitializeMenuBindings|TkpMakeMenuWindow|TkpConfigureMenuEntry|TkpDrawMenuEntry|TkpCompute\w*Menu\w*|TkpGetString)$'),
]
fam = collections.OrderedDict((f, []) for f, _ in FAMILIES); rest = []
for s in miss:
    for f, rx in FAMILIES:
        if re.match(rx, s):
            fam[f].append(s); break
    else:
        rest.append(s)
if rest:
    sys.exit('UNCATEGORISED symbols (extend FAMILIES in this script): ' + ' '.join(rest))

# macOS counterpart file for each symbol (definition at column 0), for sizing/modelling.
want = set(miss); where = {}; size = {}
for fn in sorted(os.listdir(tk + '/macosx')):
    if fn.endswith(('.c', '.m')):
        txt = open(f'{tk}/macosx/{fn}', errors='replace').read()
        size[fn] = txt.count('\n')
        for m in re.finditer(r'^(\w+)\s*\(', txt, re.M):
            if m.group(1) in want: where.setdefault(m.group(1), fn)
files = collections.Counter(where.values())

out = []
w = out.append
w('# Tk-on-Android: platform work list\n')
w('_Generated by `tools/gen-tk-worklist.py` from `tools/survey-tk-android.sh`. Do not edit by hand; re-run the tools._\n')
w('## Method\n')
w(f'Tk\'s platform-independent sources ({compiled} files: the shared widget/canvas/text/image/font/ttk code from `generic/`, `generic/ttk/`, `xlib/`, plus the platform-neutral Unix widget files `tkUnixButton`, `tkUnixMenubu`, `tkUnixScrlbr`, `tkUnixMenu`, `tkUnix3d`, `tkUnixScale`) were compiled for **{abi}** with the NDK against the Android port header (`tk-android/android/`). **{failed} files fail to compile.** Every symbol they reference but that neither Tk, Bionic nor Tcl\'s stub library defines is a function the Android platform layer must provide. The file set mirrors what the macOS port builds (read from `unix/Makefile.in`).\n')
w(f'**Result: {len(miss)} functions to implement.**\n')
w('| Family | Functions |\n| --- | --- |')
for f, v in fam.items():
    w(f'| {f} | {len(v)} |')
w('')
w('## By family\n')
for f, v in fam.items():
    if not v: continue
    w(f'### {f} ({len(v)})\n')
    w(', '.join(f'`{s}`' for s in v) + '\n')
w('## Where the macOS port implements them (model and size proxy)\n')
w('A macOS counterpart is a model, not a template: those files include Cocoa glue, native controls and comments. Line counts are an upper bound on the effort, not an estimate of it.\n')
w('| macOS file | Missing functions it implements | File size (lines) |\n| --- | --- | --- |')
for fn, c in files.most_common():
    w(f'| `macosx/{fn}` | {c} | {size[fn]} |')
nomac = [s for s in miss if s not in where]
w(f'\n{len(where)} of {len(miss)} have a macOS counterpart; the remaining {len(nomac)} are implemented on other platforms only (or are tables): ' + ', '.join(f'`{s}`' for s in nomac) + '.\n')
w('## Reading this list\n')
w('- **Unchanged upstream code is reused**: all widgets, geometry managers, bindings, the canvas/text engines and ttk compile as-is. The platform layer implements the *Xlib-shaped API* Tk calls, not new widgets.\n- **Several families are bookkeeping on Tk\'s own data**, not rendering: the window tree, properties/atoms, selection, event queue, focus. On Android these operate on in-process records (no X server), exactly as on macOS and Windows.\n- **The genuinely platform-specific work** is: drawing and images, fonts/text, regions, input (keyboard/IME/touch), the window-manager policy, and menus. Rendering and text are the decisions ADR 0002 defers to a spike.\n- Items marked unsupported by policy are expected to be stubs that raise a clear Tcl error.\n')
open(os.path.join(root, 'docs', 'tk-android-work-list.md'), 'w').write('\n'.join(out))
print('wrote docs/tk-android-work-list.md:', len(miss), 'functions in', len([v for v in fam.values() if v]), 'families')
