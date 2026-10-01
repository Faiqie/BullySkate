derpy's script loader: custom_healthbar example

difficulty:
 this is an INTERMEDIATE example, so it is expected you'll understand the basics without extra comments

prerequisites:
 you should understand the basics of Lua and Bully scripting and understand normalized screen coordinates
 all of DSL's drawing functions use normalized screen coordinates, basically meaning (0, 0) is the top left of the screen and (1, 1) is the bottom right

summary:
 this mod draws a custom healthbar using a heart texture
 the health only shows up when it changes or when you hit left/right

files:
 config.txt: the config for the script collection
 heart.lua: the main script
 heart.png: heart texture

challenges:
 1. make the healthbar flash rapidly when you are at critical health
 2. move the healthbar down when you have allies (since their healthbars will be where the heart is)
