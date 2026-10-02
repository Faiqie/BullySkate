-- Jimmy and his native skateboard, driven by the supplied Skate simulation.
mode,menu,row=0,false,1
physicalBoard=nil
yaw,speed,feet=0,0,0
oldButtons,oldKeys=0,{}
padKind=1
function PadText(xbox,playstation) return padKind>=2 and playstation or xbox end
savedWeapon,savedBoard=nil,0
boardOwned=false
useFullSkate,usePhysicalBoard=true,true
mountUntil,mountEvents,boardAnimation,animationUntil=0,0,"Idle",0
hint,hintEnd="F6 Skate / F8 Edit Skater / F5 Bully",0
pushUntil,ollieUntil,physicsNextLog=0,0,0
actorUpdate,actorHistory=0,{}
trafficUpdate,boardKeyUntil,hitSequence,towVehicle=0,0,0,4294967295
markerFlags,markerSets,markerReturns,markerProgress=0,0,0,0
markerModifier,markerKeyUntil=false,0
markerResetPending,markerResetAt=false,0
function clamp(x,a,b) return math.max(a,math.min(b,x)) end
function bit(b,n) return math.mod(math.floor(b/n),2)==1 end
function say(s) hint=s;hintEnd=GetTimer()+5000;print(s) end
function key(vk)
 local down,pressed=FS_Key(vk);local edge=pressed or (down and not oldKeys[vk]);oldKeys[vk]=down;return down,edge
end
LoadScript("editor.lua")
function restore(fast)
 mountUntil=0
 if physicalBoard then FS_SkateFree(physicalBoard);physicalBoard=nil end
 FS_View()
 actorHistory={};actorUpdate=0
 trafficUpdate=0;boardKeyUntil=0;towVehicle=4294967295
 if mode~=0 or menu then PlayerSetControl(1) end
 if mode~=0 then
  if boardOwned then
   PlayerDetachFromVehicle()
   if PedIsValid(gPlayer) and GetCutsceneRunning()==0 and PedGetHealth(gPlayer)>0 then
    PedSetActionNode(gPlayer,"/Global/BullyMotion/Exit","Act/BullyMotion.act")
    -- Let the native board stop its rolling cue before its bank can unload.
    if not fast then Wait(200) end
   end
  end
  PedSetEffectedByGravity(gPlayer,true);PlayerSetControl(1)
  PedSetAlpha(gPlayer,255,false)
  PedSetActionTree(gPlayer,"/Global/Player","Act/Player.act")
  PedSetActionNode(gPlayer,"/Global/Player/Default_KEY","Act/Player.act")
  PedFaceHeading(gPlayer,PedGetHeading(gPlayer)*180/math.pi,0)
  PlayerUnequip()
  if savedBoard==0 then PedClearWeapon(gPlayer,437) end
  if savedWeapon and savedWeapon>=0 then PlayerSetWeapon(savedWeapon,0) else PlayerUnequip() end
  CameraReturnToPlayer();CameraReset();CameraDefaultFOV()
 end
 mode=0;menu=false;speed=0;vz=0;state=nil;boardOwned=false;boardAnimation="Idle";animationUntil=0
 savedWeapon=nil;savedBoard=0
