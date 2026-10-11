# Local LAN co-op

**Unfinished development branch: `steam-frame-multiplayer`. Do not treat this as a playable multiplayer release.** `steam-frame` contains the solo port.

## Required multiplayer behavior

P1 and P2 must have **independent health, ammunition, damage, cover and reload** while shooting the same enemies in one synchronized world. Each player's trigger uses that player's aim and ammunition; hiding or reloading must not hide or reload the other player. An enemy attack must have a deterministic target fixed before impact, and only that target's exposure, invulnerability and health determine its damage. A player dying must not end the surviving player's run. The shared rail progression and original P1 cutscene actor may remain.

The existing single-cabinet arbitration does not implement these rules. Its shared health/ammo/cover design is obsolete. Separate HUD counters would not satisfy the requirement: actual game shot acceptance, hit processing, damage and death must use the appropriate player's state.

## Observed failure and proof required

In the paired wearer test, the Frame hosted P1 and Quest 3 joined P2. Cutscenes synchronized, but P2 ducking/hiding changed P1's view; P2 shooting did not work and no damage was received. The absence of a P2 cutscene actor was accepted. Launch/focus fixes and network tests do not establish multiplayer gameplay.

Private tests through the real translated game show that an exposed player loses health while the same run in cover retains it. Original shot, reload, damage and cover routines exist, but safe per-player ownership and presentation are not yet proven. Before implementing a full combat adapter, a real encounter must demonstrate P2 damaging/killing a shared enemy, independent ammunition/reload, P1 exposed taking damage with P2 protected, the reverse case, and continued play while one player is dead. Independent enemy targeting, cover camera/HUD state and script transitions also need verification.

The bounded original-game encounter check did not establish a safe two-player adapter. Exposed health fell from three to zero; the resulting death reaction changed the global game mode and ended the shared stage script. The covered comparison retained three health and zero damage. Both runs completed 4,500 frames without traps. Cover also affects enemy update scheduling and the shared camera used for aiming. Stable identity was observed for one projectile class, but other attacks and independent player death/presentation remain unmapped. Multiplayer remains unfinished; no independent-player build is being delivered.

Keep this feature out of `steam-frame` until those checks and paired-headset play pass. If the game cannot support these rules reliably, remove the multiplayer feature. The following sections document the historical prototype, not the approved behavior.

The co-op implementation aims to let Steam Frame and Quest 3 share one fresh Time Crisis run over the same local Wi-Fi network. The headset that hosts controls P1; the joining headset controls P2. The game and simulation run on both headsets. The Frame coordinates one ordered arcade input stream so both simulations consume the same input at each `rr_frame`; the joiner waits at a tick boundary when the next canonical input has not arrived. No PC, cloud account, or remote avatar is involved.

## Launch and lobby

After the ROM archive has been verified, choose **Play solo**, **Host co-op · P1**, or **Join co-op · P2**. Hosting displays the headset's local IPv4 address and enables **Start shared run** only after a compatible partner is ready. Joining searches local broadcast addresses and also accepts a manually entered IPv4 address. Both headsets must use matching simulation/protocol code and the same 31-chip ROM manifest. The Frame and Quest renderer binaries are expected to differ; they are deliberately excluded from the compatibility identity.

On Steam Frame, the setup activity automatically enters SDL/OpenXR after ROM validation. Solo/Host/Join is a native stereo panel before game initialization; it does not wait for a flat Android lobby. Use the right stick to navigate, A to select, B or a short View press to go back, or aim the default weapon hand at a button and pull its trigger. View recenter gestures also reposition the panel. Release both triggers and grips after choosing a mode before gameplay input is armed. Hosting, joining, discovery and cancellation run off the render thread. The already-created XR session is reused when the game attaches after selection.

Quest retains its Android setup/lobby flow. This Frame startup repair leaves protocol 1, the simulation identity and the ROM manifest identity unchanged; it works with the matching Quest APK already supplied. Ordinary Frame startup writes `timecris-vr-launch.log`, including `[vr] native launch lobby ready: Solo / Host / Join`; no diagnostic override is required. Headset rendering and physical controller operation need their own device acceptance evidence.

The native bridge exposes nonblocking `statusJson()` states (`idle`, `host_waiting`, `connecting`, `ready`, `starting`, `running`, `paused`, and `error`). Status includes `role`, `peerIpv4`, `compatible`, `canStart`, `message`, `tcpPort`, and `protocolVersion`. Discovery returns `ipv4`, `address`, `name`, `label`, `tcpPort`, `protocolVersion`, and `compatible`. The transport uses TCP port **27009** and UDP broadcast discovery port **27010** by default. A direct IPv4 join works when discovery broadcasts are filtered by the access point.

The host starts both game engines together at frame zero. The host's current EEPROM image is sent in memory at the start barrier, and the joiner's copy is read-only for that run. Co-op starts only after both engines have attached and reached the same barrier. Late joins, mid-run recovery, diagnostic replay, synthetic-input overrides, and recording playback are not supported; after a disconnect or stalled exchange, return to the launcher and start a new session.

## Historical shared-play rules (obsolete)

Both headsets share the single arcade player's health and ammunition. Cover is shared: if either player is exposed, the arcade player is exposed for that tick. A shot is accepted only when its originating player was exposed on that trigger edge, and it uses that player's captured aim and off-screen state. When both players are covered, a trigger-only pulse still reaches the arcade so either player can start or confirm menus; while either player is exposed, covered players' trigger edges cannot fire. Simultaneous shots are ordered fairly. Both players' coin inputs contribute to the shared machine. Releasing both players' grips hides in cover and reloads; either player may hold a grip to leave cover. The game screen and cabinet state are shared without rendering a remote player avatar.

Losing focus pauses both simulations at a tick boundary. The run resumes only when both players are focused, after pending shots are cleared and triggers are neutral-rearmed. A socket disconnect or a running exchange that stalls for five seconds stops the shared run and displays a readable error in the in-game pause panel. Co-op disables input recording and writes diagnostics to `timecris-vr-coop.log`; it does not rotate or overwrite Solo recordings, logs, settings, or EEPROM progress. An explicit Solo selection clears the process-local co-op request; ordinary Solo play does not inherit a saved co-op mode.

## ROM-free builds

Run both commands on macOS from the repository root, with JDK 17, CMake, Ninja, Android platform 34, build-tools 35.0.0, and NDK 27.2.12479018 already available:

```sh
python3 tools/build.py --rom "/path/to/timecris.zip" --jobs 4
python3 tools/build_frame.py --rom "/path/to/timecris.zip" --jobs 4
```

The first command builds `artifacts/TimeCrisisVR-quest3-debug.apk`; the second builds `artifacts/TimeCrisisVR-SteamFrame-arm64.apk`. Each packages the verified ROM manifest and model only, never the ROM archive, extracted chips, generated game source, or signing key. Matching build receipts record the same nonzero `simulation_sha256`, `rom_manifest_sha256`, and protocol version even though target-specific renderer code and APK hashes differ. Compare those identity values before sharing a lobby.

## Verification status

Host tests exercise the real TCP/UDP lobby, compatibility handshake, fresh EEPROM barrier, per-tick input ordering, cover/shot arbitration, pause/resume, disconnect, stalled-but-open socket watchdog, launcher routing, EEPROM write guard, and Solo recording history. Quest and Frame production HUD raster tests cover the paused and failed-session status panel. These checks do not establish reliable shared gameplay on the two headsets. Multiplayer remains unfinished.
