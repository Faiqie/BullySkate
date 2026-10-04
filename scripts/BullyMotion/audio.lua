-- Use the player's installed Bully sound bank; no audio is bundled with the mod.
skateAudio={loop=false,state=nil,airAt=nil,landAt=-1000,popAt=-1000}
function SkateAudioLoad() SoundLoadBank("SkateBrd.bnk");FS_AudioReady() end
function SkateAudioStop()
 FS_AudioStop()
 if skateAudio.loop then SoundLoopPlayOnPed(gPlayer,"SKATE_ROAD",false);skateAudio.loop=false end
 skateAudio.state=nil;skateAudio.airAt=nil
end
function SkateAudioUpdate(now,x,y,z,state,ground,speed,vertical)
 if FS_AudioStep() then
  if skateAudio.loop then SoundLoopPlayOnPed(gPlayer,"SKATE_ROAD",false);skateAudio.loop=false end
  skateAudio.state=nil;skateAudio.airAt=nil;return
 end
 local riding=state>=100 and state<300
 local air=state>=200 and state<300
 local rolling=riding and not air and ground>0 and speed>(skateAudio.loop and 0.35 or 0.9)
 if rolling~=skateAudio.loop then
  SoundLoopPlayOnPed(gPlayer,"SKATE_ROAD",rolling);skateAudio.loop=rolling
 end
 if air and skateAudio.state and skateAudio.state<200 then
  skateAudio.airAt=now
  if vertical>0.2 and now-skateAudio.popAt>160 then
   SoundPlay3D(x,y,z,"SKATEOLIUPCEM");skateAudio.popAt=now
  end
 elseif not air and ground>0 and skateAudio.airAt then
  if now-skateAudio.airAt>=90 and now-skateAudio.landAt>180 and state<500 then
   SoundPlay3D(x,y,z,"SKATEOLILANDCEM");skateAudio.landAt=now
  end
  skateAudio.airAt=nil
 end
 if state>=300 and state<400 or state>=500 then skateAudio.airAt=nil end
 skateAudio.state=state
end
