# Controls

Controller names use the Xbox/XInput layout. View/Back is the small button to the left of Guide. Native Bully controls apply when skate mode is off.

| Action | Controller | Keyboard |
|---|---|---|
| Toggle skate mode | View/Back + D-pad Left | F6 |
| Return to native Bully | View/Back + D-pad Down | F5 |
| Edit Skater / skating FOV menu | View/Back + D-pad Up | F8 |
| Steer / move | Left stick | WASD |
| Push | A | Space |
| Brake | B | S |
| Flick-It tricks / ollies / flips / manuals | Right-stick gestures | Left Shift for a basic ollie; advanced stick gestures require a controller |
| Source grab input | LT / RT | Controller recommended |
| Get off / get back on the board | Y | E |
| Swing held board on foot | Press RB while facing a nearby pedestrian | R |
| Retrieve a dropped board | Source RB pickup action | R |
| Skitch | Hold RB close behind a moving street car, matching its speed | Hold R |
| Release skitch | Release RB, brake, dismount, or leave the ground | Release R / S / E |
| Shift along the rear while skitching | Left stick | A / D |
| Set session marker on supported ground | LB + D-pad Down | F7 |
| Respawn at session marker | Hold LB + D-pad Up | Hold F10 |
| Cancel pending respawn | Release D-pad Up | Release F10 |
| Perform assigned gesture | Hold its D-pad direction without View/Back or LB | Controller |
| Native pause | Start | Escape |
| Reload mod scripts / return to native controls | — | F9 (debug/repair) |

Source right-stick motions determine tricks; this adapter forwards the original controller input rather than assigning every trick to a separate button. LT/RT feed source grab input. Triggers, finger flips and source-specific combinations follow the supplied rewrite's recognizer; retail-complete recognition is not claimed.

Markers last for the game session and survive switching between Skate and Bully. They reset when the visible Bully area changes or the game session restarts. Set another marker after an area transition. Respawning requires a short hold that increases with distance; release cancels it. View/Back and LB reserve their D-pad combinations so they do not trigger gestures.

## Menu

| Action | Controller | Keyboard |
|---|---|---|
| Select row | D-pad Up/Down or left stick | Up/Down |
| Adjust selected value | D-pad Left/Right | Left/Right |
| Open submenu | A | Enter |
| Back | B | Backspace |
| Close menu | View/Back + D-pad Up | F8 |

Edit Skater changes **Regular/Goofy**, truck tightness, wheel hardness, style (**Default, Loose, Gonzo, Aggressive**), posture (**Default, Stiff, Slouch, Buff**) and each D-pad gesture. Style/posture appear as the next movement animation starts. Camera/FOV adjusts the skating FOV from **40° to 110°**; the default is 67°. Changes save automatically. Reset Skater preserves FOV, while Reset FOV changes only FOV. Bully's native camera returns when skate mode ends.

## Launcher

| Option | Behavior |
|---|---|
| Double-click EXE | File pickers on first setup; saved setup and launch thereafter |
| `--setup` / Setup.cmd | Open file pickers again and prepare fresh local data |
| `--check` / Verify.cmd | Verify saved setup and installed mod; do not launch |
| `--no-launch` | Install/repair without launching |
| `--fullscreen` | Use native display settings; the default |
| `--windowed` | Experimental borderless display; may cause a black scene |
| `--no-dpi-fix` | Use original Windows display scaling; troubleshooting only |
| `--game PATH --xex PATH` | Advanced/scripted setup without picker windows |
| `--state-dir PATH` | Use a separate local setup/cache directory |
| `--report FILE` | Save the launcher transcript locally; may include your paths |
| `--help` | Show options |
