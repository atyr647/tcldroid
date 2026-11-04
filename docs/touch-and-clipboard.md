# Touch, selection, clipboard and gestures

What a finger means to a Tk app on Android. All policy is ordinary Tk script in
[`tk-android/library/android.tcl`](../tk-android/library/android.tcl) (loaded by a one-hunk patch to
`tk.tcl`); the platform layer only reports what Android saw. Nothing here changes how Tk's
widgets work on a mouse.

## Clipboard

| Tcl | Android |
| --- | --- |
| `clipboard clear`, `clipboard append x` | puts the text on the system `ClipboardManager` |
| `clipboard get`, `selection get -selection CLIPBOARD`, Entry/Text `<<Paste>>` | reads the system clipboard (unless Tk still owns it) |
| another app copies something | Tk drops its ownership, so it never serves stale text |

Text only (`STRING`/`UTF8_STRING`). `PRIMARY` (text selected in a widget) stays inside Tk, as on
every platform. Android 10+ lets only the focused app read the clipboard; the app is always the
focused one when its own Tk widgets ask.

## One finger

- **Drag on a Text, Listbox or Treeview scrolls it**, pixel by pixel for text, with momentum after
  a flick. **A tap clicks** (places the caret, selects the row). Set `::tkdroid::touchScroll 0`
  to get the desktop behaviour (drag selects) back. An app's own bindings on the widget still
  win, so existing touch-scroll code keeps working.
- **Long press in a text-entry widget** (Entry, Text, Spinbox, ttk entry/combobox/spinbox)
  selects the word under the finger, exactly as a double click does; keep dragging to extend the
  selection; on lift the system's floating **Cut / Copy / Paste / Select all** toolbar appears
  (only the actions that make sense). Its choices fire Tk's `<<Cut>> <<Copy>> <<Paste>>
  <<SelectAll>>` on the focused widget, so custom widgets that handle those work.
- **Long press elsewhere** generates the virtual event `<<LongPress>>` at that widget (`%x %y`).
- **Sliding off a button before lifting does not click it**, and a cancelled touch (a system
  gesture, a second finger) never clicks: the same as releasing a mouse button outside.

## Two fingers

A second finger cancels the first and starts a pinch. Bind these on any widget:

```tcl
bind .canvas <<PinchStart>> {set last 1.0}
bind .canvas <<Pinch>>      {.canvas scale all %x %y [expr {%d / $last}] [expr {%d / $last}]; set last %d}
```

`%d` is the cumulative scale (1.0 = unchanged), `%x %y` the point between the fingers.
`<<PinchEnd>>` reports the final scale. Everything stays cancelled until all fingers lift.

## Rotation

Rotation is an ordinary resize: `.` is resized, widgets re-lay-out, your `<Configure>` bindings
run. The interpreter is not restarted.

## Not done

Draggable selection handles (extend with long-press-and-drag or Select all instead), IME
composition and suggestions, hardware-keyboard Ctrl+C/V, rich (non-text) clipboard content,
two-finger scroll/rotate gestures, and per-widget scroll physics tuning.
