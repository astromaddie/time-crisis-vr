# Attribution and license scope

Time Crisis VR is an unofficial community port for Meta Quest 3 and Windows. It is not
affiliated with or endorsed by Namco, Bandai Namco, or Meta.

- **VR port code:** DR-89 and contributors, MIT; see [LICENSE](LICENSE).
- **Underlying reconstruction and shared engine:** the `upstream/` submodule
  points to
  [spacestate1/namco22-decompile](https://github.com/spacestate1/namco22-decompile)
  at `6aaa90b4cbc7a23733e1c9f5f9a5e772a19fe23d`. Its MIT notice credits
  **Copyright (c) 2026 cmcrann** and is preserved in the submodule. The
  submodule history and pinned tree also contain large generated C translations
  for multiple commercial games. This repository has not established rights to
  publish those generated translations or a combined binary; its MIT notice
  does not resolve that question. Treat the reconstruction dependency's source
  and generated outputs as rights-unreviewed.
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
  project's MIT license. Bundled APK and Windows releases include these original game files;
  a ROM-free APK requires the user to supply them separately.

The repository contains the port and build/import tools plus an `upstream/`
submodule link. Initializing that submodule fetches the generated source tree
described above; its publication rights remain unresolved. ROM archives,
extracted ROMs, local build output and APKs are excluded from the main repository.
This notice does not authorize distributing APKs or Windows packages.
