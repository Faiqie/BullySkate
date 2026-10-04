function LoadScript(name) if name=="editor.lua" or name=="debug_camera.lua" then assert(loadfile("scripts/BullyMotion/"..name))() end end
assert(loadfile("scripts/BullyMotion/main.lua"))()
local timer=0
function GetTimer() return timer end
function Wait(ms) assert(ms==0,"Mode switch contains a fixed sleep");timer=timer+16 end
function FS_SkateFree() end
function FS_View() end
function SkateAudioStop() end
function PlayerSetControl() end
function PlayerDetachFromVehicle() end
function PedIsValid() return true end
function GetCutsceneRunning() return 0 end
function PedGetHealth() return 100 end
function PedSetActionNode() return true end
function PedIsPlaying() return true end
function PedSetActionTree() end
function PedSetEffectedByGravity() end
function PedSetAlpha() end
function PedFaceHeading() end
function PedGetHeading() return 0 end
function PlayerUnequip() end
function PedClearWeapon() end
function PlayerSetWeapon() end
function CameraReturnToPlayer() end
function CameraReset() end
function CameraDefaultFOV() end
function FS_SkateReady() return 2 end
function PlayerIsInAnyVehicle() return false end
function PlayerGetPosXYZ() return 0,0,1 end
function FS_Trace() return true,0,0,0 end
function PedGetWeapon() return -1 end
function PedGetAmmoCount() return 0 end
local boardAvailable=true
function FS_BoardReady() return boardAvailable and timer>=32 end
function AreaGetVisible() return 0 end
function FS_SkateNew() return {} end
function FS_SkateInteraction() return 500,4294967295,0,1 end
local _,edge,menuEdge=ShortcutEdges(130,0);assert(edge and not menuEdge)
_,edge=ShortcutEdges(130,130);assert(not edge)
_,edge=ShortcutEdges(130,2);assert(edge,"D-pad held before right stick must work")
_,edge,menuEdge=ShortcutEdges(132,128);assert(not edge and menuEdge)
_,edge,menuEdge=ShortcutEdges(36,0);assert(not edge and not menuEdge,"Old View shortcuts must not activate")
switch();assert(mode==1 and physicalBoard and timer<=200,"Fast mount failed")
switch();assert(mode==0 and not physicalBoard,"Toggle did not return to Bully")
boardAvailable=false;timer=0;switch();assert(mode==0 and not physicalBoard and timer<=650,"Failed equip left skating active")
print("PASS: right-stick chords work in either press order, do not repeat while held, and mode switching uses bounded readiness with cleanup on failure")
-- Board contact must accept the struck NPC's torso/collider and the held
-- board, but must reject intervening scenery. Exercise different body sizes.
local sequence,damages,blocked,hitPed,hitModel,hitX=0,0,false,0,0,0
function FS_SkateInteraction() return 500,4294967295,sequence,123 end
function PedGetPosXYZ() return 0,-1,1 end
local bodyHeight,bodyRadius=1.56,.28
function FS_PedBaseOffset() return 1,bodyHeight,bodyRadius end
function FS_Trace() return blocked,hitX,-1,bodyHeight*.6,0,0,1,hitPed,hitModel end
function PedApplyDamage(ped,amount) assert(ped==123 and amount==25);damages=damages+1 end
hitSequence=0
local sizes={{1.05,.18},{1.56,.28},{2.1,.48}}
for _,size in ipairs(sizes) do
 bodyHeight,bodyRadius=size[1],size[2];blocked=false
 sequence=sequence+1;updateInteraction(0,0,0,0)
