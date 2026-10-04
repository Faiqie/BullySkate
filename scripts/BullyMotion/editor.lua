-- Controller and keyboard editor. The native bridge saves only changed values.
skaterOptions={1,0.7,0.7,0,0,0,1,2,3,67}
editorPage,editorRepeats,editorConfigure,editorNextConfigure="home",{},true,0
editorResumeAt=0
gestureNames={"Air guitar","Airplane","Boxing","Bruce Lee","Check time","Devil horns","Double finger guns","Dunno","Finger wag","Fists","Bicep flex","Flip table","Fonz","Freedom","Fist salute","Get away","Get outta here","Handcuffs","High pump","Low pump","Prewind","Peace","Point","Raise the roof","Shaka","Shrug","Point at sky","Snap","Soul arch","Spock","Surf's up","Swing high","Swing low","Throw arms","Thumbs down","Wings","Yard sale"}
function EditorLoad()
 skaterOptions={FS_SkaterOptions()};editorConfigure=true
 videoOptions={FS_VideoOptions()}
 performanceOptions={FS_PerformanceOptions()}
 physicsOptions={FS_PhysicsOptions()}
 menuIcons={CreateTexture("ui/right-stick.png"),CreateTexture("ui/dpad-left.png"),CreateTexture("ui/dpad-down.png"),CreateTexture("ui/dpad-right.png")}
end
function EditorSync(now)
 if editorConfigure and now>=editorNextConfigure then
  editorNextConfigure=now+250
  local status=FS_SkateReady()
  if status==2 and FS_SkateConfigure() then editorConfigure=false end
 end
end
function EditorSave()
 if FS_SkaterOptions(unpack(skaterOptions)) then editorConfigure=true;editorNextConfigure=0
 else say("Could not save skater settings") end
end
function EditorClose()
 menu=false;editorRepeats={};editorResumeAt=GetTimer()+200
 if mode==0 then PlayerSetControl(1) end
end
function EditorToggle()
 if menu then EditorClose();return end
 if GetCutsceneRunning()~=0 then say("Open the skate menu after the scene finishes");return end
 menu=true;editorPage="home";row=1;editorRepeats={}
 if mode==0 then PlayerSetControl(0) end
