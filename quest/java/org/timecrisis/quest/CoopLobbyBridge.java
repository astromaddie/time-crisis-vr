package org.timecrisis.quest;

/** Narrow, process-local bridge between the approved launcher controller and the native LAN session. */
public final class CoopLobbyBridge {
    private static final String LOAD_ERROR;
    static {
        String problem = null;
        try {
            // Match MainActivity's native library order. Loading these libraries does not create an XR session.
            System.loadLibrary("SDL2");
            System.loadLibrary("openxr_loader");
            System.loadLibrary("main");
        } catch (LinkageError error) {
            problem = "The LAN service could not load its required game library.";
        }
        LOAD_ERROR = problem;
    }
    private CoopLobbyBridge() {}

    public static final int DEFAULT_TCP_PORT = 27009;
    public static final int DEFAULT_DISCOVERY_PORT = 27010;

    public static boolean startHost(String romDir, int tcpPort, int discoveryPort) {
        return LOAD_ERROR == null && nativeStartHost(romDir, tcpPort, discoveryPort);
    }
    public static boolean startJoin(String romDir, String ipv4, int tcpPort, int discoveryPort) {
        return LOAD_ERROR == null && nativeStartJoin(romDir, ipv4, tcpPort, discoveryPort);
    }
    public static String statusJson() {
        return LOAD_ERROR == null ? nativeStatusJson()
                : "{\"state\":\"error\",\"role\":\"none\",\"peerIpv4\":\"\",\"compatible\":false,\"canStart\":false,\"message\":\"" + LOAD_ERROR + "\",\"tcpPort\":27009,\"protocolVersion\":1}";
    }
    public static boolean hostStart() { return LOAD_ERROR == null && nativeHostStart(); }
    /** Returns one JSON object string per discovered host. */
    public static String[] discover(String romDir, int discoveryPort, int timeoutMs) {
        return LOAD_ERROR == null ? nativeDiscover(romDir, discoveryPort, timeoutMs) : new String[0];
    }
    /** Returns usable local IPv4 address strings for the host lobby. */
    public static String[] localIpv4Addresses() {
        return LOAD_ERROR == null ? nativeLocalIpv4Addresses() : new String[0];
    }
    /** Stops an unfinished or running lobby without persisting a launch mode. */
    public static void cancel() { if (LOAD_ERROR == null) nativeCancel(); }
    /** Cancels any lobby and marks an intentional Solo selection for this process. */
    public static void selectSolo() { if (LOAD_ERROR == null) nativeSelectSolo(); }

    private static native boolean nativeStartHost(String romDir, int tcpPort, int discoveryPort);
    private static native boolean nativeStartJoin(String romDir, String ipv4, int tcpPort, int discoveryPort);
    private static native String nativeStatusJson();
    private static native boolean nativeHostStart();
    private static native String[] nativeDiscover(String romDir, int discoveryPort, int timeoutMs);
    private static native String[] nativeLocalIpv4Addresses();
    private static native void nativeCancel();
    private static native void nativeSelectSolo();
}
