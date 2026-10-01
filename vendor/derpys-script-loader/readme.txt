derpy's script loader: version 9

-- installation --

install:
 put "derpy_script_loader.asi" and "_derpy_script_loader" in the game's root directory
 if you do not already have an asi loader then put "dinput8.dll" in the game's root directory too
 if you have issues updating you may need to delete "_derpy_script_loader/scripts/loadact.lua" as it has been deprecated

scripts:
 put scripts into "_derpy_script_loader/scripts" by following the instructions for whatever mod you are installing
 see the included "install.png" for help as some mods are single files and some are entire folders or even zips
 "loadanim.lua" is built-in so other mods can rely on it, and server_browser so you can get started quickly

-- information --

about:
 this is a script loader for Bully: Scholarship Edition that can load any amount of scripts
 it provides scripters with a lot more power and ease of use than they had modifying built-in scripts

config:
 a default config file will be generated when derpy's script loader starts if one does not exist or the current one is outdated
 open up _derpy_script_loader/config.txt after it generates to see extra loader options

console:
 press ~ (or ` in some regions) to toggle the console
 use /help for a list of commands

networking:
 there are networking features that are disabled by default (enable it in the config)
 use /connect to connect to a server *or* hit F1 to use the server browser

-- miscellaneous --

resources:
 join the discord to get updates, ask questions, find online games, or just hang out: https://discord.gg/r6abc7Avpm
 as a scripter, you can use the light documentation.html or visit the full documentation: http://bullyscripting.net

packages:
 scripts can make use of custom "packages" in "_derpy_script_loader/packages" (non-existent by default) using the "require" function
 if you want to develop one that loads a DLL using the "loadlib" function, see "extras/sdk/dslpackage.h"

credits:
 derpy54320 - developer
 SWEGTA - ui design