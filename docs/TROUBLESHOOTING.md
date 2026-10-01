# Troubleshooting

**Enlarged or cropped game/HUD on a 4K or high-DPI display:** use the updated launcher. It automatically corrects Windows display scaling before Bully starts. You can keep your preferred Windows Scale setting and choose the game's rendering resolution in Bully's graphics settings. If clipping remains, report the monitor resolution, Windows Scale percentage, fullscreen/windowed mode and graphics wrappers in use. For a wrapper that needs the original Windows scaling behavior, try `BullySkateLauncher.exe --no-dpi-fix`.

**Black scene with HUD/audio:** use the normal launcher / native fullscreen settings. Forced borderless mode caused this on the original test PC. Avoid `--windowed`. Close the game, then try `BullySkateLauncher.exe --fullscreen`. Other graphics wrappers or conflicting ASIs can also affect rendering; test against a clean compatible installation and retain your backups.

**Unsupported Bully executable, including Steam:** use beta.4 or newer. Beta.4 recognizes Steam-wrapped files and verifies the entire required engine layout in memory before hooking it. Select the real installed Bully.exe shown by Steam > Manage > Browse local files; the launcher handles installation and starts Steam automatically. If skating does not load, check `_derpy_script_loader/logs/skate-compatibility.log`. Optional `CheckBully.cmd` and `CheckBullyRunning.cmd` create file/loaded-engine reports through the launcher. A loaded-engine mismatch needs investigation or a separate profile; older retail versions need the official 1.200 update. See [compatibility and diagnostic steps](COMPATIBILITY.md).

**Missing Skate files:** selecting default.xex requires the full extracted Xbox 360 folder, including `data/big/miscload.big`, `miscboot.big`, and `db.big`, with their original directory structure. It cannot extract an ISO or console package directly. Unsupported or altered source assets are reported by filename; no game data is downloaded.

**Setup canceled:** the prior setup is kept, and the game is not started. Run Setup.cmd to choose again. The launcher uses standard Windows file-selection windows, which may be behind another window; use Alt+Tab if needed.

**Close Bully before installing:** save your progress and exit first. The launcher refuses to replace files that the running game has loaded.

**Write permission denied:** the launcher needs to write the mod into the selected Bully folder. Use a writable game installation folder or change that folder's permissions. Do not disable Windows security globally.

**Skate physics is preparing:** load your save, wait about 15 seconds, then try F6 again. Preparation time depends on CPU/storage. On failure, inspect `_derpy_script_loader/logs/skate-worker.log` and `skate-panic.log`; redact personal paths when sharing logs.

**Bad performance:** downtown contains more geometry, NPCs and traffic. This build reduces adapter work and separates area collision, but total game FPS depends on Bully, other mods and your GPU/CPU. Compare the same spot using F5 and F6. Report your hardware, native and skating FPS, and nearby landmark; no fixed FPS target is promised.

**Walls/transition problems:** area collision is prepared from your own world archive. Test a clean compatible game folder if a world overhaul changes geometry. This beta has not been played through every mission or transition. F5 restores native movement; markers reset on visible-area changes.

**Controller:** this release reads XInput. Connect the pad before launching; avoid running two controller-mapping layers simultaneously. PlayStation pads need an XInput mapping. The keyboard has a basic ollie, while full Flick-It input needs a controller.

**Swing/skitch:** swings need actual deck contact, a living nearby NPC and clear native line of sight. Retrieve the board if it dropped. For skitching, match the car's speed and stay within arm's reach behind the rear bumper while holding RB. Static, distant, removed or unsupported vehicles cannot tow the skater.

**Restore native Bully:** press F5 during gameplay. To disable the collection, close Bully and run Disable.cmd. Previous installations are backed up in `_derpy_script_loader/motion-backups`. Re-running the launcher re-enables/repairs the mod. Original game archives and saves are not modified by installation.