end
function EditorRows()
 if editorPage=="skater" then return {
  {"Stance",1,"enum",{"Regular","Goofy"},"Choose your natural leading foot."},
  {"Trucks",2,"percent",nil,"Loose trucks turn more; tight trucks feel steadier."},
  {"Wheels",3,"percent",nil,"Soft to hard: adjusts the source wheel slide response."},
  {"Style",4,"enum",{"Default","Loose","Gonzo","Aggressive"},"Changes the source movement animation style."},
  {"Posture",5,"enum",{"Default","Stiff","Slouch","Buff"},"Posture blends in when the next movement starts."},
  {"Gestures",nil,"gestures",nil,"Assign an animation to each D-pad direction."},
  {"Reset skater",nil,"reset",nil,"Restore stance, equipment, style, posture and gestures."},
  {"Back",nil,"home"}}
 elseif editorPage=="gestures" then return {
  {"D-pad Up",6,"enum",gestureNames,"Hold D-pad Up while skating to perform this gesture."},
  {"D-pad Down",7,"enum",gestureNames,"Hold D-pad Down while skating to perform this gesture."},
  {"D-pad Left",8,"enum",gestureNames,"Hold D-pad Left while skating to perform this gesture."},
  {"D-pad Right",9,"enum",gestureNames,"Hold D-pad Right while skating to perform this gesture."},
  {"Back",nil,"skater"}}
 elseif editorPage=="performance" then return {
  {"Texture detail",1,"enum",{"Full","Medium","Low"},"Lower detail uses smaller texture mip levels.","performance"},
  {"Nearby NPC collisions",2,"enum",{"8","16","24"},"Nearest pedestrians are kept first. Native NPCs stay in the game.","performance"},
  {"NPC collision range",3,"enum",{"8 m","12 m","16 m"},"Smaller range reduces skating collision work.","performance"},
  {"NPC updates",4,"enum",{"20 Hz","10 Hz","7 Hz"},"Lower rates reduce CPU work; movement is predicted between updates.","performance"},
  {"Nearby car collisions",5,"enum",{"4","8"},"Nearest traffic is kept first. Native cars stay in the game.","performance"},
  {"Car collision range",6,"enum",{"15 m","25 m","35 m"},"Smaller range reduces traffic collision work.","performance"},
  {"Car updates",7,"enum",{"30 Hz","20 Hz","10 Hz"},"Lower rates reduce CPU work; nearby cars still support skitching.","performance"},
  {"Skating sounds",8,"enum",{"Off","On"},"Off stops the extra skating audio processing.","performance"},
  {"Control help",9,"enum",{"Off","On"},"Show or hide on-screen shortcuts and camera controls.","performance"},
  {"Balanced preset",nil,"balanced",nil,"Medium texture detail and fewer distant collision objects."},
  {"Low CPU preset",nil,"lowcpu",nil,"Reduces texture detail and nearby collision processing."},
  {"Restore defaults",nil,"resetperformance",nil,"Restore full texture detail and original collision budgets."},
  {"Back",nil,"home"}}
 elseif editorPage=="video" then return {
  {"Display mode",1,"enum",{"Fullscreen","Borderless","Windowed"},"Applies next time you launch Bully.","video"},
  {"VSync",2,"enum",{"Game default","Off","On"},"Applies next launch. Synchronizes to your display.","video"},
  {"Texture filtering",3,"enum",{"Game default","Nearest","Linear","2x AF","4x AF","8x AF","16x AF"},"Anisotropic filtering is limited to your GPU's support.","video"},
  {"Frame limit",4,"enum",{"Game default","30","60","90","120"},"Caps frames; Bully or SilentPatch can set a lower limit.","video"},
  {"Menu text size",5,"scale",nil,"Changes text size in this settings menu.","video"},
  {"FPS counter",6,"enum",{"Off","On"},"Show the measured frame rate.","video"},
  {"Reset video",nil,"resetvideo",nil,"Restore native display and filtering settings."},
  {"Back",nil,"home"}}
 elseif editorPage=="controls" then return {{"Back",nil,"home"}}
 end
 return {{"Edit skater",nil,"skater"},{"Camera FOV",10,"fov",nil,"Adjust the skating camera's field of view."},
  {"Camera height",2,"enum",{"Low","High"},"Choose the original Skate 3 Low or High camera. Applies while skating.","physics"},
  {"Difficulty",1,"enum",{"Easy","Normal","Hardcore","Motorized","Easy + Motorized"},"Easy + Motorized keeps Easy tricks and ollies with a powered board.","physics"},{"Video",nil,"video"},
  {"Performance",nil,"performance"},{"Controls",nil,"controls"},{"Control help",9,"enum",{"Off","On"},"Show or hide on-screen shortcuts and camera controls.","performance"},{"Debug camera",nil,"debugcamera",nil,"Fly around; B / Circle teleports you to the camera."},{mode==1 and "Return to Bully" or "Skate mode",nil,"mode"},{"Resume",nil,"close"}}
end
function EditorChange(direction)
 local item=EditorRows()[row];local index=item[2]
 if not index then return end
 local options=item[6]=="video" and videoOptions or item[6]=="performance" and performanceOptions or item[6]=="physics" and physicsOptions or skaterOptions
 local old=options[index];local value=old
 if item[3]=="enum" then value=math.mod(old+direction+table.getn(item[4]),table.getn(item[4]))
 elseif item[3]=="percent" then value=clamp(math.floor(old*20+0.5)+direction,0,20)/20
 elseif item[3]=="fov" then value=clamp(old+direction,40,110)
 elseif item[3]=="scale" then value=clamp(old+direction*5,80,130) end
 if value~=old then
  options[index]=value
  if item[6]=="video" then if not FS_VideoOptions(unpack(videoOptions)) then say("Could not save video settings") end
  elseif item[6]=="performance" then EditorPerformanceSave()
  elseif item[6]=="physics" then
   if FS_PhysicsOptions(unpack(physicsOptions)) then editorConfigure=true;editorNextConfigure=0
   else options[index]=old;say("Could not save skating settings") end
  else EditorSave() end
  if item[6]==nil and index==10 and mode==1 then CameraSetFOV(value) end
 end
end
function EditorPerformanceSave()
 if FS_PerformanceOptions(unpack(performanceOptions)) then
  actorHistory={};actorUpdate=0;trafficUpdate=0
  if performanceOptions[8]==0 then SkateAudioStop() end
 else say("Could not save performance settings") end
