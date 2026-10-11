package org.timecrisis.quest;

import android.app.Activity;
import android.graphics.Typeface;
import android.view.View;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;
import java.util.List;

/** Approved stacked setup/lobby components. Transport stays in the controller. */
public final class CoopLauncherUi {
    public interface Listener {
        void solo(); void host(); void join(); void refresh();
        void connect(String address); void start(); void back();
    }
    public static final class Host {
        public final String address, label;
        public final boolean compatible;
        public Host(String address, String label, boolean compatible) { this.address = address; this.label = label; this.compatible = compatible; }
    }
    private final Activity activity;
    private final boolean frame;
    private final Listener listener;
    private LinearLayout body;
    private TextView status, addressDisplay;
    private Button start;
    private String enteredAddress = "";

    public CoopLauncherUi(Activity activity, boolean frame, Listener listener) {
        this.activity = activity; this.frame = frame; this.listener = listener;
    }
    private int dp(int value) {
        return Math.round(value * activity.getResources().getDisplayMetrics().density);
    }
    private void screen(String title, String subtitle) {
        ScrollView scroll = new ScrollView(activity);
        body = new LinearLayout(activity);
        body.setOrientation(LinearLayout.VERTICAL); body.setPadding(dp(24),dp(20),dp(24),dp(20));
        scroll.addView(body, new ScrollView.LayoutParams(-1,-2));
        text(title,24,true); text(subtitle,16,false);
        status = null; start = null; addressDisplay = null;
        activity.setContentView(scroll);
    }
    private TextView text(String value, int size, boolean bold) {
        TextView text = new TextView(activity); text.setText(value); text.setTextSize(size);
        if (bold) text.setTypeface(Typeface.DEFAULT,Typeface.BOLD);
        text.setPadding(0,dp(6),0,dp(6)); body.addView(text); return text;
    }
    private Button button(String label, Runnable action) {
        Button button = new Button(activity); button.setText(label); button.setAllCaps(false);
        button.setMinHeight(dp(48)); body.addView(button,new LinearLayout.LayoutParams(-1,-2));
        button.setOnClickListener(v -> action.run()); return button;
    }
    private void rules() {
        text("Shared health, ammo and cover.",16,true);
        text("Either player gripping leaves cover. Release both players’ grips to hide and reload.",15,false);
    }
    public void modes() {
        screen("Time Crisis VR",frame ? "Steam Frame" : "Quest");
        text("Game files verified",16,false);
        button("Play solo",listener::solo);
        button("Host co-op · P1",listener::host);
        button("Join co-op · P2",listener::join);
        status = text("",16,false);
        status.setAccessibilityLiveRegion(View.ACCESSIBILITY_LIVE_REGION_POLITE);
        rules();
    }
    public void host(String address) {
        screen("Host co-op · P1","New shared run");
        text("Host address: " + address,17,true);
        text("P1 · " + (frame ? "Steam Frame" : "Quest") + " · Ready",17,true);
        TextView peer = text("P2 · Waiting…",17,true); peer.setTag("coop-peer");
        status = text("Ask your partner to choose Join co-op on the same Wi-Fi.",16,false);
        status.setAccessibilityLiveRegion(View.ACCESSIBILITY_LIVE_REGION_POLITE);
        start = button("Start shared run",listener::start); start.setEnabled(false);
        button("Back",listener::back);
        text("Both players start together. A lost connection ends this shared run.",15,false);
    }
    public void hostStatus(String detail, boolean ready) {
        if (status != null) status.setText(detail);
        if (start != null) start.setEnabled(ready);
        TextView peer = body.findViewWithTag("coop-peer");
        if (peer != null) peer.setText(ready ? "P2 · Ready" : "P2 · Waiting…");
    }
    public void join(List<Host> hosts, String detail) {
        screen("Join co-op · P2","Same Wi-Fi as the host");
        for (Host host : hosts) {
            Button entry = button(host.label + "\n" + host.address + (host.compatible ? " · Join" : " · Incompatible"),() -> listener.connect(host.address));
            entry.setEnabled(host.compatible);
        }
        status = text(detail,16,false);
        status.setAccessibilityLiveRegion(View.ACCESSIBILITY_LIVE_REGION_POLITE);
        button("Refresh",listener::refresh);
        text("Or enter the host address",15,false);
        addressDisplay = text(enteredAddress.isEmpty() ? "Host IPv4 address" : enteredAddress,20,true);
        addressDisplay.setContentDescription("Host address");
        String[][] keys = {{"1","2","3"},{"4","5","6"},{"7","8","9"},{".","0","⌫"}};
        for (String[] row : keys) {
            LinearLayout line = new LinearLayout(activity); line.setOrientation(LinearLayout.HORIZONTAL);
            body.addView(line,new LinearLayout.LayoutParams(-1,-2));
            for (String key : row) {
                Button button = new Button(activity); button.setText(key); button.setMinHeight(dp(48));
                if (key.equals("⌫")) button.setContentDescription("Delete last character");
                line.addView(button,new LinearLayout.LayoutParams(0,-2,1));
                button.setOnClickListener(v -> {
                    if (key.equals("⌫")) {
                        if (!enteredAddress.isEmpty()) enteredAddress=enteredAddress.substring(0,enteredAddress.length()-1);
                    } else if (enteredAddress.length()<15) enteredAddress += key;
                    addressDisplay.setText(enteredAddress.isEmpty() ? "Host IPv4 address" : enteredAddress);
                });
            }
        }
        button("Connect to host",() -> listener.connect(enteredAddress));
        button("Back",listener::back);
    }
    public void joining(String address, String detail) {
        screen("Join co-op · P2",address);
        status = text(detail,17,false);
        status.setAccessibilityLiveRegion(View.ACCESSIBILITY_LIVE_REGION_POLITE);
        button("Back",listener::back); rules();
    }
    public void status(String detail) { if (status != null) status.setText(detail); }
}
