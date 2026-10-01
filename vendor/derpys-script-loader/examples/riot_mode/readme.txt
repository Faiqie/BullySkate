derpy's script loader: riot_mode example

difficulty:
 this is an INTERMEDIATE example, so it is expected you'll understand the basics without extra comments

prerequisites:
 you should understand the basics of Lua and understand what a "ped" and "faction" is in Bully
 a "ped" refers to an instance of a character (this includes the player and npcs)
 a ped's "faction" or "type" refers to their clique (represented by a number)

summary:
 this mod makes peds hate other factions and start fighting each other
 it also makes more peds spawn everywhere

files:
 config.txt: this file is in every "script collection" (a folder that you put in _derpy_script_loader/scripts), it determines what scripts run
 riot.lua: our main script where all our riot code is

challenges:
 1. make everyone fight the player instead of just fighting the nearest ped
 2. give every ped fire crackers if they aren't already holding some using PedGetWeapon and PedSetWeapon (hint: the id fire crackers is 301 and you should give 5 ammo)
