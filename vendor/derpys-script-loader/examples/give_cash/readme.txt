derpy's script loader: give_cash example

difficulty:
 this is a STARTER example, so it is extra simple to show you the basics

prerequisites:
 you should understand the basics of Lua

summary:
 this mod lets you get money by using a newly created /give_cash command

files:
 config.txt: this file is in every "script collection" (a folder that you put in _derpy_script_loader/scripts), it determines what scripts run
 hello.lua: our main script where all our code is

challenges:
 1. give yourself fire crackers instead of cash using GiveAmmoToPlayer(301, 5)
 2. instead of defining the function seperately, put the entire function inside the call to SetCommand (hint: don't include the "give_player_cash = " part)
