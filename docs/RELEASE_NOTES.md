# v0.1.0-beta.3

**Required Bully version: Bully Scholarship Edition for PC, version 1.200, with a compatible native engine layout.** Steam and other distribution executables are checked by their engine locations instead of one whole-file checksum. You also need your own complete, extracted Xbox 360 Skate 3 files.

**Players: [Download the Windows ZIP](https://github.com/Faiqie/BullySkate/releases/download/v0.1.0-beta.3/BullySkate-Windows-v0.1.0-beta.3.zip).** Extract it, open `BullySkateLauncher.exe`, and select your own game files when asked. This is the only player download needed; setup helpers, instructions and licenses are included.

**Source code is separate:** use GitHub's **Source code (zip)** or **Source code (tar.gz)** downloads below, or browse/clone [the repository](https://github.com/Faiqie/BullySkate). The player ZIP does not include source code. SHA-256 checksums are inside the player ZIP.

This update removes the exact-file checksum requirement that could reject another PC distribution of the same Bully engine. The launcher and installer share a check of 203 required code locations and 19 mapped data locations. Header/resource/overlay differences can pass; different engine layouts still require their own adapter profile. The ASI also checks the running process before installing any game hooks.

New `CheckBully.cmd` opens a file picker and saves a compatibility report without installing or requiring Skate 3 files. The report contains no personal paths or game code and is never uploaded automatically. See [compatibility and diagnosis](https://github.com/Faiqie/BullySkate/blob/main/docs/COMPATIBILITY.md).

- Windows file-selection windows for Bully.exe and default.xex, with a terminal for progress and saved setup thereafter.
- Display-scaling correction before the game starts, with an optional `--no-dpi-fix` troubleshooting switch.
- First-run local extraction of Bully collision, area rails, Jimmy bind rig and car bounds, plus verified Skate animation/physics/input/camera data. No game data is distributed.
- Native Jimmy and skateboard rig retargeting, source skating and controller input, Edit Skater and 40–110° skating FOV.
- Session markers, board strikes and adapter-assisted skitching, area collision separation and asynchronous simulation submission.
- Native display path, preference-preserving install/repair, recoverable backups and disable helper.
- Separate portable source download, pinned upstream provenance and build workflow; player ZIP with setup guide, controls, troubleshooting and checksums.

Executable compatibility tests cover checksum-only variants, changed native code, malformed files, architecture/base mismatches and non-writable globals. The actual native ASI loads in an unrelated x86 test host without installing game hooks. Windows display-scaling tests continue to pass. Stock Steam gameplay, every retail executable, the reported 4K PC and a physical multi-monitor/scaling matrix remain unverified. Matching 1.200 layouts are eligible; this release does not claim universal compatibility.

The upstream rewrite is incomplete. This is not a claim of retail Skate 3 equivalence, guaranteed FPS, independently spinning wheels, or compatibility with every mission, outfit, interior, world overhaul or other ASI. Read docs/VALIDATION.md before reporting a bug. No gun mode is included.
