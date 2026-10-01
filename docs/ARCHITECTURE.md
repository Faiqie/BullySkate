# Architecture and data boundaries

The x86 ASI adapter runs inside Bully. Game-thread code reads native actors, traffic and input, and sends bounded values through shared memory and events to an x64 Rust worker. Game pointers never cross that boundary. The fixed shared structure is 5,724 bytes with compile-time size checks on both sides. Input submission is asynchronous and bounded to eight frames; rendering uses the last coherent simulation snapshot. The source solve stays at 60 Hz.

The worker runs the rewrite's controller conversion, state/motion graphs, animation evaluation, board/skeleton physics, trick recognition and camera. Jimmy's native 36-bone skin is retargeted from the source pose using locally extracted bind transforms and leg IK. The original two-bone skateboard follows the physical deck. Native matrices are overridden for world drawing and restored afterwards; D3D state is restored with a full state block.

Nearby living NPCs are kinematic capsules, limited to the closest 24 within 16 m and sampled every 100 ms. Up to eight supported street cars are mirrored every 50 ms using model bounds extracted from the user's COL archive. Snapshots expire if stale. Bully owns their AI and native reactions. A source board sweep produces hit requests, with native living/distance/line-of-sight checks before damage. The rewrite lacks a complete physical Skitching state; bounded towing and a native-proportion reaching pose are adapter behavior.

## Local preparation

Executable validation checks an embedded PC 1.200 profile: PE32/x86 image layout, 203 SHA-256 code fingerprints and 19 mapped data locations. The launcher and install-time `BullyBuildCheck.exe` share the same checker and profile. Whole-file SHA-256 is reported for diagnostics, rather than used as the acceptance gate. The native loader checks the loaded process before any game hook write. See [compatibility](COMPATIBILITY.md) for the tested scope and remaining limits.

The launcher sets `HIGHDPIAWARE` in Bully's child-process compatibility environment before creating the game process, replacing inherited DPI-unaware/GDI-scaling flags while preserving unrelated flags. It changes neither global Windows scaling nor the game's rendering resolution. The launcher itself declares PerMonitorV2 awareness, with a PerMonitor fallback, and enables .NET Framework 4.8 Windows Forms DPI handling for file pickers. See [Microsoft's process DPI guidance](https://learn.microsoft.com/en-us/windows/win32/hidpi/setting-the-default-dpi-awareness-for-a-process) and [Windows Forms DPI configuration](https://learn.microsoft.com/en-us/dotnet/desktop/winforms/high-dpi-support-in-windows-forms). `--no-dpi-fix` leaves the child's original compatibility environment intact.

The public launcher embeds mod code, software runtimes and checksum lists. It does not embed original or converted game assets. First setup reads the selected games without changing their archives:

- Bully `Stream/World.dir` / `World.img`: generates `world.bmgeo` (BMGEO2, per-area collision triangles), `world.bmrails` (BMRL2, area grind candidates), `jimmy-bind.json` (native bind transforms), and `vehicle-bounds.txt` (native street-car bounds).
- Skate 3 local BIG archives: extracts/converts the 18 required animation, physics, skeleton, graph, camera and controller files. Every required file is verified against the supported asset manifest.

Data is staged, checked and installed locally under the Bully script collection. Paths are saved atomically under `%LocalAppData%/BullySkate`. Setup cancellation or extraction failure leaves the previous saved selection intact. A public payload checker and Git ignore rules reject game-file paths and asset extensions. Build-time game collection projections and the earlier unused native weapon mesh were removed from this public build.

Each area loads its own collision, grind and grab providers. LOD placements are skipped, mirrored transforms correct winding, and invalid/missing areas produce a recoverable mounting error. Markers reset across visible-area changes.
