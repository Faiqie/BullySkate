# BullySkate

Skate through Bullworth as Jimmy using the Skate 3 Rust rewrite's simulation, animation and controller systems. Bully keeps its world, missions, pedestrians, traffic, audio and saves. This is an unofficial **Windows beta**.

**[Download the Windows ZIP](https://github.com/Faiqie/BullySkate/releases/download/v0.1.0-beta.3/BullySkate-Windows-v0.1.0-beta.3.zip)** · [Controls](docs/CONTROLS.md) · [Build from source](docs/BUILD.md) · [Troubleshooting](docs/TROUBLESHOOTING.md)

Players need only this ZIP: it includes the launcher, setup helpers, instructions and licenses. Extract it and run `BullySkateLauncher.exe`. Source code is a separate download using **Source code (zip)** or **Source code (tar.gz)** on the [release page](https://github.com/Faiqie/BullySkate/releases/tag/v0.1.0-beta.3), or clone this repository.

## Install and play

**Required Bully version: Bully Scholarship Edition for PC, version 1.200, with a compatible native engine layout.** Steam and other PC distributions are checked by their engine code and data locations, rather than one whole-file checksum. See [compatibility and diagnosis](docs/COMPATIBILITY.md) for the tested scope.

1. Download `BullySkate-Windows-v0.1.0-beta.3.zip` from Releases and extract it into a folder you can write to.
2. Close Bully, then open `BullySkateLauncher.exe`. The terminal opens a Windows file-selection window. Select **Bully.exe** in your installed **Bully Scholarship Edition 1.200** folder.
3. A second file-selection window asks for **default.xex** in your own complete, extracted Xbox 360 Skate 3 folder. Select the XEX; leave the surrounding `data` folder intact.
4. Wait while the launcher prepares the required data locally, verifies it, backs up the previous mod installation, and starts Bully. This preparation runs once; subsequent launches reuse your saved setup.
5. Load Story, finish the cutscene, and stand on foot on solid ground. Press **F6**, or **View/Back + D-pad Left**, to start skating. Initial simulation preparation may take about 15 seconds; if it says it is preparing, wait and toggle again.

You need **your own copies of both games**. The download and source repository contain **no game executables, levels, meshes, textures, animation banks, saves, or extracted physics databases**. Jimmy's rig, world collision, rails and car bounds are generated from the selected Bully files. Skate data is extracted from the selected local Skate 3 archives. The launcher does not download either game or execute Xbox code. **default.xex alone is insufficient.**

Requirements: Windows 10/11 **64-bit**, .NET Framework 4.8, an installed **Bully Scholarship Edition 1.200** with the verified engine layout, and the supported Xbox 360 Skate 3 asset edition. Different executable checksums are accepted when the native locations match. Older or genuinely different engine layouts require an official game update or a separate adapter profile. Stock Steam gameplay has not been tested on the release machine. Xbox/XInput controllers are supported; PlayStation controllers need an existing XInput mapping. Players do not need Python, Rust, or Visual Studio.

## Features

- Skate mode toggle with Jimmy's native 36-bone rig and original skateboard; leg IK preserves his proportions, and facial poses follow the head.
- Source skating, Flick-It input, tricks, manuals, grabs, bail/off-board behavior, state and animation graphs, and skating camera running in an isolated x64 Rust worker.
- Edit Skater: regular/goofy stance, truck tightness, wheel hardness, style, posture, and four bindings chosen from 37 gestures. No clothing editor.
- Skating FOV slider from **40–110°**, with automatic preference saving.
- Session marker and hold-to-respawn controls, including distance-dependent hold time and cancellation on release.
- Board swings against nearby pedestrians: a swept deck contact plus a native line-of-sight check applies one damage/reaction event.
- Skitch moving street cars by matching their speed, approaching the rear bumper, and holding RB. Bully still owns the car's AI; the adapter supplies bounded towing and a reaching pose.
- Collision and grind candidates separated by Bully area; nearby NPC and traffic contacts are mirrored into the skating solver.
- Render-thread input submission is asynchronous, with bounded queues, cached geometry and rigs, and throttled actor snapshots. The skating speed panel has been removed.
- Automatic Windows display-scaling correction for high-DPI screens, including 4K displays; native fullscreen and your chosen game resolution are preserved.
- Installation backups, saved setup, integrity checks, and repair.

See [all controls](docs/CONTROLS.md), [technical design](docs/ARCHITECTURE.md), and [validation and limits](docs/VALIDATION.md). The upstream rewrite is unfinished; this beta is **not verified as identical to retail Skate 3**. Skitching is implemented by this Bully adapter. Independent wheel spin, every mission/interior/outfit, and performance on other PCs are not fully verified. No gun mode is included.

## Update, repair, or remove

Close Bully and run the newer launcher. It verifies files before starting the game and repairs a damaged mod installation. Existing skater settings and custom configuration are preserved.

Run `Setup.cmd` to select different game files; `Verify.cmd` checks your saved installation without launching. `CheckBully.cmd` opens a Bully.exe picker and creates a compatibility report without installing anything or asking for Skate 3. Canceling a picker preserves the previous setup. Preferences live in the game's `_derpy_script_loader/scripts/BullyMotion/skater-settings.dat`. Setup paths and extraction caches are local to `%LocalAppData%/BullySkate`; none are sent to GitHub.

Press F5 in gameplay to restore native Bully controls. To disable the collection with Bully closed, run the included `Disable.cmd`; it moves the collection into a recoverable disabled folder. Installation backups live under `_derpy_script_loader/motion-backups`. The installer preserves original game archives and unrelated mods. Existing unrecognized `dinput8.dll` files are refused rather than overwritten.

## Source and credits

The repository includes the launcher, installer, Lua gameplay/menu scripts, x86 native adapter, x64 Rust worker, adapted headless runtime, local asset converters, required upstream Rust crates, and build tooling. Only public mod code and third-party software are included.

The skating rewrite is [SK8-ENGINE/skate-3-rust-engine](https://github.com/SK8-ENGINE/skate-3-rust-engine), pinned to commit `60efdef86600d8d8d4feb4b7c608fa0efd0643d7`; its redistribution was authorized by the project owner of this mod. The native loader comes from [NathSSH/derpys-script-loader](https://github.com/NathSSH/derpys-script-loader), by derpy54320, with SWEGTA UI credits. [Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader) is by ThirteenAG. Additional dependencies include Bevy, Rapier/Parry, Lua, PyFFI/NifTools, NumPy and PyInstaller. See [third-party notices](THIRD_PARTY_NOTICES.md) and `licenses/` for provenance and license texts; upstream code retains its original rights.

Bully and Skate 3 are owned by their respective rights holders. This project is not affiliated with Rockstar, EA, or their developers. Do not upload game data or saves with bug reports. Describe the area, controls, and symptoms, and redact personal paths from logs.