end
assert(damages==3,"NPC dimensions prevented a clear strike")
blocked=true;hitX=0;hitModel=0;sequence=sequence+1;updateInteraction(0,0,0,0)
assert(damages==4,"Contact on the target collider was rejected")
hitX=1;hitModel=437;sequence=sequence+1;updateInteraction(0,0,0,0)
assert(damages==5,"Jimmy's own held board rejected the strike")
hitModel=6000;sequence=sequence+1;updateInteraction(0,0,0,0)
assert(damages==5,"A wall allowed damage through it")
updateInteraction(0,0,0,0);assert(damages==5,"A consumed contact was applied twice")
print("PASS: board damage accepts different NPC dimensions and actual target/held-board contact, rejects walls, and consumes each event once")
function FS_SkaterOptions() return 1,.7,.7,0,0,0,1,2,3,67 end
function FS_VideoOptions() return 0,0,0,0,100,0 end
function FS_PerformanceOptions(...) if arg.n>0 then return true end;return 0,2,2,1,1,2,1,1,1 end
local savedDifficulty,savedCamera=0,1
function FS_PhysicsOptions(...) if arg.n>0 then savedDifficulty,savedCamera=arg[1],arg[2];return true end;return savedDifficulty,savedCamera end
function CreateTexture(path) return path end
function FS_VideoStats() return true,1920,1080,60,false end
EditorLoad();editorPage="home";local rows=EditorRows();assert(rows[2][2]==10 and rows[2][3]=="fov")
editorPage="skater";for _,item in ipairs(EditorRows()) do assert(item[2]~=10,"FOV belongs in the skate menu") end
local tx,ty,th,tc,font=0,0,0,{},"Arial"
function SetTextFont(s) font=s end
function SetTextHeight(h) th=h end
function SetTextPosition(x,y) tx,ty=x,y end
function SetTextAlign() end
function SetTextColor(r,g,b,a) tc={r,g,b,a} end
function SetTextShadow() end
function DrawText(s) print(string.format("DRAW_TEXT %.6f %.6f %.6f %d %d %d %s|%s",tx,ty,th,tc[1],tc[2],tc[3],font,s)) end
function DrawRectangle(x,y,w,h,r,g,b,a) assert(x>=0 and y>=0 and x+w<=1 and y+h<=1);print(string.format("DRAW_RECT %.6f %.6f %.6f %.6f %d %d %d %d",x,y,w,h,r,g,b,a)) end
local iconDraws=0
function DrawTexture(path,x,y,w,h) iconDraws=iconDraws+1;print(string.format("DRAW_TEXTURE %s %.6f %.6f %.6f %.6f",path,x,y,w,h)) end
editorPage="home";row=2;EditorDraw();iconDraws=0;EditorShortcuts()
assert(iconDraws==6,"All three shortcut chords must be drawn")
performanceOptions[9]=0;EditorShortcuts();assert(iconDraws==6,"Control help Off must hide every shortcut")
performanceOptions[9]=1
row=3;EditorChange(1);assert(savedCamera==0 and savedDifficulty==0 and editorConfigure);EditorChange(1);assert(savedCamera==1)
row=4;for i=1,5 do EditorChange(1);assert(savedDifficulty==math.mod(i,5) and editorConfigure) end
assert(EditorRows()[4][4][5]=="Easy + Motorized")
print("PASS: FOV is on the root skate menu and controller icons replace the keyboard HUD hints")
editorPage="performance";row=10;EditorActivate();assert(performanceOptions[1]==1 and performanceOptions[2]==1 and performanceOptions[7]==2)
row=11;EditorActivate();assert(performanceOptions[1]==2 and performanceOptions[2]==0 and performanceOptions[4]==2)
row=12;EditorActivate();assert(performanceOptions[1]==0 and performanceOptions[2]==2 and performanceOptions[7]==1)
print("PASS: balanced and low CPU settings apply immediately and reset restores full defaults")
-- Free camera must not move Jimmy until confirmation, must use the skating
-- relocation when mounted, and must restore native control on cancellation.
local player={12,34,6};local moved,streamed,control,gravity=0,0,1,true
function PlayerGetPosXYZ() return unpack(player) end
function PedSetPosSimple(p,x,y,z) player={x,y,z};moved=moved+1 end
function PlayerSetControl(value) control=value end
function PedSetEffectedByGravity(p,value) gravity=value end
function AreaLoadCollision(x,y) assert(x==math.floor(x) and y==math.floor(y));streamed=streamed+1 end
local cameraArgs
function CameraSetXYZ(...) assert(arg.n==6);cameraArgs=arg end
function CameraSetFOV(value) assert(value>=40 and value<=110) end
function FS_Key() return false,false end
mode=0;menu=false;boardAvailable=true
DebugCameraToggle();assert(debugCamera and control==0 and not gravity)
debugCamera.yaw=0;debugCamera.pitch=0
local origin={debugCamera.x,debugCamera.y,debugCamera.z}
DebugCameraUpdate(0,.1,0,0,1,0,0,0,0,function()return false end)
assert(debugCamera.y<origin[2] and debugCamera.x==origin[1],"Forward must follow the actual view")
assert(cameraArgs[2]==debugCamera.y and cameraArgs[5]<cameraArgs[2],"Native render eye is first; target is ahead of movement")
origin={debugCamera.x,debugCamera.y,debugCamera.z}
DebugCameraUpdate(1,.1,0,1,0,0,0,0,0,function()return false end)
assert(debugCamera.x<origin[1] and debugCamera.y==origin[2],"Right strafe must follow screen right")
DebugCameraUpdate(2,.1,0,0,0,1,1,0,0,function()return false end)
assert(debugCamera.yaw<0 and debugCamera.pitch>0,"Right/up look must not be inverted")
-- The native RenderWare view looks down its at vector and flips camera X.
-- Exercise both stick signs with rotated/pitched views, not just world axes.
for _,yaw in ipairs({0,.7,-1.6,3.1}) do
 for _,pitch in ipairs({-.6,0,.6}) do
  for _,axis in ipairs({{0,1},{0,-1},{1,0},{-1,0}}) do
   debugCamera.yaw=yaw;debugCamera.pitch=pitch
   local bx,by,bz=debugCamera.x,debugCamera.y,debugCamera.z
   DebugCameraUpdate(3,.1,0,axis[1],axis[2],0,0,0,0,function()return false end)
   local fx,fy,fz=cameraArgs[4]-cameraArgs[1],cameraArgs[5]-cameraArgs[2],cameraArgs[6]-cameraArgs[3]
   local mx,my,mz=debugCamera.x-bx,debugCamera.y-by,debugCamera.z-bz
   if axis[2]~=0 then assert((mx*fx+my*fy+mz*fz)*axis[2]>0,"Forward/back movement disagrees with native view")
   else assert((mx*fy-my*fx)*axis[1]>0,"Left/right movement disagrees with RenderWare's projected X") end
  end
 end
