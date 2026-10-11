package org.timecrisis.quest;

import android.app.Activity;
import android.os.Handler;
import android.os.Looper;
import java.io.File;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import org.json.JSONObject;

/** Setup controller: networking runs off the UI thread; the lobby owns no game state. */
public final class CoopSessionController implements CoopLauncherUi.Listener {
    private static final int TCP_PORT = 27009, DISCOVERY_PORT = 27010;
    private final Activity activity;
    private final File romDirectory;
    private final Runnable launchGame;
    private final CoopLauncherUi ui;
    private final Handler main = new Handler(Looper.getMainLooper());
    private final ExecutorService work = Executors.newSingleThreadExecutor();
    private int generation;
    private boolean destroyed, launching;
    private String screen = "modes";
    private List<CoopLauncherUi.Host> hosts = Collections.emptyList();
    private String lastStatus = "";

    public CoopSessionController(Activity activity, boolean frame, File romDirectory, Runnable launchGame) {
        this.activity = activity; this.romDirectory = romDirectory; this.launchGame = launchGame;
        ui = new CoopLauncherUi(activity,frame,this); ui.modes();
    }
    private boolean current(int token) { return !destroyed && !launching && token == generation; }
    private void post(int token, Runnable update) {
        main.post(() -> { if (current(token)) update.run(); });
    }
    private String message(Exception error) {
        String value = error.getMessage(); return value == null ? error.toString() : value;
    }
    private void launch() {
        if (destroyed || launching) return;
        launching = true; ++generation; main.removeCallbacks(poll);
        try { launchGame.run(); }
        catch (RuntimeException error) {
            launching = false; screen = "modes"; ui.modes();
            ui.status("Cannot start Time Crisis VR: " + message(error));
        }
    }
    private final Runnable poll = new Runnable() {
        @Override public void run() {
            if (destroyed || launching || !(screen.equals("host") || screen.equals("joining"))) return;
            try {
                JSONObject state = new JSONObject(CoopLobbyBridge.statusJson());
                String phase = state.optString("state"), detail = state.optString("message");
                if (phase.equals("starting")) { launch(); return; }
                if (screen.equals("host")) ui.hostStatus(detail,state.optBoolean("canStart"));
                else if (!detail.equals(lastStatus)) ui.status(detail);
                lastStatus = detail;
            } catch (Exception error) { ui.status("Connection status unavailable: " + message(error)); }
            main.postDelayed(this,200);
        }
    };
    @Override public void solo() {
        if (destroyed || launching) return;
        int token = ++generation;
        work.execute(() -> { CoopLobbyBridge.selectSolo(); post(token,this::launch); });
    }
    @Override public void host() {
        if (destroyed || launching) return;
        int token = ++generation; screen="host"; main.removeCallbacks(poll);
        ui.host("Checking network…");
        work.execute(() -> {
            try {
                CoopLobbyBridge.cancel();
                boolean started = CoopLobbyBridge.startHost(romDirectory.getAbsolutePath(),TCP_PORT,DISCOVERY_PORT);
                String[] addresses = CoopLobbyBridge.localIpv4Addresses();
                String address = addresses.length == 0 ? "No local address" : String.join(" / ",addresses);
                post(token,() -> { ui.host(address); if (!started) ui.hostStatus("Cannot host: check Wi-Fi and the connection status.",false); main.post(poll); });
            } catch (Exception error) { post(token,() -> ui.hostStatus("Cannot host: " + message(error),false)); }
        });
    }
    @Override public void join() {
        if (destroyed || launching) return;
        ++generation; screen="join"; main.removeCallbacks(poll);
        ui.join(hosts,"Finding hosts on your Wi-Fi…"); refresh();
    }
    @Override public void refresh() {
        if (destroyed || launching || !screen.equals("join")) return;
        int token = ++generation; ui.status("Finding hosts on your Wi-Fi…");
        work.execute(() -> {
            try {
                String[] found = CoopLobbyBridge.discover(romDirectory.getAbsolutePath(),DISCOVERY_PORT,800);
                List<CoopLauncherUi.Host> result = new ArrayList<>();
                for (String entry : found) {
                    JSONObject host = new JSONObject(entry);
                    String address = host.optString("ipv4");
                    if (!address.isEmpty()) result.add(new CoopLauncherUi.Host(address,
                            host.optString("label",host.optString("name","Time Crisis host")),host.optBoolean("compatible")));
                }
                post(token,() -> { hosts=result; ui.join(hosts,hosts.isEmpty() ? "No hosts found. Refresh or enter the host address." : hosts.size()+" host(s) found"); });
            } catch (Exception error) { post(token,() -> ui.status("Host search failed: " + message(error))); }
        });
    }
    @Override public void connect(String address) {
        if (destroyed || launching) return;
        if (!validIpv4(address)) { ui.status("Enter four IPv4 numbers from 0 to 255, separated by dots."); return; }
        int token=++generation; screen="joining"; main.removeCallbacks(poll);
        ui.joining(address,"Connecting to host…");
        work.execute(() -> {
            try {
                CoopLobbyBridge.cancel();
                boolean started=CoopLobbyBridge.startJoin(romDirectory.getAbsolutePath(),address,TCP_PORT,DISCOVERY_PORT);
                post(token,() -> { if (!started) ui.status("Cannot connect: check the host address and Wi-Fi."); main.post(poll); });
            } catch (Exception error) { post(token,() -> ui.status("Cannot connect: " + message(error))); }
        });
    }
    @Override public void start() {
        if (destroyed || launching || !screen.equals("host")) return;
        int token=generation;
        work.execute(() -> {
            boolean started=CoopLobbyBridge.hostStart();
            post(token,() -> { if (started) main.post(poll); else ui.hostStatus("Your partner is not ready. Wait or create a new session.",false); });
        });
    }
    @Override public void back() {
        if (destroyed || launching) return;
        int token=++generation; main.removeCallbacks(poll);
        if (screen.equals("modes")) { activity.finish(); return; }
        screen="modes"; ui.modes();
        work.execute(() -> { CoopLobbyBridge.cancel(); post(token,() -> {}); });
    }
    public void destroy() {
        if (destroyed) return;
        destroyed=true; ++generation; main.removeCallbacks(poll);
        if (!launching) work.execute(CoopLobbyBridge::cancel);
        work.shutdown();
    }
    static boolean validIpv4(String address) {
        if (address == null || address.isEmpty() || address.length()>15) return false;
        String[] parts=address.split("\\.",-1);
        if (parts.length!=4) return false;
        for (String part : parts) {
            if (part.isEmpty() || part.length()>3 || (part.length()>1 && part.charAt(0)=='0')) return false;
            for (int i=0;i<part.length();i++) if (part.charAt(i)<'0' || part.charAt(i)>'9') return false;
            if (Integer.parseInt(part)>255) return false;
        }
        return !address.equals("0.0.0.0") && !address.equals("255.255.255.255");
    }
}
