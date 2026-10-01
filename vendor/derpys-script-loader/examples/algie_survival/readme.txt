derpy's script loader: algie_survival example

difficulty:
 this is an EXPERT example, comments are less verbose and so they can focus on more complex concepts

prerequisites:
 you should understand the basics of Lua and Bully scripting and about the difference between a "native" and "dsl" script
 a "native" script is one from the un-modded game (such as "main.lua" from Scripts/Scripts.img, which is always running)
 a "dsl" script is one from a script loaded by derpy's script loader

summary:
 this mod spawns a challenge blip by the tenements where you can go and fight waves of Algie

files:
 config.txt: the config for the script collection
 launcher.lua: responsible for spawning the blip and controlling the survival script
 survival.lua: the actual survival script
 spawns.lua: spawn points for algie

challenges:
 1. make a command to instantly start the survival instead of going to the blip
 2. make a way to punish the player for camping in corners or standing on top of things
