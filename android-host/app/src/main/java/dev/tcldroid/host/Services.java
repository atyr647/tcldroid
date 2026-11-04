package dev.tcldroid.host;

import android.Manifest;
import android.app.Activity;
import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.ActivityNotFoundException;
import android.content.ClipData;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.database.Cursor;
import android.net.Uri;
import android.os.Build;
import android.provider.OpenableColumns;
import android.webkit.MimeTypeMap;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Asynchronous platform services for Tcl scripts (::tkdroid::pickfile, savefile, share, openurl,
 * permission, notify). Requests arrive from the Tcl thread, run on the UI thread, and are answered with
 * TclRuntime.result when the system UI finishes. Picked files are copied into the app's cache so Tcl can
 * open them as ordinary files; no content:// URI ever reaches a script.
 */
final class Services {
    private static final String CHANNEL = "tcldroid";
    private final Activity act;
    private final Map<Integer, String> saveSource = new HashMap<>();
    private final Map<Integer, String> savedName = new HashMap<>();

    Services(Activity activity) {
        act = activity;
    }

    static Map<String, String> parse(byte[] args) {
        Map<String, String> m = new HashMap<>();
        for (String line : new String(args, StandardCharsets.UTF_8).split("\n")) {
            int t = line.indexOf('\t');
            if (t > 0) {
                m.put(line.substring(0, t), line.substring(t + 1));
            }
        }
        return m;
    }

    int check(String permission) {
        return act.checkSelfPermission(permission) == PackageManager.PERMISSION_GRANTED ? 1 : 0;
    }

    /** UI thread. */
    void handle(int id, String kind, Map<String, String> a) {
        try {
            switch (kind) {
                case "openurl": openUrl(id, a); break;
                case "share": share(id, a); break;
                case "pickfile": pick(id, a); break;
                case "savefile": save(id, a); break;
                case "permission": permission(id, a); break;
                case "notify": notify(id, a); break;
                case "toast":
                    android.widget.Toast.makeText(act, String.valueOf(a.get("text")), android.widget.Toast.LENGTH_SHORT).show();
                    TclRuntime.result(id, "ok", "");
                    break;
                default: TclRuntime.result(id, "error", "unknown request " + kind);
            }
        } catch (RuntimeException | IOException e) {
            TclRuntime.result(id, "error", String.valueOf(e.getMessage()));
        }
    }

    // ---- open a link ------------------------------------------------------------------------

    private void openUrl(int id, Map<String, String> a) {
        String url = a.get("url");
        Uri uri = url == null ? null : Uri.parse(url);
        String scheme = uri == null || uri.getScheme() == null ? "" : uri.getScheme().toLowerCase();
        if (!(scheme.equals("http") || scheme.equals("https") || scheme.equals("mailto") || scheme.equals("tel")
                || scheme.equals("geo") || scheme.equals("sms"))) {
            TclRuntime.result(id, "error", "unsupported link type \"" + scheme + "\"");
            return;
        }
        try {
            act.startActivity(new Intent(Intent.ACTION_VIEW, uri));
            TclRuntime.result(id, "ok", "");
        } catch (ActivityNotFoundException e) {
            TclRuntime.result(id, "error", "no app can open this link");
        }
    }

    // ---- share ----------------------------------------------------------------------------------

    private static String mimeOf(String name) {
        String ext = MimeTypeMap.getFileExtensionFromUrl(name.replace(" ", "_"));
        String t = ext == null || ext.isEmpty() ? null
                : MimeTypeMap.getSingleton().getMimeTypeFromExtension(ext.toLowerCase());
        return t != null ? t : "application/octet-stream";
    }

    private static String safeName(String name) {
        String n = name == null ? "file" : name.replaceAll("[\\\\/\\p{Cntrl}]", "_");
        while (n.startsWith(".")) {
            n = n.substring(1);
        }
        return n.isEmpty() ? "file" : n;
    }

