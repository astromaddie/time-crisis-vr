# Attribution and license scope

Time Crisis VR is an unofficial community port for Meta Quest 3 and Windows. It is not
affiliated with or endorsed by Namco, Bandai Namco, or Meta.

- **VR port code:** DR-89 and contributors, MIT; see [LICENSE](LICENSE).
- **Underlying reconstruction and shared engine:** the `upstream/` submodule
  points to
  [spacestate1/namco22-decompile](https://github.com/spacestate1/namco22-decompile)
  at `6aaa90b4cbc7a23733e1c9f5f9a5e772a19fe23d`. Its MIT notice credits
  **Copyright (c) 2026 cmcrann** and is preserved in the submodule. The
  submodule includes the reconstructed engine and generated C translations.
  The original upstream source notice is retained. Game ROMs, graphics and
  audio are separate user-supplied inputs; the source license does not grant
  redistribution rights to those files.
- **SDL 2.30.11:** zlib license; original notice included in the APK.
- **Khronos OpenXR loader 1.1.43:** its original license notice is included in the APK.
- **Windows OpenGL loader:** generated with [glad 2.0.8](https://github.com/Dav1dde/glad);
  generated code and Khronos declarations retain their notices in `pc/vendor/glad`.
- **Windows dependencies:** zlib 1.3.1, SDL 2.30.11, Khronos OpenXR 1.1.43,
  LLVM/MinGW runtime and winpthreads. Their notices are included under `licenses`
  in the portable Windows package.
- **Player weapon model:** [Lowpoly Pistol by FireWarden](https://opengameart.org/content/lowpoly-pistol),
  released under CC0. Imported from the user-supplied QuestRetroDepth header, with
  separate body, trigger and slide parts. Provenance, geometry and checksums are
  recorded in `quest/assets/models/player-gun.json`. The model is not extracted
  from Time Crisis. The earlier Tripo3D model remains in the Git history.
- **Time Crisis, original game program, graphics, sound, ROM contents and trademarks:**
  belong to their respective original rights holders. They are not covered by this
  project's MIT license. The Steam Frame APK excludes the ROM and imports it
  from the user's own copy. Any other package containing original game files
  needs separate redistribution rights.

The repository contains the port and build/import tools plus a public `upstream/`
source submodule. ROM archives, extracted chips, local build output and APKs are
excluded from the repository. The Frame package was checked with no ROM bundled.
Keep original game data separate from any public source or package.
