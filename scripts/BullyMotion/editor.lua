-- Controller and keyboard editor. The native bridge saves only changed values.
skaterOptions={1,0.7,0.7,0,0,0,1,2,3,67}
editorPage,editorRepeats,editorConfigure,editorNextConfigure="home",{},true,0
editorResumeAt=0
gestureNames={"Air guitar","Airplane","Boxing","Bruce Lee","Check time","Devil horns","Double finger guns","Dunno","Finger wag","Fists","Bicep flex","Flip table","Fonz","Freedom","Fist salute","Get away","Get outta here","Handcuffs","High pump","Low pump","Prewind","Peace","Point","Raise the roof","Shaka","Shrug","Point at sky","Snap","Soul arch","Spock","Surf's up","Swing high","Swing low","Throw arms","Thumbs down","Wings","Yard sale"}
function EditorLoad()
 skaterOptions={FS_SkaterOptions()};editorConfigure=true
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
 if GetCutsceneRunning()~=0 then say("Open Edit Skater after the scene finishes");return end
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
 elseif editorPage=="camera" then return {
  {"Skating FOV",10,"fov",nil,"40 to 110 degrees. Changes the skating camera."},
  {"Reset FOV",nil,"resetfov",nil,"Restore the default 67 degree skating camera."},
  {"Back",nil,"home"}}
 elseif editorPage=="controls" then return {{"Back",nil,"home"}}
 end
 return {{"Edit skater",nil,"skater"},{"Camera / FOV",nil,"camera"},
  {"Controls",nil,"controls"},{mode==1 and "Return to Bully" or "Skate mode",nil,"mode"},{"Resume",nil,"close"}}
end
function EditorChange(direction)
 local item=EditorRows()[row];local index=item[2]
 if not index then return end
 local old=skaterOptions[index];local value=old
 if item[3]=="enum" then value=math.mod(old+direction+table.getn(item[4]),table.getn(item[4]))
 elseif item[3]=="percent" then value=clamp(math.floor(old*20+0.5)+direction,0,20)/20
 elseif item[3]=="fov" then value=clamp(old+direction,40,110) end
 if value~=old then
  skaterOptions[index]=value;EditorSave()
  if index==10 and mode==1 then CameraSetFOV(value) end
 end
end
function EditorActivate()
 local item=EditorRows()[row];local action=item[3]
 if item[2] then EditorChange(1)
 elseif action=="mode" then
  EditorClose();if mode==1 then restore();say("Bully controls restored") else switch() end
 elseif action=="close" then EditorClose()
 elseif action=="reset" then
  local defaults={1,0.7,0.7,0,0,0,1,2,3};local k
  for k=1,9 do skaterOptions[k]=defaults[k] end;EditorSave();say("Skater settings reset")
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
 if back then return end -- Reserve View + D-pad for mode/menu shortcuts.
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
function EditorDraw()
 local rows=EditorRows();local count=table.getn(rows)
 DrawRectangle(0.235,0.13,0.53,0.76,19,22,17,242)
 DrawRectangle(0.235,0.13,0.53,0.005,174,166,120,255)
 local title=editorPage=="home" and "BULLY SKATE" or editorPage=="skater" and "EDIT SKATER" or editorPage=="gestures" and "GESTURES" or editorPage=="camera" and "CAMERA" or "CONTROLS"
 text(title,0.26,0.156,0.030)
 local first=math.max(1,row-6);local last=math.min(count,first+6);local k
 for k=first,last do
  local item=rows[k];local y=0.224+(k-first)*0.061
  if k==row then DrawRectangle(0.25,y-0.006,0.50,0.054,69,80,54,245) end
  text(item[1],0.263,y,0.019)
  local index=item[2]
  if index then
   local value=skaterOptions[index];local display
   if item[3]=="enum" then display=item[4][value+1]
   elseif item[3]=="percent" then display=string.format("%.0f%%",value*100)
   else display=string.format("%.0f deg",value) end
   text("< "..display.." >",0.48,y,0.018)
   if item[3]=="percent" or item[3]=="fov" then
    local scalar=item[3]=="percent" and value or (value-40)/70
    DrawRectangle(0.48,y+0.030,0.23,0.004,93,98,77,255)
    if scalar>0 then DrawRectangle(0.48,y+0.030,0.23*scalar,0.004,203,190,128,255) end
   end
  else text(">",0.713,y,0.019) end
 end
 if editorPage=="controls" then
  text("CONTROLLER",0.263,0.305,0.020)
  text(PadText("Left stick: steer | A: push | B: brake","Left stick: steer | Cross: push | Circle: brake"),0.263,0.349,0.017)
  text(PadText("Right stick: Flick-It | LT / RT: grabs","Right stick: Flick-It | L2 / R2: grabs"),0.263,0.390,0.017)
  text(PadText("D-pad: gestures | View + Up: this menu","D-pad: gestures | Touchpad + Up: this menu"),0.263,0.431,0.017)
  text(PadText("View + Left: Skate | View + Down: Bully","Touchpad + Left: Skate | Touchpad + Down: Bully"),0.263,0.472,0.015)
  text(PadText("LB + Down: marker | Hold LB + Up: respawn","L1 + Down: marker | Hold L1 + Up: respawn"),0.263,0.505,0.016)
  text("KEYBOARD",0.263,0.538,0.020)
  text("WASD: steer | Space: push | S: brake",0.263,0.582,0.017)
  text("Shift: ollie | F6: Skate | F5: Bully",0.263,0.623,0.017)
  text("F7: set marker | Hold F10: respawn",0.263,0.660,0.016)
 else
  text(rows[row][5] or "Select a page or return to the game.",0.263,0.690,0.016)
  text("Settings save automatically",0.263,0.731,0.014,186,177,140)
  if count>7 then text(string.format("%d / %d",row,count),0.67,0.184,0.014) end
 end
 text("D-pad / Left stick: select | Left / Right: adjust",0.263,0.791,0.015)
 text(PadText("A / Enter: open | B / Backspace: back | F8: close","Cross / Enter: open | Circle / Backspace: back | F8: close"),0.263,0.832,0.014)
end