end
function EditorActivate()
 local item=EditorRows()[row];local action=item[3]
 if item[2] then EditorChange(1)
 elseif action=="debugcamera" then EditorClose();DebugCameraToggle()
 elseif action=="mode" then
  EditorClose();if mode==1 then restore();say("Bully controls restored") else switch() end
 elseif action=="close" then EditorClose()
 elseif action=="reset" then
  local defaults={1,0.7,0.7,0,0,0,1,2,3};local k
  for k=1,9 do skaterOptions[k]=defaults[k] end;EditorSave();say("Skater settings reset")
 elseif action=="resetvideo" then videoOptions={0,0,0,0,100,0};FS_VideoOptions(unpack(videoOptions));say("Video defaults saved; display mode applies next launch")
 elseif action=="balanced" then performanceOptions={1,1,1,1,0,1,2,1,1};EditorPerformanceSave();say("Balanced settings applied")
 elseif action=="lowcpu" then performanceOptions={2,0,0,2,0,0,2,1,1};EditorPerformanceSave();say("Low CPU settings applied")
 elseif action=="resetperformance" then performanceOptions={0,2,2,1,1,2,1,1,1};EditorPerformanceSave();say("Performance defaults restored")
 elseif action=="resetfov" then
  skaterOptions[10]=67;EditorSave();if mode==1 then CameraSetFOV(67) end
 else editorPage=action;row=1;editorRepeats={} end
end
function EditorBack()
 if editorPage=="home" then EditorClose()
 elseif editorPage=="gestures" then editorPage="skater";row=6;editorRepeats={}
 else editorPage="home";row=1;editorRepeats={} end
end
function EditorPulse(name,held,edge,now)
 if not held then editorRepeats[name]=nil;return edge end
 local next=editorRepeats[name]
 if edge or not next then editorRepeats[name]=now+350;return true end
 if now>=next then editorRepeats[name]=now+110;return true end
 return false
end
function EditorInput(now,buttons,lx,ly,pressed,back)
 if back then return end -- Reserve right-stick click chords for shortcuts.
 local upHeld,up=key(38);local downHeld,down=key(40)
 local leftHeld,left=key(37);local rightHeld,right=key(39)
 local _,confirm=key(13);local _,cancel=key(8)
 up=EditorPulse("up",upHeld or bit(buttons,1) or ly>0.55,up or pressed(1),now)
 down=EditorPulse("down",downHeld or bit(buttons,2) or ly< -0.55,down or pressed(2),now)
 left=EditorPulse("left",leftHeld or bit(buttons,4) or lx< -0.55,left or pressed(4),now)
 right=EditorPulse("right",rightHeld or bit(buttons,8) or lx>0.55,right or pressed(8),now)
 if cancel or pressed(8192) then EditorBack();return end
 if up then row=row-1 elseif down then row=row+1 end
 local count=table.getn(EditorRows());if row<1 then row=count elseif row>count then row=1 end
 if left then EditorChange(-1) elseif right then EditorChange(1) end
 if confirm or pressed(4096) then EditorActivate() end
end
function EditorChord(x,y,direction,height,label)
 local ok,w,h=FS_VideoStats();local width=height*(w>0 and h/w or 0.5625)
 DrawTexture(menuIcons[1],x,y,width,height)
 text("+",x+width+0.004,y+height*0.18,height*0.55)
 DrawTexture(menuIcons[direction],x+width+0.018,y,width,height)
 text(label,x+width*2+0.030,y+height*0.15,height*0.52)
end
function EditorShortcuts()
 if not menuIcons or (performanceOptions and performanceOptions[9]==0) then return end
 EditorChord(0.17,0.035,2,0.034,"To Open Menu")
 EditorChord(0.17,0.078,3,0.034,mode==1 and "To Bully" or "To Skate")
 EditorChord(0.17,0.121,4,0.034,debugCamera and "To Exit Camera" or "To Debug Camera")