    private void share(int id, Map<String, String> a) throws IOException {
        Intent send = new Intent(Intent.ACTION_SEND);
        String text = a.get("text"), file = a.get("file");
        if (text != null) {
            send.putExtra(Intent.EXTRA_TEXT, text);
        }
        send.setType(a.containsKey("type") ? a.get("type") : file != null ? mimeOf(file) : "text/plain");
        if (file != null) {
            File dir = new File(act.getCacheDir(), "share");
            if (!dir.isDirectory() && !dir.mkdirs()) {
                throw new IOException("cannot create " + dir);
            }
            File copy = new File(dir, safeName(new File(file).getName()));
            copyFile(new File(file), copy);
            Uri uri = new Uri.Builder().scheme("content").authority(act.getPackageName() + ".share")
                    .appendPath(copy.getName()).build();
            send.putExtra(Intent.EXTRA_STREAM, uri);
            send.setClipData(ClipData.newRawUri(copy.getName(), uri));
            send.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
        }
        act.startActivity(Intent.createChooser(send, a.get("title")));
        TclRuntime.result(id, "ok", "");	// Android does not say whether the user finished sharing
    }

    private static void copyFile(File from, File to) throws IOException {
        try (InputStream in = new FileInputStream(from); OutputStream out = new FileOutputStream(to)) {
            copy(in, out);
        }
    }

    private static void copy(InputStream in, OutputStream out) throws IOException {
        byte[] buf = new byte[64 * 1024];
        for (int n; (n = in.read(buf)) > 0; ) {
            out.write(buf, 0, n);
        }
    }

    // ---- pick / save a file --------------------------------------------------------------------------

    private void pick(int id, Map<String, String> a) {
        Intent i = new Intent(Intent.ACTION_OPEN_DOCUMENT).addCategory(Intent.CATEGORY_OPENABLE);
        String types = a.get("types");
        if (types == null || types.trim().isEmpty()) {
            i.setType("*/*");
        } else {
            String[] t = types.split(",");
            i.setType(t.length == 1 ? t[0] : "*/*");
            if (t.length > 1) {
                i.putExtra(Intent.EXTRA_MIME_TYPES, t);
            }
        }
        i.putExtra(Intent.EXTRA_ALLOW_MULTIPLE, "1".equals(a.get("multiple")));
        act.startActivityForResult(i, id);
    }

    private void save(int id, Map<String, String> a) {
        String name = safeName(a.get("name"));
        Intent i = new Intent(Intent.ACTION_CREATE_DOCUMENT).addCategory(Intent.CATEGORY_OPENABLE);
        i.setType(a.containsKey("type") ? a.get("type") : mimeOf(name));
        i.putExtra(Intent.EXTRA_TITLE, name);
        saveSource.put(id, a.get("source"));
        savedName.put(id, name);
        act.startActivityForResult(i, id);
    }

    /** From Activity.onActivityResult. Returns true if the request code was ours. */
    boolean onActivityResult(final int id, int resultCode, final Intent data) {
        final boolean saving = saveSource.containsKey(id);
        if (!saving && id <= 0) {
            return false;
        }
        if (resultCode != Activity.RESULT_OK || data == null) {
            saveSource.remove(id);
            TclRuntime.result(id, "cancelled", "");
            return true;
        }
        final List<Uri> uris = new ArrayList<>();
        if (data.getClipData() != null) {
            for (int k = 0; k < data.getClipData().getItemCount(); k++) {
                uris.add(data.getClipData().getItemAt(k).getUri());
            }
        } else if (data.getData() != null) {
            uris.add(data.getData());
        }
        final String source = saveSource.remove(id);
        final String name = savedName.remove(id);
        new Thread(new Runnable() {		// copying can be slow: never on the UI thread
            @Override
            public void run() {
                try {
                    if (saving) {
                        try (InputStream in = new FileInputStream(source);
                             OutputStream out = act.getContentResolver().openOutputStream(uris.get(0), "wt")) {
                            if (out == null) {
                                throw new IOException("cannot write the chosen location");
                            }
                            copy(in, out);
                        }
                        TclRuntime.result(id, "ok", name);
                    } else {
                        TclRuntime.result(id, "ok", copyPicked(id, uris));
                    }
                } catch (IOException | RuntimeException e) {
                    TclRuntime.result(id, "error", String.valueOf(e.getMessage()));
                }
            }
        }, "tcldroid-copy").start();
        return true;
    }