end
function switch()
 local target=1
 if target==mode then target=0 end
 restore()
 if target==0 then say("Bully controls restored");return end
 if target==1 and useFullSkate then
  local status,failure=FS_SkateReady()
  if status~=2 then say(status==3 and ("Skate startup failed: "..failure) or "Preparing Skate animation and physics; try again shortly");return end
 end
 if GetCutsceneRunning()~=0 or PlayerIsInAnyVehicle() then say("Switch modes on foot after the scene finishes");return end
 local x,y,z=PlayerGetPosXYZ();local ok,hx,hy,hz=FS_Trace(x,y,z+0.5,x,y,z-3)
 if not ok then say("No ground found; mode switch cancelled");return end
 yaw=PedGetHeading(gPlayer);pitch=0;feet=hz;vz=0;grounded=true
 savedWeapon=PedGetWeapon(gPlayer);savedBoard=PedGetAmmoCount(gPlayer,437)
 print("Inventory captured "..tostring(savedWeapon).." board "..savedBoard)
 mode=target;PlayerSetControl(0);PedSetEffectedByGravity(gPlayer,false)
 if mode==1 then
  PlayerSetWeapon(437,savedBoard>0 and 0 or 1)
  Wait(500)
  local ok=PedSetActionNode(gPlayer,"/Global/Vehicles/SkateBoard/Locomotion/BoardInHand/GetOn","Act/Vehicles.act")
  print("Native board GetOn: "..tostring(ok))
  if not ok then restore();say("Board mount failed; Bully controls restored");return end
  Wait(1500)
  PedSetEffectedByGravity(gPlayer,false)
  local coast="/Global/BullyMotion/Idle"
  print("Coast idle "..tostring(PedSetActionNode(gPlayer,coast,"Act/BullyMotion.act")))
  Wait(500)
  print("Coast idle active "..tostring(PedIsPlaying(gPlayer,coast,true)))
  boardOwned=true
  if usePhysicalBoard then physicalBoard=FS_SkateNew(x,y,feet,yaw,AreaGetVisible()) end
  if not physicalBoard then restore();say("Skate mount failed; Bully controls restored");return end
  local physical,tow,hits=FS_SkateInteraction();hitSequence=hits
  say(PadText("A / Space push | Y / E off board | RB / R swing board or hold behind a car","Cross: push | Triangle: off board | R1: swing board / skitch"))
 end
end
function text(s,x,y,h,r,g,b)
 SetTextFont("Georgia");SetTextHeight(h or 0.022);SetTextPosition(x,y);SetTextAlign("LEFT","TOP")
 SetTextColor(r or 235,g or 225,b or 197,255);SetTextShadow();DrawText(s)
end
function updateActors(x,y,now)
 if now<actorUpdate then return end
 actorUpdate=now+100
 local records,history,count,candidates={}, {},0,{};local ped
 for ped in AllPeds() do
  if ped~=gPlayer and PedIsValid(ped) then
   local px,py,pz=PedGetPosXYZ(ped)
   local distance=(px-x)^2+(py-y)^2
   if distance<16^2 and math.abs(pz-feet)<4 and PedGetHealth(ped)>0 then
    table.insert(candidates,{ped,px,py,pz,distance})
   end
  end
 end
 table.sort(candidates,function(a,b) return a[5]<b[5] end)
 local c
 for c=1,math.min(24,table.getn(candidates)) do
    local entry=candidates[c];local ped,px,py,pz=entry[1],entry[2],entry[3],entry[4]
    local old=actorHistory[ped];local base=old and old[5] or FS_PedBaseOffset(ped)
    pz=pz-base;local vx,vy,vz=0,0,0
    if old then local elapsed=math.max(0.01,(now-old[4])/1000);vx,vy,vz=(px-old[1])/elapsed,(py-old[2])/elapsed,(pz-old[3])/elapsed end
    history[ped]={px,py,pz,now,base};local values={ped,px,py,pz,vx,vy,vz};local k
    for k=1,7 do records[count*7+k]=values[k] end;count=count+1
 end
 actorHistory=history
 FS_SkateActors(physicalBoard,records,count)
end
function updateInteraction(now,x,y,z)
 local physical,tow,hits,ped=FS_SkateInteraction()
 if hits~=hitSequence then
  hitSequence=hits
  if PedIsValid(ped) and PedGetHealth(ped)>0 then
   local px,py,pz=PedGetPosXYZ(ped)
   if (px-x)^2+(py-y)^2<3^2 and math.abs(pz-z)<3 then
    local blocked,hx,hy,hz,nx,ny,nz,hitPed=FS_Trace(x,y,z+0.8,px,py,pz)
    if not blocked or hitPed==ped then
     PedApplyDamage(ped,25)
     if PedGetHealth(ped)>0 then PedSetActionNode(ped,"/Global/HitTree/Standing/Melee/Generic/Straight/HEADHEAVY3/Front/Front","Act/HitTree.act") end
    end
   end
  end
 end
 if tow~=towVehicle then
  if tow~=4294967295 then say(PadText("Skitching: release RB / R or brake to let go","Skitching: release R1 / R or brake to let go")) end
  towVehicle=tow
 end
