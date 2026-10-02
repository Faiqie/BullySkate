"""Author a small action tree that uses Bully's existing Jimmy and board clips.

No game animation data is copied. Compile with marcdred's MACT_TO_CAT.py.
"""
from pathlib import Path

STATES = {
    "Idle": ("SK8_IDLE_REG", "Idle_SK8Board", True),
    "Coast": ("SK8_COAST_REG", "Idle_SK8Board", True),
    "TurnLeft": ("SK8_TURNL_REG", "Idle_SK8Board", True),
    "TurnRight": ("SK8_TURNR_REG", "Idle_SK8Board", True),
    "Push": ("SK8_PUMP_REG", "Idle_SK8Board", False),
    "Brake": ("SK8_BREAK_REG", "Break_Reg_SK8Board", True),
    "Ollie": ("Sk8_Ollie_Reg", "Ollie_Reg_SK8Board", False),
    "Crouch": ("SK8_CROUCH_FWD_REG", "Idle_SK8Board", True),
}


def animation(kind: str, clip: str, loop: bool) -> str:
    return f'''\t\t\t{kind}
\t\t\t{{
\t\t\t\tparam00008 true
\t\t\t\tparam00012 0.000000
\t\t\t\tparam00016 -1.000000
\t\t\t\tparam00024 h"SKATEBOARD\\{clip}"
\t\t\t\tparam00032 {2 if loop else 0}
\t\t\t\tparam00036 0
\t\t\t\tparam00040 0.000000
\t\t\t\tparam00044 -1.000000
\t\t\t\tparam00048 1.000000
\t\t\t\tparam00052 0.120000
\t\t\t\tparam00056 -1.000000
\t\t\t}}
'''


text = "Bank BullyMotion\n{\n"
for name, (ped, board, loop) in STATES.items():
    text += f"\tNode {name}\n\t{{\n\t\tConditionGroup\n\t\t{{\n\t\t}}\n\t\tTracks\n\t\t{{\n"
    text += '''\t\t\tPropSetSocket
\t\t\t{
\t\t\t\tparam00008 false
\t\t\t\tparam00012 0.000000
\t\t\t\tparam00024 h"DUMMY"
\t\t\t\tparam00032 false
\t\t\t}
'''
    text += '''\t\t\tSetWeaponFlags
\t\t\t{
\t\t\t\tparam00008 false
\t\t\t\tparam00012 0.000000
\t\t\t\tparam00024 true
\t\t\t\tparam00028 0
\t\t\t}
'''
    text += animation("Animation", ped, loop) + animation("WeaponAnimation", board, loop)
    text += "\t\t}\n\t}\n"
text += "}\n"
exit_node='''\tNode Exit
\t{
\t\tConditionGroup
\t\t{
\t\t}
\t\tTracks
\t\t{
\t\t\tSetWeaponFlags
\t\t\t{
\t\t\t\tparam00008 false
\t\t\t\tparam00012 0.000000
\t\t\t\tparam00024 false
\t\t\t\tparam00028 0
\t\t\t}
\t\t\tPropSetSocket
\t\t\t{
\t\t\t\tparam00008 false
\t\t\t\tparam00012 0.000000
\t\t\t\tparam00024 h"LeftHand"
\t\t\t\tparam00028 h"LocoHold"
\t\t\t\tparam00032 false
\t\t\t}
\t\t}
\t}
'''
text=text[:-2]+exit_node+'}\n'
destination = Path(__file__).resolve().parents[1] / "scripts/BullyMotion/BullyMotion.mact"
destination.write_text(text, encoding="utf-8")
print(destination)
