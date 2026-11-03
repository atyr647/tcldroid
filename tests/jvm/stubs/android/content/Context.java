package android.content;
import android.content.pm.ApplicationInfo;
import java.io.File;
/** Minimal host stand-in for the parts of Context that TclRuntime uses. */
public class Context {
    public static String nativeLibDir, filesDir, cacheDir;
    public ApplicationInfo getApplicationInfo() { ApplicationInfo a = new ApplicationInfo(); a.nativeLibraryDir = nativeLibDir; return a; }
    public File getFilesDir() { return new File(filesDir); }
    public File getCacheDir() { return new File(cacheDir); }
}
