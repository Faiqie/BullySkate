-- Production Lua 5.0.2 control flow with an authored game API fixture.
function LoadScript(path) assert(loadfile('scripts/BullyMotion/'..path))() end
assert(loadfile('scripts/BullyMotion/main.lua'))()
local clock,weapon,ammo,present,idle,controls=0,5,0,true,true,1
local equips,damage,free,context=0,0,0,0
gPlayer=1
function GetTimer() return clock end
function Wait(ms) assert(ms==0,'A mode switch must not wait for animation') end
function GetCutsceneRunning() return 0 end
function AreaGetVisible() return 0 end
function PedIsValid(ped) return ped==1 or ped==123 end
function PedGetHealth() return 100 end
function PlayerIsInAnyVehicle() return false end
function PlayerGetPosXYZ() return 0,0,1 end
function PedGetHeading() return 0 end
function FS_Trace() return true,0,0,0,0,0,1,123 end
function FS_View() end
function FS_SkateReady() return 2 end
function FS_SkateNew() return 99 end
function FS_SkateFree(host) assert(host==99);free=free+1 end
function FS_SkateInteraction() return 500,4294967295,1,123 end
function FS_BoardPresent() return present end
function PlayerSetWeapon(w,n) weapon=w;ammo=ammo+n;equips=equips+1 end
function PedGetWeapon() return weapon end
function PedGetAmmoCount() return ammo end
function PlayerSetControl(value) controls=value end
function PedSetEffectedByGravity() end
function PedSetActionNode() idle=true;return true end
function PedIsPlaying() return idle end
function PlayerDetachFromVehicle() end
function PedSetAlpha() end
function PedSetActionTree() end
function PedFaceHeading() end
function PlayerUnequip() weapon=-1 end
function PedClearWeapon() ammo=0 end
function CameraReturnToPlayer() end
function CameraReset() end
function CameraDefaultFOV() end
function PedGetPosXYZ() return 0,1,1 end
function FS_PedBaseOffset() return 1,1.7,0.3 end
function PedApplyDamage(ped,n) assert(ped==123 and n==25);damage=damage+1 end
function FS_Interact() context=context+1 end
local sayOriginal=say
function say() end
switch();assert(mode==1 and physicalBoard==99 and controls==1 and boardOwned)
assert(savedWeapon==5 and savedBoard==0)
local mounted=equips
recoverBoard(0);assert(equips==mounted)
weapon=-1;present=false;recoverBoard(100);assert(equips==mounted)
recoverBoard(200);assert(equips==mounted+1 and weapon==437)
present=true;idle=false;recoverBoard(400);assert(idle)
switch();assert(mode==0 and physicalBoard==nil and weapon==5 and ammo==0 and free==1)
assert(not modeChord(64,0) and not modeChord(128,0))
assert(modeChord(192,64) and not modeChord(192,192))
assert(not modeChord(64,192) and modeChord(192,64))
assert(skateButtons(192+4096,false)==4096 and skateButtons(64,false)==64 and skateButtons(4096,true)==0)
switch();interact();assert(mode==0 and context==1 and free==2)
hitSequence=0;updateInteraction(0,0,0,0);assert(damage==1)
updateInteraction(1,0,0,0);assert(damage==1)
local originalTrace=FS_Trace
function FS_Trace() return true,0,0,0,0,0,1,-1 end
hitSequence=0;updateInteraction(2,0,0,0);assert(damage==1,'Walls must block board damage')
FS_Trace=originalTrace
hitSequence=0;updateInteraction(3,10,0,0);assert(damage==1,'A stale distant contact must not hurt a ped')
skaterOptions[11]=0;editorPage='skater';row=6
function FS_SkaterOptions(...) return true end
EditorChange(1);assert(skaterOptions[11]==1);EditorChange(1);assert(skaterOptions[11]==0)
print('PASS instant switching, inventory restore, board recovery, L3+R3 edges, native context handoff, guarded board hits, Motorized menu')
