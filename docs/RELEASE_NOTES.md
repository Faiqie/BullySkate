# v0.1.0-beta.2

**Required Bully version: Bully Scholarship Edition for PC, version 1.200 SP Build 3.** The launcher verifies your `Bully.exe` and rejects other executable versions. You also need your own complete, extracted Xbox 360 Skate 3 files.

**Players: [Download the Windows ZIP](https://github.com/Faiqie/BullySkate/releases/download/v0.1.0-beta.2/BullySkate-Windows-v0.1.0-beta.2.zip).** Extract it, open `BullySkateLauncher.exe`, and select your own game files when asked. This is the only player download needed; setup helpers, instructions and licenses are included.

**Source code is separate:** use GitHub's **Source code (zip)** or **Source code (tar.gz)** downloads below, or browse/clone [the repository](https://github.com/Faiqie/BullySkate). The player ZIP does not include source code. SHA-256 checksums are inside the player ZIP.

This update adds automatic Windows display-scaling correction for enlarged or cropped game windows/HUDs on high-DPI screens, including typical 4K setups. File-selection windows are also per-monitor DPI aware. Choose your rendering resolution in Bully's graphics settings; the launcher keeps that choice.

- Windows file-selection windows for Bully.exe and default.xex, with a terminal for progress and saved setup thereafter.
- Display-scaling correction before the game starts, with an optional `--no-dpi-fix` troubleshooting switch.
- First-run local extraction of Bully collision, area rails, Jimmy bind rig and car bounds, plus verified Skate animation/physics/input/camera data. No game data is distributed.
- Native Jimmy and skateboard rig retargeting, source skating and controller input, Edit Skater and 40–110° skating FOV.
- Session markers, board strikes and adapter-assisted skitching, area collision separation and asynchronous simulation submission.
- Native display path, preference-preserving install/repair, recoverable backups and disable helper.
- Separate portable source download, pinned upstream provenance and build workflow; player ZIP with setup guide, controls, troubleshooting and checksums.

Windows process tests verify the launcher's per-monitor DPI awareness, six real child-process compatibility cases, fullscreen/windowed launch settings, and the troubleshooting opt-out. Existing setup/payload checks also pass. Gameplay on the reported 4K PC and a physical multi-monitor/scaling matrix remain unverified; please report your resolution and Windows Scale setting if clipping persists.

The upstream rewrite is incomplete. This is not a claim of retail Skate 3 equivalence, guaranteed FPS, independently spinning wheels, or compatibility with every mission, outfit, interior, world overhaul or other ASI. Read docs/VALIDATION.md before reporting a bug. No gun mode is included.
