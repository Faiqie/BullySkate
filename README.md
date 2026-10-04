# BullySkate

Skate around Bullworth as Jimmy with Skate 3 physics and controller support.

**[Download BullySkate](https://github.com/Faiqie/BullySkate/releases/download/v0.1.2/BullySkate-Windows-v0.1.2.zip)**

## What you need

- **Bully Scholarship Edition for Windows, version 1.200.** Steam users should select their installed game and keep Steam signed in.
- Your own complete, extracted **Xbox 360 Skate 3** files. Keep `default.xex` and its surrounding `data` folder together.
- Windows 10 or 11, 64-bit. Xbox/XInput, DualShock 4 and DualSense controllers are supported.

Neither game is included.

## Setup

1. Download and extract the ZIP.
2. Open **BullySkateLauncher.exe**.
3. Use the file pickers to select **Bully.exe**, then Skate 3's **default.xex**.
4. Wait for setup to finish. The launcher installs the mod and starts Bully.
5. Load your save, stand on foot, and press **Right stick click + D-pad Down** (or F6) to skate.

After setup, open the same launcher to play. Your file paths and skater settings are saved.

For PlayStation controllers, connect by USB or pair through Windows Bluetooth. No controller mapper is needed for skating. **R3 + D-pad Down** switches Skate / Bully; **R3 + D-pad Left** opens the Skate menu.

## Features

- Jimmy and his skateboard animated for skating.
- Tricks, manuals, grabs, bails and controller Flick-It controls.
- Edit Skater: stance, trucks, wheels, gestures, style and posture.
- Easy, Normal, Hardcore, Motorized and Easy + Motorized difficulties.
- High and Low skating cameras, FOV slider, session markers and respawn.
- Board strikes against pedestrians and skitching behind cars.
- Skate 3 board sounds matched to Bully's surface materials.
- Connected grind edges throughout the map.
- F8 > Video: fullscreen, borderless, VSync, filtering and frame limit.
- Live Performance settings with Balanced and Low CPU presets.
- Free camera with teleport, and on-screen control help you can hide.
- Keeps current DSL installations and other script mods.
- Display-scaling correction, performance improvements and installation backups.

## Main controls

| Action | Keyboard | Controller |
|---|---|---|
| Skate / Bully | F6 | Right stick click + D-pad Down |
| Native Bully mode | F5 | Right stick click + D-pad Down while skating |
| Skate menu / FOV | F8 | Right stick click + D-pad Left |
| Debug camera / cancel | F11 | Right stick click + D-pad Right |
| Bully pause menu | Escape | Start / Options |
| Set marker | F7 | LB / L1 + D-pad Down |
| Respawn | Hold F10 | Hold LB / L1 + D-pad Up |

[All controls](docs/CONTROLS.md) · [Troubleshooting](docs/TROUBLESHOOTING.md)

Close Bully before updating. Open the newer launcher to install the update. Use `Setup.cmd` to choose different files, or `Disable.cmd` to disable the mod.

## Credits

Built using [SK8-ENGINE's Skate 3 Rust rewrite](https://github.com/SK8-ENGINE/skate-3-rust-engine), [Derpy's Script Loader](https://www.nexusmods.com/bullyscholarshipedition/mods/43) and [Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader). Skate audio port by [Hailey Ross](https://github.com/Hailey-Ross/rusty-trucks), with [Andrew Nakas's research](https://github.com/andrewnakas/skate3-audio). [Build instructions](docs/BUILD.md) · [Licenses and notices](THIRD_PARTY_NOTICES.md).
