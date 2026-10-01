# Build from source (Windows x64)

Install Git, a recent Rust toolchain supporting edition 2024, Python 3.12, .NET Framework 4.8, and Visual Studio 2022 Build Tools with **Desktop development with C++** and a Windows SDK. `cargo`, `rustup`, and Python must be available on PATH. You do not need either game's data to compile the mod or package the launcher.

```powershell
git clone https://github.com/Faiqie/BullySkate.git
cd BullySkate
rustup target add i686-pc-windows-msvc x86_64-pc-windows-msvc
python -m pip install -r requirements-build.txt
powershell -NoProfile -ExecutionPolicy Bypass -File tools/BuildRelease.ps1
```

BuildRelease compiles the x86 Rust bridge library and patched native ASI, x64 worker, standalone local asset converter, and terminal launcher. It audits the source/payload and writes one player ZIP under `release/`, with SHA-256 checksums inside the ZIP. Source code remains a separate repository/source-archive download. Intermediate files live under `work/`. The checked-in authored `BullyMotion.cat` action bank uses references to Bully's existing animations; `BullyMotion.mact` and `tools/make_actions.py` are its source. It contains no animation samples. Its compiler provenance is recorded in THIRD_PARTY_NOTICES.

`tools/Build.cmd` locates Visual Studio with vswhere. If needed, set `BULLY_VCVARS` to your installation's `VC/Auxiliary/Build/vcvarsall.bat`. A custom target directory may be passed to Build.ps1 or BuildWorker.ps1. The launcher is compiled with the Windows Forms reference and an STA entry point for its standard file pickers.

The upstream rewrite's required Rust crates and the modified headless game modules are included. The loader's source, headers and upstream static libraries are included with its original notices and pinned provenance. Rust dependencies resolve from Cargo.lock; dependency license texts are under `licenses/`. The vendored BSD PyFFI parser provides offline NIF reading; PyInstaller packages it with NumPy, so users need no Python installation.

BuildRelease also runs `tools/TestLauncherDisplay.ps1`. It compiles a legacy x86 DPI probe and starts it through the production launch settings, checking actual Windows DPI awareness, inherited compatibility settings and the opt-out. This opens no visible game window and requires no game files. The launcher uses a per-monitor DPI manifest and the .NET Framework 4.8 Windows Forms DPI configuration.

## Checks

```powershell
python -m unittest discover -s tests -p "test_*.py"
cargo test --lib
cargo test --manifest-path skate-runtime/Cargo.toml --example moving_queries_probe
python tools/AuditDistribution.py
```

Game-dependent setup and worker probes require your own game files. Use launcher `--game ... --xex ... --no-launch --state-dir ...` for isolated setup; `--check` verifies the result. The converter can be run directly:

```powershell
python extractor/extract.py --bully "D:/Games/Bully Scholarship Edition" --out prepared/bully-assets
python extractor/extract.py --xex "D:/OwnedGames/Skate3/default.xex" --out prepared/skate-assets --manifest runtime/expected-skate-assets.sha256
```

Prepared game data is ignored by Git and must not be attached to a release or issue. One upstream collision-fixture test is gated behind `private-fixtures`; its original game sample is deliberately excluded. The copied headless runtime's full upstream test suite is not included, so run the dedicated examples rather than treating `cargo test --workspace` as a complete retail validation.
