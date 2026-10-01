# v0.1.0-beta.4

**Required Bully version: Bully Scholarship Edition for PC, version 1.200, with a compatible native engine layout.** Steam-wrapped executable code is checked after normal Steam startup; matching unwrapped PC layouts remain supported. You also need your own complete, extracted Xbox 360 Skate 3 files.

**Players: [Download the Windows ZIP](https://github.com/Faiqie/BullySkate/releases/download/v0.1.0-beta.4/BullySkate-Windows-v0.1.0-beta.4.zip).** Extract it, open `BullySkateLauncher.exe`, and select your own game files when asked. This is the only player download needed; setup helpers, instructions and licenses are included.

**Source code is separate:** use GitHub's **Source code (zip)** or **Source code (tar.gz)** downloads below, or browse/clone [the repository](https://github.com/Faiqie/BullySkate). The player ZIP does not include source code. SHA-256 checksums are inside the player ZIP.

This update handles Steam-wrapped Bully executables that could report 0/203 file code matches despite reaching the normal game menu. The launcher recognizes the wrapper, validates native data mappings and the selected Steam installation, installs automatically, and starts the game through Steam. Users keep using the launcher: select Bully.exe and the complete Skate 3 folder once, then open the same launcher to play. No manual installer or replacement game executable is required.

The ASI verifies all 203 code fingerprints and 19 data mappings in memory before installing its first hook. It skips hooks and records a compatibility log if the loaded engine does not match. Checks run once at startup. Existing saved paths, skater preferences and unrelated mods are preserved. A one-use setting applies the display-scaling fix to Steam-created game processes too.

Optional `CheckBullyRunning.cmd` opens the launcher in read-only loaded-engine diagnostic mode. Existing `CheckBully.cmd` still checks the file. Reports include match counts, section names and SilentPatch detection without personal paths or game code; nothing is uploaded automatically. See [compatibility and diagnosis](https://github.com/Faiqie/BullySkate/blob/main/docs/COMPATIBILITY.md).

- Windows file-selection windows for Bully.exe and default.xex, with a terminal for progress and saved setup thereafter.
- Display-scaling correction before the game starts, with an optional `--no-dpi-fix` troubleshooting switch.
- First-run local extraction of Bully collision, area rails, Jimmy bind rig and car bounds, plus verified Skate animation/physics/input/camera data. No game data is distributed.
- Native Jimmy and skateboard rig retargeting, source skating and controller input, Edit Skater and 40â€“110Â° skating FOV.
- Session markers, board strikes and adapter-assisted skitching, area collision separation and asynchronous simulation submission.
- Native display path, preference-preserving install/repair, recoverable backups and disable helper.
- Separate portable source download, pinned upstream provenance and build workflow; player ZIP with setup guide, controls, troubleshooting and checksums.

Tests cover wrapper recognition, file/loaded-code differences, altered runtime code, native SHA-256 vectors, Steam manifest selection, URI construction and saved launcher setup. The actual ASI passes its full startup check in the owner's local 1.200 game, consumes the Steam DPI hint and skips hooks in an unrelated x86 host. Existing Windows display-scaling tests pass. Stock Steam gameplay, every retail executable, the reported 4K PC and a physical multi-monitor/scaling matrix remain unverified. Matching 1.200 layouts are eligible; this release does not claim universal compatibility.

The upstream rewrite is incomplete. This is not a claim of retail Skate 3 equivalence, guaranteed FPS, independently spinning wheels, or compatibility with every mission, outfit, interior, world overhaul or other ASI. Read docs/VALIDATION.md before reporting a bug. No gun mode is included.
