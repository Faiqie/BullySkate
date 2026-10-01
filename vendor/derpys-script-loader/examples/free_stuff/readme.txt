derpy's script loader: free_stuff example

difficulty:
 this is a BEGINNER example, so comments are extra verbose to help you learn

prerequisites:
 you should understand the basics of Lua and Bully scripting and about the difference between a "native" and "dsl" script
 a "native" script is one from the un-modded game (such as "main.lua" from Scripts/Scripts.img, which is always running)
 a "dsl" script is one from a script loaded by derpy's script loader

summary:
 this mod makes all items in stores free

files:
 config.txt: this file is in every "script collection" (a folder that you put in _derpy_script_loader/scripts), it determines what scripts run
 free.lua: our main script

challenges:
 1. change the item sold in stores (consider printing the arguments given to ShopAddItem to see what the script normally does)
 2. replace a function in another script (consider printing the names of each script and all the values in their environment)
