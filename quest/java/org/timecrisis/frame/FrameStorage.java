package org.timecrisis.frame;

import org.timecrisis.quest.RomInstaller;
import java.io.File;
import java.io.IOException;
import java.util.Map;

/** Persistent shared data used by the Steam Frame installer and native process. */
public final class FrameStorage {
    public static final File ROOT = new File("/sdcard/Documents/TimeCrisisVR");
    private FrameStorage() {}

    public static File archive(File root) { return new File(root, "timecris.zip"); }
    public static File roms(File root) { return new File(root, "roms"); }
    public static File models(File root) { return new File(root, "models"); }

    public static File ensureWritable(File root) throws IOException {
        if (!root.isDirectory() && !root.mkdirs())
            throw new IOException("Cannot create persistent storage directory: " + root);
        File probe = null;
        try {
            probe = File.createTempFile(".tcvr-write-", ".tmp", root);
        } catch (IOException e) {
            throw new IOException("Cannot write " + root + "; grant Time Crisis VR storage access", e);
        } finally {
            if (probe != null && probe.exists() && !probe.delete()) probe.deleteOnExit();
        }
        return root;
    }

    /** Import directly from the persistent root; chip matching is hash-based, not filename-based. */
    public static File prepareRoms(File root, Map<String,String> expected) throws IOException {
        ensureWritable(root);
        File directory = roms(root);
        if (RomInstaller.ready(directory, expected)) return directory;
        File zip = archive(root);
        if (!zip.isFile())
            throw new IOException("Missing " + zip + ". Copy your own timecris.zip there, then retry.");
        try {
            RomInstaller.importZip(zip, directory, expected);
        } catch (IOException e) {
            throw new IOException("Could not import " + zip + ": " + e.getMessage(), e);
        }
        return directory;
    }
}
