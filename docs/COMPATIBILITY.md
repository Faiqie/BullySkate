# Bully executable compatibility

**Required engine: Bully Scholarship Edition for PC, version 1.200.** The production profile checks PE32/x86 at the expected image base, 203 native code fingerprints and 19 mapped data locations. Different file timestamps, resources and appended distribution metadata can pass without sharing one whole-file checksum.

Some Steam executables wrap engine code on disk. The launcher recognizes an executable entry point inside an executable `.bind` section, validates the mapped native data, and allows installation with the full code check deferred until startup. It also confirms the selected folder against Steam's Bully app manifest. It then starts that installation through Steam's normal `steam://rungameid/12200` request. Players select Bully.exe and their complete Skate 3 folder in the launcher's file pickers; no separate installer is required.

The native ASI verifies **all 203 code fingerprints and 19 data mappings in the loaded engine before installing its first hook**. A wrapped-file candidate is not a declaration that the loaded engine is supported. If the loaded check fails, the ASI skips its hooks and records matched counts and the first mismatch in `_derpy_script_loader/logs/skate-compatibility.log`. The check runs once at startup, not every frame. The bundled [Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader/blob/master/source/dllmain.cpp) defers plugin loading from `.bind` callbacks and calls `InitializeASI` after loading the plugin. BullySkate exposes that entry point and supports the older loader startup path too.

The launcher does not replace Bully.exe, unpack its executable, change DRM, or download a game. Older 1.154, relocated engines and changes at required native locations need an official update or a separately verified adapter profile. Loose compatibility XML cannot override the production profile.

## Tested scope

The owner's local 1.200 executable passes both file and full loaded-engine verification. Private metadata variants, malformed/incompatible fixtures, synthetic Steam-wrapper detection, and an authored native process whose file code differs from its loaded code have been tested. The actual ASI also skips hooks in an unrelated x86 process. Steam app-manifest routing, metacharacter paths and one-use Steam DPI settings have been checked locally. **A stock Steam installation has not been launched on the release machine; Steam authentication, third-party ASI combinations and gameplay on other retail builds still need player confirmation.**

SilentPatch may remain installed. Other ASIs can modify code locations after startup, so a running report can differ from the ASI's earlier startup result. The public [SilentPatchBully source](https://github.com/CookiePLMonster/SilentPatchBully/blob/master/SilentPatchBully/SilentPatchBully.cpp) also uses a version marker for the 1.200 engine. Its SP Build number describes SilentPatch, not a separate Bully engine requirement.

## Optional diagnostics

Normal setup and play use `BullySkateLauncher.exe` only. These checks help diagnose a failing copy:

1. `CheckBully.cmd` opens the launcher in file-check mode and asks for Bully.exe. It saves `%LocalAppData%/BullySkate/compatibility-report.txt`. A Steam-wrapped file can show mismatched file code while still being eligible for the startup check.
2. If needed, launch the selected copy normally through Steam, reach its menu, and leave it running. Open `CheckBullyRunning.cmd`, choose the same Bully.exe, and read `%LocalAppData%/BullySkate/running-compatibility-report.txt`. This uses query/read access only; it does not patch or stop the process. Close the game before using the launcher to install again.
3. Share the report and, if present, the `skate-compatibility.log` above. Reports contain hashes, section names, address/match counts and SilentPatch detection; no personal paths, executable bytes or game assets. Nothing is uploaded automatically. Redact personal paths from any other logs.

Steam users can verify their installed game files through Steam. Choose the real installed folder shown by **Manage > Browse local files**, rather than a copied or backup executable. An older retail installation needs the official 1.200 update. Keep your own executable.
