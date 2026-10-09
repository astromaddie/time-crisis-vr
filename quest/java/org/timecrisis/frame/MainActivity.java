package org.timecrisis.frame;

import android.os.Bundle;
import org.libsdl.app.SDLActivity;
import org.timecrisis.quest.RomInstaller;
import java.io.*;
import java.security.MessageDigest;
import java.util.Map;

public final class MainActivity extends SDLActivity {
    @Override protected String[] getLibraries() { return new String[]{"SDL2", "openxr_loader", "main"}; }

    @Override public void onCreate(Bundle state) {
        try {
            Map<String,String> romManifest = RomInstaller.manifest(getAssets().open("roms.sha256"));
            FrameStorage.prepareRoms(FrameStorage.ROOT, romManifest);
            installAssets("models");
        } catch (Exception e) {
            throw new IllegalStateException("Frame storage preparation failed: " + e.getMessage(), e);
        }
        super.onCreate(state);
    }

    private void installAssets(String category) throws Exception {
        File dir = new File(FrameStorage.ROOT, category);
        if (!dir.isDirectory() && !dir.mkdirs()) throw new IOException("Cannot create " + dir);
        Map<String,String> expected = RomInstaller.manifest(getAssets().open(category + ".sha256"));
        for (Map.Entry<String,String> item : expected.entrySet()) {
            String name = item.getKey(); File dest = new File(dir, name);
            if (dest.isFile() && digest(dest).equals(item.getValue())) continue;
            File tmp = new File(dir, name + ".tmp");
            try (InputStream in = getAssets().open(category + "/" + name); FileOutputStream out = new FileOutputStream(tmp)) {
                byte[] buffer = new byte[65536]; int n;
                while ((n = in.read(buffer)) != -1) out.write(buffer, 0, n);
                out.getFD().sync();
            }
            if (!digest(tmp).equals(item.getValue())) throw new IOException("Asset checksum: " + name);
            if (!tmp.renameTo(dest)) throw new IOException("Cannot install " + dest);
        }
    }

    private String digest(File file) throws Exception {
        MessageDigest d = MessageDigest.getInstance("SHA-256");
        try (InputStream in = new FileInputStream(file)) { byte[] b = new byte[65536]; int n; while ((n = in.read(b)) != -1) d.update(b,0,n); }
        StringBuilder s = new StringBuilder(); for (byte b : d.digest()) s.append(String.format("%02x",b&255)); return s.toString();
    }
}