end
function drawing()
 while true do
  if FS_Active() and GetCutsceneRunning()==0 then
   if mode==1 and markerModifier and not menu then
    text(PadText("LB + Down: set marker | Hold LB + Up: respawn","L1 + Down: set marker | Hold L1 + Up: respawn"),0.04,0.095,0.017)
    if markerProgress>0 then DrawRectangle(0.04,0.126,0.23*markerProgress,0.005,203,190,128,235) end
   end
   if GetTimer()<hintEnd then text(hint,0.04,0.06,0.019) end
   if menu then
    EditorDraw()
   end
  end
  Wait(0)
 end
end
function MissionSetup()
 while not SystemIsReady() do Wait(0) end
 if not FS_Available() then error("Unsupported Bully executable; native adapter disabled") end
 EditorLoad();FS_SkateReady();FS_RigDump()
 LoadAnimationGroup("Skateboard");LoadActionTree("Act/Vehicles.act");LoadActionTree("Act/BullyMotion.act");LoadActionTree("Act/HitTree.act")
 RegisterLocalEventHandler("ControllerUpdating",function(controller)
  if (mode==1 or menu) and FS_Active() then FS_MuteGameplay() end
 end)
 hintEnd=GetTimer()+10000;CreateDrawingThread(drawing)
 print("Bully Skate ready; F6 skating / F8 Edit Skater / F5 native Bully")
