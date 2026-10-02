# Third-party provenance and notices

The repository and launcher contain no Bully or Skate 3 game files or locally
converted game assets. All original-game inputs remain on the player's machine.
Names and format identifiers in code describe interoperability.

| Component | Origin / pinned revision | Location / terms |
|---|---|---|
| Skate 3 Rust rewrite | https://github.com/SK8-ENGINE/skate-3-rust-engine @ `60efdef86600d8d8d4feb4b7c608fa0efd0643d7` | Required crates under `vendor/skate-rewrite`; adapted game modules under `skate-runtime`; extractor archive/VLT/skeleton readers under `extractor/tools`. No root project license was supplied. The mod owner stated permission to redistribute this rewrite. Original authors retain rights; the mod's MIT license does not apply to this upstream code. |
| derpy's script loader 9 | https://github.com/NathSSH/derpys-script-loader @ `09bfc7be7605b3854ea3b3a5fd7acf04ba8e081c` | `vendor/derpys-script-loader`, modified loader sources in `tools/loader`, linked ASI. Developer derpy54320; UI SWEGTA. Retains upstream notices; no root project license supplied. |
| Ultimate ASI Loader | https://github.com/ThirteenAG/Ultimate-ASI-Loader | Bundled x86 dinput8.dll; MIT text in `licenses/Ultimate-ASI-Loader-MIT.txt`. |
| SDL3 | https://github.com/libsdl-org/SDL @ `release-3.4.16` | Unmodified headers/import library in `vendor/SDL3`, private x86 gamepad DLL in `runtime/controller-dependencies`. Zlib license in `licenses/SDL3-Zlib.txt`. |
| PyFFI / NifTools | https://github.com/niftools/pyffi | BSD parser, version 2.2.4.dev3, under `vendor/python/pyffi`. `licenses/PyFFI-BSD.txt`. Only parser code and format schemas are bundled. |
| MACT compiler (build provenance) | https://github.com/marcdred/bully-mact-tool @ `fb4922ec9fc0bac1cab8af119e37c9600328ee3c` | GPL-3.0 upstream tool used to compile the authored BullyMotion.mact bank. The distributed bank contains authored action nodes and references, with no copied animations. Compiler source/license are under `tools/mact`; it is not linked or bundled into the launcher. |
| Rust dependencies | Versions locked by Cargo.lock | Bevy, Rapier/Parry, serde, glTF and dependencies. SPDX declarations and repository links in `licenses/rust-dependencies.json`; upstream license texts in `licenses/rust/`. |
| Python / NumPy / PyInstaller and packaging helpers | Versions in requirements-build.txt; CPython 3.12 | License texts in `licenses/python` and `licenses/Python.txt`. PyInstaller's bootloader distribution exception applies to packaged applications; upstream texts are retained. |
| Lua 5.0.2, libpng, libzip and compression libraries | Loader's original source/headers/static libraries | MIT/BSD/libpng/zlib/BSD compression terms remain upstream; notices in `licenses/` and loader headers. Original loader documentation credits bzip2, lzma, zlib and zstd. |
| Microsoft Visual C++ runtimes | App-local x86/x64 VCRUNTIME140 and VC80 OpenMP | Microsoft redistributable software; copyright Microsoft Corporation. Original binaries retained. These are software dependencies, not game assets. |

Original BullySkate additions are covered only as stated in LICENSE. This notice
does not grant rights to the games or replace any third-party author's terms.
