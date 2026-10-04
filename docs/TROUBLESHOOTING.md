# Setup help

**Game version:** use Bully Scholarship Edition for Windows, version 1.200. For Steam, choose Bully.exe from **Manage > Browse local files**. Keep Steam signed in. Older retail copies need the official 1.200 update.

**Skate 3 files:** `default.xex` alone is not enough. Keep the complete extracted Xbox 360 folder, including its `data` folder. The launcher cannot open an ISO directly.

**Changing file paths:** close Bully and open `Setup.cmd` to pick your files again. Canceling preserves your previous setup.

**Skating is preparing:** load your save and wait about 15 seconds, then press F6 again.

**Black scene or cropped screen:** choose your rendering resolution in Bully's Video menu. Use F8 > Video for fullscreen or borderless mode. Mode changes apply on the next launch. If a saved mode fails, start the launcher with `--fullscreen` to recover native fullscreen.

**Other DSL mods:** the launcher uses official DSL 15.3 and keeps newer installations. Leave other mods in their usual folders with their `config.ini` files. Existing settings and script folders are preserved.

**Sound-card / DirectX error:** the launcher checks Bully's XACT audio engine and offers the verified Microsoft legacy DirectX installer when that runtime is missing. Accept Windows' administrator prompt. An enabled speaker or headphone output is also required.

**Skate sounds:** first setup prepares player audio from your own Skate 3 files. Bully keeps its pedestrians, music and ambience. If sound preparation fails, keep the full Skate 3 folder with `data/audio` beside its XEX.

**Controller:** connect an Xbox/XInput controller, DualShock 4 or DualSense by USB, or pair it through Windows Bluetooth. PlayStation controllers work directly in the skating mod. If another program hides your PlayStation controller, turn that program off for direct input, or use its Xbox mapping. Native Bully controls apply outside skate mode.

**Skitching:** match the car's speed, approach its rear bumper and hold RB / R1. Board strikes also use RB / R1; they need the board to contact a nearby pedestrian.

**Updating or disabling:** close Bully before running a newer launcher. Use F5 for native movement, or `Disable.cmd` to disable the mod. Reopening the launcher re-enables it.

**Still having trouble:** [report the problem](https://github.com/Faiqie/BullySkate/issues) with your game version, PC specs and what happened. `CheckBully.cmd` creates a compatibility report. If skating does not load, include `_derpy_script_loader/logs/skate-compatibility.log` if present. Remove personal paths from other logs before sharing.