end
debugCamera.streamAt=0;debugCamera.streamX=nil;streamed=0
local beforeX,beforeY=debugCamera.x,debugCamera.y
DebugCameraUpdate(1000,.1,0,1,1,0,0,0,1,function() return false end)
assert(moved==0 and streamed==1,"Flying must leave the player in place and stream the viewed area")
assert(debugCamera.x~=beforeX and debugCamera.y~=beforeY)
DebugCameraClose(false);assert(not debugCamera and moved==0 and control==1 and gravity)
DebugCameraToggle();debugCamera.x,debugCamera.y,debugCamera.z=100,-300,25
DebugCameraUpdate(1100,.1,8192,0,0,0,0,0,0,function(n)return n==8192 end)
assert(not debugCamera and moved==1 and player[1]==100 and player[2]==-300 and player[3]==25)
local teleported=false
mode=1;physicalBoard={}
function FS_SkateCamera() return true,100,-300,26,0,-1,0,67 end
function FS_SkateTeleport(handle,x,y,z,heading) assert(handle==physicalBoard and x==200 and y==-400 and z==30);assert(math.abs(math.sin(heading))<.0001 and math.cos(heading)<-.9999,"Teleport must face the current camera view");teleported=true;return true end
function FS_SkateRoot() return true,200,-400,30 end
DebugCameraToggle()
assert(debugCamera.x==100 and debugCamera.y==-300 and debugCamera.z==26 and math.abs(debugCamera.yaw)<.0001,"Free camera must inherit the riding eye and forward direction")
debugCamera.x,debugCamera.y,debugCamera.z=200,-400,30
assert(DebugCameraClose(true) and teleported and feet==30 and moved==2)
DebugCameraToggle()
function FS_SkateTeleport() return false end
debugCamera.x,debugCamera.y,debugCamera.z=200,-400,30
assert(not DebugCameraClose(true) and debugCamera and moved==2,"Failed worker teleport must keep the camera recoverable")
DebugCameraClose(false)
local held,a,b,c=ShortcutEdges(136,128);assert(held and c and not a and not b)
held,a,b,c=ShortcutEdges(136,8);assert(c,"Camera chord must work in either order")
held,a,b,c=ShortcutEdges(136,136);assert(not c,"Camera chord must not repeat while held")
print("PASS: free camera streams the viewed area without moving Jimmy; B confirms native/skating teleport, cancellation restores control, and failed teleport is recoverable")