    private String copyPicked(int id, List<Uri> uris) throws IOException {
        File dir = new File(act.getCacheDir(), "picked/" + id);
        if (!dir.isDirectory() && !dir.mkdirs()) {
            throw new IOException("cannot create " + dir);
        }
        StringBuilder out = new StringBuilder();
        for (Uri u : uris) {
            String name = null;
            try (Cursor c = act.getContentResolver().query(u, new String[] {OpenableColumns.DISPLAY_NAME}, null, null, null)) {
                if (c != null && c.moveToFirst()) {
                    name = c.getString(0);
                }
            }
            File dest = new File(dir, safeName(name != null ? name : u.getLastPathSegment()));
            for (int n = 1; dest.exists(); n++) {
                dest = new File(dir, n + "-" + safeName(name != null ? name : "file"));
            }
            try (InputStream in = act.getContentResolver().openInputStream(u); OutputStream o = new FileOutputStream(dest)) {
                if (in == null) {
                    throw new IOException("cannot read the chosen file");
                }
                copy(in, o);
            }
            if (out.length() > 0) {
                out.append('\n');
            }
            out.append(dest.getAbsolutePath());
        }
        return out.toString();
    }

    // ---- permissions ----------------------------------------------------------------------------------------

    private void permission(int id, Map<String, String> a) {
        String[] want = a.get("permissions").split(",");
        List<String> need = new ArrayList<>();
        for (String p : want) {
            if (check(p) == 0) {
                need.add(p);
            }
        }
        if (need.isEmpty()) {
            TclRuntime.result(id, "ok", a.get("permissions").replace(',', '\n'));
            return;
        }
        pendingPerms.put(id, want);
        act.requestPermissions(need.toArray(new String[0]), id);
    }

    private final Map<Integer, String[]> pendingPerms = new HashMap<>();

    boolean onPermissionResult(int id, String[] perms, int[] results) {
        String[] want = pendingPerms.remove(id);
        if (want == null) {
            return false;
        }
        StringBuilder granted = new StringBuilder();
        boolean all = true;
        for (String p : want) {
            if (check(p) == 1) {
                if (granted.length() > 0) {
                    granted.append('\n');
                }
                granted.append(p);
            } else {
                all = false;
            }
        }
        TclRuntime.result(id, all ? "ok" : "denied", granted.toString());
        return true;
    }

    // ---- notifications ------------------------------------------------------------------------------------------

    private void notify(int id, Map<String, String> a) {
        if (Build.VERSION.SDK_INT >= 33 && check(Manifest.permission.POST_NOTIFICATIONS) == 0) {
            TclRuntime.result(id, "denied", "POST_NOTIFICATIONS is not granted (declare it with --permission and request it first)");
            return;
        }
        NotificationManager nm = (NotificationManager) act.getSystemService(Activity.NOTIFICATION_SERVICE);
        nm.createNotificationChannel(new NotificationChannel(CHANNEL, "Notifications", NotificationManager.IMPORTANCE_DEFAULT));
        Intent open = new Intent(act, act.getClass()).addFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP);
        PendingIntent pi = PendingIntent.getActivity(act, 0, open, PendingIntent.FLAG_IMMUTABLE);
        Notification n = new Notification.Builder(act, CHANNEL)
                .setSmallIcon(act.getApplicationInfo().icon)
                .setContentTitle(a.get("title")).setContentText(a.get("text"))
                .setContentIntent(pi).setAutoCancel(true).build();
        nm.notify(id, n);
        TclRuntime.result(id, "ok", "");
    }
}
