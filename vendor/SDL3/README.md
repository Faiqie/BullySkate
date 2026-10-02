# SDL3 3.4.16

Unmodified headers and x86 import library from [SDL's official Visual C++ release](https://github.com/libsdl-org/SDL/releases/download/release-3.4.16/SDL3-devel-3.4.16-VC.zip).

Archive SHA-256: `1a784cb2a5c64d56fe7a62090fe9d242d9865f235e4ea9678f1a6ba4e693e7de`.
The matching x86 DLL is `runtime/controller-dependencies/SDL3.dll`, SHA-256 `47d508b4232cf9462096bdb6f613a330d9a94a7eb09c305fad073430c6531920`.
License: `licenses/SDL3-Zlib.txt`. Upstream source: https://github.com/libsdl-org/SDL/tree/release-3.4.16.

The mod dynamically loads its private DLL on the game thread and initializes only gamepad input. The import library is used by the hardware-free virtual-controller tests.