end
function EditorDraw()
 local rows=EditorRows();local count=table.getn(rows);local scale=videoOptions[5]/100
 local function label(s,x,y,size,r,g,b,background,title)
  SetTextFont(title and "Arial Black" or "Arial");SetTextHeight(size*scale);SetTextPosition(x,y);SetTextAlign("LEFT","TOP")
  SetTextColor(r,g,b,255);SetTextShadow(background[1],background[2],background[3]);DrawText(s)
 end
 local paper={220,211,185};local navy={27,43,67};local gold={226,188,71}
 DrawRectangle(0.247,0.148,0.52,0.726,0,0,0,140)
 DrawRectangle(0.24,0.14,0.52,0.726,paper[1],paper[2],paper[3],246)
 DrawRectangle(0.24,0.14,0.52,0.093,navy[1],navy[2],navy[3],255)
 DrawRectangle(0.24,0.233,0.52,0.004,gold[1],gold[2],gold[3],255)
 local title=editorPage=="home" and "SKATE MENU" or editorPage=="skater" and "EDIT SKATER" or editorPage=="gestures" and "GESTURES" or editorPage=="video" and "VIDEO" or editorPage=="performance" and "PERFORMANCE" or "CONTROLS"
 label(title,0.262,0.166,0.034,gold[1],gold[2],gold[3],navy,true)
 local first=math.max(1,row-6);local last=math.min(count,first+6);local k
 for k=first,last do
  local item=rows[k];local y=0.264+(k-first)*0.056;local selected=k==row
  local bg=selected and navy or paper;local ink=selected and {245,237,214} or {32,38,46}
  if selected then
   DrawRectangle(0.252,y-0.006,0.496,0.050,navy[1],navy[2],navy[3],255)
   DrawRectangle(0.252,y-0.006,0.004,0.050,gold[1],gold[2],gold[3],255)
  end
  label(item[1],0.268,y,0.019,ink[1],ink[2],ink[3],bg,false)
  local index=item[2]
  if index then
   local value=(item[6]=="video" and videoOptions or item[6]=="performance" and performanceOptions or item[6]=="physics" and physicsOptions or skaterOptions)[index];local display
   if item[3]=="enum" then display=item[4][value+1]
   elseif item[3]=="percent" or item[3]=="scale" then display=string.format("%.0f%%",value*100/(item[3]=="scale" and 100 or 1))
   else display=string.format("%.0f deg",value) end
   label("< "..display.." >",0.494,y,0.017,ink[1],ink[2],ink[3],bg,false)
   if item[3]=="percent" or item[3]=="fov" then
    local scalar=item[3]=="percent" and value or (value-40)/70
    DrawRectangle(0.494,y+0.027,0.216,0.003,116,111,99,255)
    if scalar>0 then DrawRectangle(0.494,y+0.027,0.216*scalar,0.003,gold[1],gold[2],gold[3],255) end
    DrawRectangle(0.491+0.216*scalar,y+0.023,0.006,0.011,gold[1],gold[2],gold[3],255)
   end
  else label(">",0.720,y,0.019,ink[1],ink[2],ink[3],bg,false) end
 end
 if editorPage=="controls" then
  local lines={PadText("Left stick: steer | A: push | B: brake","Left stick: steer | Cross: push | Circle: brake"),
   PadText("Right stick: tricks | LT / RT: grabs","Right stick: tricks | L2 / R2: grabs"),
   "Click right stick + Left: skate menu",
   "Click right stick + Down: Skate / Bully",
   PadText("LB + Down: marker | Hold LB + Up: respawn","L1 + Down: marker | Hold L1 + Up: respawn"),
   PadText("Start / Escape: Bully pause menu","Options / Escape: Bully pause menu"),
   "WASD: steer | Space: push | Shift: ollie",
   "F6: Skate / Bully | F5: Bully | F8: menu",
   "F7: marker | Hold F10: respawn",
   "Right stick + D-pad Right / F11: debug camera"}
  for k=1,table.getn(lines) do label(lines[k],0.268,0.33+(k-1)*0.036,0.015,32,38,46,paper,false) end
 else
  label(rows[row][5] or "Choose an option.",0.268,0.680,0.015,45,49,52,paper,false)
  if editorPage=="video" then
   local ok,w,h,fps,restart=FS_VideoStats()
   label(restart and "Restart Bully to apply display changes" or string.format("%d x %d  |  Resolution: Bully Video",w,h),0.268,0.719,0.014,67,70,70,paper,false)
  else label("Settings save automatically",0.268,0.719,0.014,67,70,70,paper,false) end
  if count>7 then label(string.format("%d / %d",row,count),0.683,0.203,0.012,245,237,214,navy,false) end
 end
 DrawRectangle(0.24,0.770,0.52,0.096,navy[1],navy[2],navy[3],255)
 label(PadText("A / Enter: select    B / Backspace: back","Cross / Enter: select    Circle / Backspace: back"),0.266,0.784,0.014,245,237,214,navy,false)
 EditorChord(0.268,0.818,2,0.029,"Close Menu")
 label("Left / Right: adjust",0.548,0.823,0.014,245,237,214,navy,false)
end
