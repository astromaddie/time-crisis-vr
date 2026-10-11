package org.timecrisis.frame;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.provider.Settings;
import android.widget.*;
import org.timecrisis.quest.RomInstaller;
import java.io.*;
import java.util.Map;

/** Small 2D setup step; the user supplies a ZIP at the fixed persistent location. */
public final class LauncherActivity extends Activity {
    private TextView status;
    private org.timecrisis.quest.CoopSessionController session;
    private Button retry, storage;

    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        LinearLayout layout = new LinearLayout(this); layout.setOrientation(LinearLayout.VERTICAL); layout.setPadding(40,40,40,40);
        TextView title = new TextView(this); title.setText("Time Crisis VR — Steam Frame Setup"); title.setTextSize(24); layout.addView(title);
        status = new TextView(this); status.setTextSize(17); status.setPadding(0,24,0,24); layout.addView(status);
        storage = new Button(this); storage.setText("Open storage access settings"); layout.addView(storage);
        retry = new Button(this); retry.setText("Check files and start"); layout.addView(retry);
        storage.setOnClickListener(v -> openStorageSettings());
        retry.setOnClickListener(v -> prepare()); setContentView(layout); prepare();
    }

    private void openStorageSettings() {
        if (Build.VERSION.SDK_INT < 30) return;
        try {
            startActivity(new Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION,
                    Uri.parse("package:" + getPackageName())));
        } catch (Exception e) {
            status.setText("Open Android Settings and allow file access for Time Crisis VR. " + e.getMessage());
        }
    }

    private void launchGame() {
        startActivity(new Intent(this, MainActivity.class)
                .setAction(Intent.ACTION_MAIN)
                .addCategory("org.khronos.openxr.intent.category.IMMERSIVE_HMD")
                .putExtra("org.timecrisis.COOP_PREPARED",true)
                .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK));
        finish();
    }
    @Override public void onBackPressed() {
        if (session != null) session.back(); else super.onBackPressed();
    }
    @Override protected void onDestroy() {
        if (session != null) session.destroy();
        super.onDestroy();
    }

    private void prepare() {
        retry.setEnabled(false); storage.setVisibility(android.view.View.GONE);
        status.setText("Checking persistent game files in /sdcard/Documents/TimeCrisisVR…");
        new Thread(() -> {
            try {
                Map<String,String> expected = RomInstaller.manifest(getAssets().open("roms.sha256"));
                FrameStorage.prepareRoms(FrameStorage.ROOT, expected);
                runOnUiThread(() -> {
                    if (isFinishing() || isDestroyed()) return;
                    if (new File(FrameStorage.ROOT,"vrdiag.once").isFile()) {
                        org.timecrisis.quest.CoopLobbyBridge.selectSolo();
                        launchGame();
                    } else {
                        session = new org.timecrisis.quest.CoopSessionController(this,true,
                                FrameStorage.roms(FrameStorage.ROOT),this::launchGame);
                    }
                });
            } catch (Exception e) {
                String detail = e.getMessage() == null ? e.toString() : e.getMessage();
                runOnUiThread(() -> {
                    if (isFinishing() || isDestroyed()) return;
                    status.setText(detail + "\n\nGame files are not included. Place your own verified TS2 Ver.B timecris.zip at /sdcard/Documents/TimeCrisisVR/timecris.zip.");
                    storage.setVisibility(Build.VERSION.SDK_INT >= 30 ? android.view.View.VISIBLE : android.view.View.GONE);
                    retry.setEnabled(true);
                });
            }
        }, "Frame-ROM-import").start();
    }
}