end
function MissionCleanup() restore(true) end
function main()
 local last=GetTimer();local lastArea=AreaGetVisible()
 while true do
  local now=GetTimer();local dt=clamp((now-last)/1000,0,0.1);last=now
  local connected,buttons,lx,ly,rx,ry,lt,rt,kind=FS_Pad()
  if kind>0 then padKind=kind end
  local function pressed(n) return bit(buttons,n) and not bit(oldButtons,n) end
  local back=bit(buttons,32)
  local active=FS_Active()
  if not active then editorResumeAt=now+200 end
  markerModifier=bit(buttons,256)
  local _,normalKey=key(116);local _,skateKey=key(117);local _,menuKey=key(119);local _,reloadKey=key(120)
  if FS_Active() then
   if normalKey or (back and pressed(2)) then restore();say("Bully controls restored")
   elseif skateKey or (back and pressed(4)) then switch()
   elseif menuKey or (back and pressed(1)) then EditorToggle()
   elseif reloadKey then restore();StartScript("main.lua");TerminateCurrentScript() end
  end
  local area=AreaGetVisible()
  if lastArea~=area then markerResetPending=true;markerResetAt=0;markerFlags=0;markerProgress=0;markerKeyUntil=0 end
  if mode==1 and (GetCutsceneRunning()~=0 or not PedIsValid(gPlayer) or PedGetHealth(gPlayer)<=0 or lastArea~=area) then restore();say("Bully controls restored for game event") end
  lastArea=area
  if markerResetPending and now>=markerResetAt then
   markerResetAt=now+250
   local status=FS_SkateReady()
   if status==3 or (status==2 and FS_SkateMarkerReset()) then markerResetPending=false end
  end
  EditorSync(now)
  if menu and FS_Active() then
   EditorInput(now,buttons,lx,ly,pressed,back)
  elseif mode==1 and physicalBoard and FS_Active() and dt>0 then
   local x,y,z=PlayerGetPosXYZ()
   if math.abs(z-feet-0.98)>4 then restore();say("Bully controls restored after teleport")
   else
    updateActors(x,y,now)
    if now>=trafficUpdate then trafficUpdate=now+50;FS_SkateVehicles(physicalBoard,x,y,z) end
    local forward,right=ly,lx
    local kw,ew=key(87);local ks,es=key(83);local kd,ed=key(68);local ka,ea=key(65)
    if IsKeyPressed("W") or kw or ew then forward=forward+1 end;if IsKeyPressed("S") or ks or es then forward=forward-1 end
    if IsKeyPressed("D") or kd or ed then right=right+1 end;if IsKeyPressed("A") or ka or ea then right=right-1 end
    forward,right=clamp(forward,-1,1),clamp(right,-1,1)
    local spaceDown,spaceEdge=key(32);local shiftDown,shiftEdge=key(160)
    if spaceEdge then pushUntil=now+80 end
    local push=bit(buttons,4096) or IsKeyPressed("SPACE") or spaceDown or now<pushUntil
    local brake=bit(buttons,8192) or IsKeyPressed("S") or ks or es
    local mapped=back and 0 or buttons
    local _,boardEdge=key(69);local grabDown=key(82)
    if boardEdge then boardKeyUntil=now+80 end
    if now<boardKeyUntil and not bit(mapped,32768) then mapped=mapped+32768 end
    if grabDown and not bit(mapped,512) then mapped=mapped+512 end
    local _,setMarker=key(118);local returnMarker=key(121)
    if setMarker then markerKeyUntil=now+60 end
    if now<markerKeyUntil or returnMarker then
     if not bit(mapped,256) then mapped=mapped+256 end
     local direction=now<markerKeyUntil and 2 or 1
     if not bit(mapped,direction) then mapped=mapped+direction end
    end
    if push and not bit(mapped,4096) then mapped=mapped+4096 end
    if brake and not bit(mapped,8192) then mapped=mapped+8192 end
    if shiftEdge then ollieUntil=now+220 end
    local flick=ry
    if now<ollieUntil-80 then flick=-1 elseif now<ollieUntil then flick=1 end
    if now<editorResumeAt then mapped,right,forward,rx,flick,lt,rt=0,0,0,0,0,0,0 end
    local valid,bx,by,bz,heading,bankPitch,bankRoll,bvx,bvy,bvz,truckTilt,contacts,ticks,clearance,groundContacts=FS_SkateStep(physicalBoard,dt,mapped,right,forward,rx,flick,lt,rt)
    if not valid then restore();say("Skate stopped after invalid solver output")
    else
     feet=bz-(clearance or 0.087);yaw=heading;speed=math.sqrt(bvx*bvx+bvy*bvy)
     local hasRoot,rootX,rootY,rootZ=FS_SkateRoot(physicalBoard)
     if hasRoot then x,y,feet=rootX,rootY,rootZ else x,y=bx,by end
     PedSetPosSimple(gPlayer,x,y,feet);PedFaceHeading(gPlayer,yaw*180/math.pi,0)
     updateInteraction(now,x,y,feet)
     FS_Bank(yaw,bankPitch,bankRoll)
     local hasCamera,cx,cy,cz,fx,fy,fz,lens=FS_SkateCamera(physicalBoard)
     if hasCamera then CameraSetXYZ(cx,cy,cz,cx+fx*10,cy+fy*10,cz+fz*10);CameraSetFOV(lens);FS_View(cx,cy,cz,fx,fy,fz,lens) end
     local flags,sets,returns,progress=FS_SkateMarkerStatus()
     if sets~=markerSets then say("Session marker set") end
     if returns~=markerReturns then say("Returned to session marker");actorHistory={};actorUpdate=0 end
     markerFlags,markerSets,markerReturns,markerProgress=flags,sets,returns,progress
     if now>physicsNextLog then
      physicsNextLog=now+10000
      local average,maximum,calls,rays=FS_BoardPerf();local rig,rigFrames=FS_RigStatus()
      print(string.format("Skate %.3f ms avg %.3f ms peak, %d frames; xyz %.2f %.2f %.2f contacts %d; rig %s frames %d",average,maximum,calls,bx,by,bz,contacts,tostring(rig),rigFrames))
     end
    end
   end
  end
  oldButtons=buttons;Wait(0)
 end
end
