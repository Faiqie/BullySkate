# Bully executable compatibility

BullySkate targets **Bully Scholarship Edition for PC, version 1.200**. Distribution labels and whole-file hashes do not establish engine compatibility. The launcher accepts any executable that passes the embedded `bully-pc-1.200` profile: PE32/x86 at the expected image base, 203 native code fingerprints and 19 mapped data locations. Different timestamps, resources and appended distribution metadata can therefore pass without matching the original test executable's checksum.

Steam and other distribution variants with this layout are eligible. This release has been tested with the owner's local 1.200 executable, private copies with changed metadata, and synthetic executable fixtures. **A stock Steam installation and every other retail build have not been tested in gameplay.** Older 1.154, relocated or packed engine code, and changes at required native locations are not supported by this profile. Supporting a different engine layout requires verified addresses and runtime changes, not simply disabling the checks.

The separate native ASI checks the loaded process before installing its first game hook. If its minimum runtime layout check fails, it skips the hooks. This also protects against manually loading the mod into an incompatible executable outside the launcher.

## Check your copy

1. Extract the latest player ZIP and open `CheckBully.cmd`.
2. Select your own Bully.exe in the Windows file picker. No Skate 3 files or mod installation are needed for this check.
3. Read the terminal result. The saved report is `%LocalAppData%/BullySkate/compatibility-report.txt`.
4. If rejected, include that report when reporting the problem. It includes the executable's SHA-256, PE metadata, matched-location counts and first mismatch; it contains no personal paths, executable bytes or game assets. Nothing is uploaded automatically.

For Steam, verify the game's installed files before checking again. An older retail installation needs the official 1.200 update. Keep your existing game executable; this project does not distribute a replacement or modify its DRM.

The public [SilentPatchBully source](https://github.com/CookiePLMonster/SilentPatchBully/blob/master/SilentPatchBully/SilentPatchBully.cpp) also identifies the 1.200 engine with its version marker. Its "SP Build" label is a SilentPatch revision, not an additional Bully version requirement.

The checked-in compatibility XML contains interoperability hashes and location metadata only. The production profile is embedded into the compiled launcher/checker; it cannot be overridden by a loose XML file. Source builds and compatibility tests require no game files.
