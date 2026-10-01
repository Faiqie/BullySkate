# derpy's script loader
A script loader for Bully: Scholarship Edition.
You can see more information [here](http://bullyscripting.net/dsl/about.html).

# contribution
If you want to make your own example scripts, feel free to go ahead and make a pull request.
If you find any issues with the loader (or server) code, please open an issue or just make a pull request if it's a quick fix.

# setup environment (windows)
Clone this repository to a folder named `_derpy_script_loader` inside your `Bully Scholarship Edition` folder.
Also put the latest dinput8.dll from [Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader) into your `Bully Scholarship Edition` folder.

# compile (windows)
Simply run `compile.cmd` for full versions (with system access) or `compile_s.cmd` for S versions (without system access).
Both require you to have visual studio command line tools. Both CMD scripts will detect whether or not they are available, and tell you if they are not.

# compile (unix)
First make sure `gcc` is installed, then run `compile.sh` or `compile_s.sh` (which you may need to chmod first).
The server version is 32 bit so that the sizes of its values match the client, so you may also need to install `lib32z1` and `libc6-i386` to run it.

# archive
The `archive.cmd` script creates a few different ZIP files for release using 7-Zip.
Run it (on windows) after compiling the windows client, windows server, and unix server.

# credits
derpy's script loader uses [libpng](http://www.libpng.org/pub/png/libpng.html), [libzip](https://libzip.org/), and [Lua 5.0.2](https://www.lua.org).
It also uses many compression libraries including[bzip2](https://sourceware.org/bzip2/), [lzma](https://7-zip.org/sdk.html),
[zlib](https://www.zlib.net/), and [zstd](https://facebook.github.io/zstd/).