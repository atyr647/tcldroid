# Accessibility (TalkBack and other screen readers)

Tk draws everything into one bitmap, so to a screen reader a Tk app would be a single blank rectangle.
tcldroid fixes that without changing your app: while an accessibility service is running it describes
the widget tree to Android, and the screen reader's actions come back as ordinary Tk events.
(Tk 9.0 has no accessibility API of its own; this is the port's.)

## What a screen reader is told

For every visible widget that means something: its role, name, value, state, screen rectangle and what can
be done to it. Parts of the screen that are scrolled out of view, covered, or unmapped are not described.

| Widget | Role | Name / value | States and actions |
| --- | --- | --- | --- |
| `button`, `ttk::button`, `menubutton` | button | its text | click; disabled when `-state disabled` |
| `label`, `ttk::label`, `message` | text | its text (or `-textvariable`'s value) | - |
| `checkbutton`, `radiobutton` (and ttk) | checkbox / radio | text; on/off | checkable, checked follows the variable |
| `entry`, `spinbox`, `ttk::entry`, `ttk::combobox` | edit | value (dots for `-show`) | editable, focus, set text |
| `text` | edit (editable) or text (read-only) | contents | multi-line, scroll |
| `listbox`, `ttk::treeview` | list, with an item per visible row | row text | selected, click |
| `scale` | slider | value | increase/decrease (scroll) |
| `ttk::progressbar` | progress | value | - |
| `canvas` | each visible `text` item is text; **a button if the canvas or the item/tag has a click binding** | the text | click |
| a `toplevel` other than `.` (dialogs) | window | its `wm title` | its widgets are its children |

Touch exploration works as with any app: the reader moves through the nodes; double-tap clicks the focused
one, which Tk sees as a tap in the middle of that node. Keyboard focus changes are announced.

## Making your app better for screen readers

- Give controls **words**. Icon-only widgets have nothing to say: name them:
  `::tkdroid::accessible $w -name "Close"`. Custom-drawn widgets (a `frame` you paint yourself) can be given a
  role: `-role button`. Purely decorative widgets: `-hidden 1`.
  Roles: `button checkbox radio edit text list slider progress view`.
- Use the standard widgets and `-text`/`-textvariable` rather than drawing words as bitmaps.
- On a canvas, draw words with `text` items (they are readable) and put click bindings on items or tags.
- Keep the focus meaningful: `focus` the first field of a form or dialog.

## How it works (for contributors)

`tk-android/library/a11y.tcl` walks the tree (`::tkdroid::a11y::snapshot`, ~4 ms for a 50-node screen) when
something was drawn, and sends the text snapshot to the host only if it changed. `A11ySnapshot.java` parses
it; `TkView`'s `AccessibilityNodeProvider` builds `AccessibilityNodeInfo`s. Actions arrive as
`::tkdroid::a11y::action`. Nothing runs when no accessibility service is active.

## Not done

Live-region announcements (changes of text are re-read only when the reader revisits the node), reading order
other than top-to-bottom as the tree is walked, notebook tab nodes, custom actions, and braille-specific
features. Accessibility of Tk's own platform dialogs is whatever their widgets provide.
