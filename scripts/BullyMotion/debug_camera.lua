-- Free camera uses the game's camera and collision streaming. The player and
-- skating simulation remain where they were until B/Circle confirms a move.
debugCamera=nil
function DebugCameraClose(teleport)
 local cam=debugCamera;if not cam then return true end
 if teleport then
  -- Native pedestrian heading uses (-sin, cos); the view basis uses
  -- (sin, -cos). Face the view after teleport, rather than its opposite.
  local heading=cam.yaw+math.pi
  AreaLoadCollision(math.floor(cam.x),math.floor(cam.y))
  if mode==1 and physicalBoard then
   if not FS_SkateTeleport(physicalBoard,cam.x,cam.y,cam.z,heading) then
    say("Teleport is not ready; try B / Circle again");return false
   end
   local ok,x,y,z=FS_SkateRoot(physicalBoard)
   if not ok then return false end
   feet=z;yaw=heading;PedSetPosSimple(gPlayer,x,y,z)
  else PedSetPosSimple(gPlayer,cam.x,cam.y,cam.z) end
  PedFaceHeading(gPlayer,heading*180/math.pi,0)
  actorHistory={};actorUpdate=0;trafficUpdate=0
 end
 local px,py=PlayerGetPosXYZ();AreaLoadCollision(math.floor(px),math.floor(py))
 debugCamera=nil;editorResumeAt=GetTimer()+200
 FS_View();CameraReturnToPlayer();CameraReset();CameraDefaultFOV()
 if mode==0 then PedSetEffectedByGravity(gPlayer,true);PlayerSetControl(1) end
 return true
end
function DebugCameraToggle()
 if debugCamera then DebugCameraClose(false);return end
 if GetCutsceneRunning()~=0 or PlayerIsInAnyVehicle() or not PedIsValid(gPlayer) or PedGetHealth(gPlayer)<=0 then
  say("Open debug camera on foot after the scene finishes");return
 end
 if menu then EditorClose() end
 local x,y,z=PlayerGetPosXYZ();local heading=PedGetHeading(gPlayer)
 local fx,fy,fz=-math.sin(heading),math.cos(heading),0
 if mode==1 and physicalBoard then
  local ok,cx,cy,cz,dx,dy,dz=FS_SkateCamera(physicalBoard)
  -- The published frame is the same eye and forward vector used for riding.
  if ok then x,y,z,fx,fy,fz=cx,cy,cz,dx,dy,dz end
 else z=z+0.6 end
 local horizontal=math.sqrt(fx*fx+fy*fy)
 debugCamera={x=x,y=y,z=z,yaw=math.atan2(fx,-fy),pitch=math.atan2(fz,horizontal),streamAt=0,area=AreaGetVisible()}
 PlayerSetControl(0);PedSetEffectedByGravity(gPlayer,false);SkateAudioStop();hintEnd=0
end
function DebugCameraUpdate(now,dt,buttons,lx,ly,rx,ry,lt,rt,pressed)
 local cam=debugCamera;if not cam then return end
 if pressed(8192) then DebugCameraClose(true);return end
 local _,cancel=key(8);local _,confirm=key(13)
 if cancel then DebugCameraClose(false);return end
 if confirm then DebugCameraClose(true);return end
 local kw=key(87);local ks=key(83);local ka=key(65);local kd=key(68)
 local up=key(32);local down=key(160)
 local kl=key(37);local kr=key(39);local ku=key(38);local kb=key(40)
 lx=clamp(lx+(kd and 1 or 0)-(ka and 1 or 0),-1,1)
 ly=clamp(ly+(kw and 1 or 0)-(ks and 1 or 0),-1,1)
 rx=clamp(rx+(kr and 1 or 0)-(kl and 1 or 0),-1,1)
 ry=clamp(ry+(ku and 1 or 0)-(kb and 1 or 0),-1,1)
 cam.yaw=cam.yaw-rx*2.1*dt;cam.pitch=clamp(cam.pitch+ry*1.5*dt,-1.5,1.5)
 local cp=math.cos(cam.pitch);local fx,fy,fz=math.sin(cam.yaw)*cp,-math.cos(cam.yaw)*cp,math.sin(cam.pitch)
 local speed=bit(buttons,512) and 36 or bit(buttons,256) and 2 or 10
 local vertical=clamp(rt-lt+(up and 1 or 0)-(down and 1 or 0),-1,1)
 -- Normalize diagonal movement without constraining camera range.
 local length=math.max(1,math.sqrt(lx*lx+ly*ly+vertical*vertical))
 local step=speed*dt/length
 cam.x=cam.x+(fx*ly-math.cos(cam.yaw)*lx)*step
 cam.y=cam.y+(fy*ly-math.sin(cam.yaw)*lx)*step
 cam.z=cam.z+(fz*ly+vertical)*step
 if now>=cam.streamAt and (not cam.streamX or (cam.x-cam.streamX)^2+(cam.y-cam.streamY)^2>100) then
  cam.streamAt=now+500;cam.streamX,cam.streamY=cam.x,cam.y
  AreaLoadCollision(math.floor(cam.x),math.floor(cam.y))
 end
 local lens=skaterOptions[10]
 -- Native CameraSetXYZ takes the eye first, then the look target. The
 -- render camera frame, rather than CCam's target vector, verifies this order.
 CameraSetXYZ(cam.x,cam.y,cam.z,cam.x+fx*10,cam.y+fy*10,cam.z+fz*10)
 CameraSetFOV(lens);FS_View(cam.x,cam.y,cam.z,fx,fy,fz,lens)
end
function DebugCameraDraw()
 text("DEBUG CAMERA",0.17,0.175,0.022,226,188,71)
 if GetTimer()<hintEnd then text(hint,0.17,0.292,0.017) end
 if performanceOptions and performanceOptions[9]==0 then return end
 text(PadText("Left stick: fly   Right stick: look   LT / RT: down / up","Left stick: fly   Right stick: look   L2 / R2: down / up"),0.17,0.207,0.017)
 text(PadText("RB: faster   LB: slower   B: teleport","R1: faster   L1: slower   Circle: teleport"),0.17,0.232,0.017)
 text("Right stick click + D-pad Right: cancel",0.17,0.257,0.017)
end
