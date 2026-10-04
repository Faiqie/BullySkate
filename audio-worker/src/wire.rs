use super::{Engine,skate_events::Riding,surfaces::Map};
use skate_audio::player::AudioState;
use std::{ffi::c_void,path::Path,sync::atomic::{fence,Ordering}};
type Handle=*mut c_void;
#[link(name="kernel32")]unsafe extern "system"{
 fn OpenFileMappingW(access:u32,inherit:i32,name:*const u16)->Handle;
 fn MapViewOfFile(mapping:Handle,access:u32,high:u32,low:u32,size:usize)->*mut c_void;
 fn UnmapViewOfFile(memory:*const c_void)->i32;fn CloseHandle(handle:Handle)->i32;
 fn OpenProcess(access:u32,inherit:i32,pid:u32)->Handle;fn WaitForSingleObject(handle:Handle,ms:u32)->u32;
}
#[repr(C)]struct Shared {ready:u32,sequence:u32,active:u32,version:u32,frame:[f32;64],camera:[f32;6]}
const _: [();296]=[();std::mem::size_of::<Shared>()];
pub struct Channel {memory:*mut Shared,mapping:Handle,parent:Handle}
impl Channel {
 pub fn open(pid:u32)->Result<Self,String>{unsafe{
  let name:Vec<u16>=format!("Local\\BullySkateAudio-{pid}").encode_utf16().chain(Some(0)).collect();
  let mapping=OpenFileMappingW(0xf001f,0,name.as_ptr());if mapping.is_null(){return Err("Audio shared memory unavailable".into())}
  let memory=MapViewOfFile(mapping,0xf001f,0,0,296).cast::<Shared>();let parent=OpenProcess(0x100000,0,pid);
  if memory.is_null()||parent.is_null(){if !memory.is_null(){UnmapViewOfFile(memory.cast());}if !parent.is_null(){CloseHandle(parent);}CloseHandle(mapping);return Err("Audio channel or parent process unavailable".into())}
  if (*memory).version!=1{UnmapViewOfFile(memory.cast());CloseHandle(parent);CloseHandle(mapping);return Err("Audio protocol mismatch".into())}Ok(Self{memory,mapping,parent})
 }}
 pub fn ready(&mut self){unsafe{std::ptr::write_volatile(&mut(*self.memory).ready,2);}}
 pub fn parent_alive(&self)->bool{unsafe{WaitForSingleObject(self.parent,0)==258}}
 pub fn read(&self)->Option<(u32,bool,[f32;64],[f32;6])>{unsafe{
  for _ in 0..3{let sequence=std::ptr::read_volatile(&(*self.memory).sequence);if sequence&1!=0{continue}
   fence(Ordering::Acquire);let active=std::ptr::read_volatile(&(*self.memory).active)!=0;let frame=std::ptr::read_volatile(&(*self.memory).frame);let camera=std::ptr::read_volatile(&(*self.memory).camera);fence(Ordering::Acquire);
   if std::ptr::read_volatile(&(*self.memory).sequence)==sequence&&frame.iter().chain(camera.iter()).all(|v|v.is_finite()){return Some((sequence,active,frame,camera));}
  }None
 }}
}
impl Drop for Channel{fn drop(&mut self){unsafe{UnmapViewOfFile(self.memory.cast());CloseHandle(self.mapping);CloseHandle(self.parent);}}}
#[derive(Default)]pub struct Bridge {last_tick:f32,air:f32,was_air:bool,planted:bool,pushes:u32,jump:f32,materials:[u32;4],returns:f32,deck:[f32;4],at:usize,last_wheels:[[f32;3];4]}
impl Bridge {
 pub fn new_tick(&self,tick:f32)->bool{tick!=self.last_tick}
 pub fn reset(&mut self){*self=Self::default();}
 pub fn observe(&mut self,a:&[f32;64],map:&Map)->Riding{
  let state=a[2] as u32;let dt=((a[0]-self.last_tick)*a[1]).clamp(1./120.,0.1);self.last_tick=a[0];
  if a[55]!=self.returns{self.air=0.;self.was_air=false;self.deck=[0.;4];self.returns=a[55];}
  let unmanned=state>=300&&state<400||state>=500;let contact=std::array::from_fn(|i|a[12+i]>0.&&!unmanned);
  let wheels=contact.iter().filter(|v|**v).count() as u32;let airborne=(200..300).contains(&state)&&wheels==0;
  self.air=if airborne{self.air+dt}else{self.air};if airborne&&!self.was_air{self.air=dt;self.jump=a[48];}
  let planted=a[61]>0.&&!unmanned;let push=planted&&!self.planted;if push{self.pushes=self.pushes.wrapping_add(1);}self.planted=planted;
  let positions=std::array::from_fn(|i|a[16+i*3..19+i*3].try_into().unwrap());
  for i in 0..4{if !airborne{if let Some(m)=map.material(a[3] as u16,positions[i]){self.materials[i]=m;}}}
  self.deck[self.at]=a[51];self.at=(self.at+1)%4;
  let material=self.materials[0];let grinding=(400..=405).contains(&state)||(a[62] as u32&1)!=0;
  let mut s=AudioState{dt,ground_speed:a[4],board_position:a[5..8].try_into().unwrap(),board_velocity:a[8..11].try_into().unwrap(),com_velocity:a[8..11].try_into().unwrap(),com_position:a[28..31].try_into().unwrap(),wheel_count:wheels,wheel_contact:contact,wheel_material:if unmanned{[143;4]}else{self.materials},wheel_position:positions,turn:a[31],slope:a[32],airborne,air_time:self.air,brake:a[33]>0.,manual_brake:a[34]>0.,balance:a[35]>0.,grinding,bail:a[36]>0.,on_foot:state>=500,soft_wheels:a[39]<0.5,push_planted:planted,push_trigger:push,feet_in_deck_box:[a[42]>0.,a[43]>0.],grind_family:a[40] as i32,grind_material:material,local:true,jump_velocity:self.jump,scorable:a[49] as i32,deck_tilt:a[44],deck_spin:a[46],deck_spin_xy:[a[45],a[47]],grind_impact:a[41],deck_impact:self.deck.iter().copied().fold(0.,f32::max),deck_material:material,offboard_308:a[52]>0.,air_until_landing:a[53],jump_height:a[54],time_scale:1.,trick_active:a[49]>=0.,foot_down:[planted&&a[38]>0.||a[33]>0.,planted&&a[38]==0.],foot_material:[material;2],..Default::default()};
  s.com_velocity=a[58..61].try_into().unwrap();s.deck_spin=a[47];s.deck_spin_xy=[a[45],a[46]];
  s.slip=if wheels>0{skate_audio::player::state::slip(a[57])}else{0.};
  s.bail_end=a[62] as u32&2!=0;s.revert=state==102&&a[62] as u32&4!=0;s.hippy_jump=airborne&&s.scorable==234;
  self.was_air=airborne;
  Riding{speed:a[4],surface:material+1,grinding,braking:s.brake,wheels,pushes:self.pushes,audio:s,deck_contact:a[50]>0.,deck_material:material,deck_up:a[56]}
 }
}
pub fn render_test(mut engine:Engine,out:&Path)->Result<(),String>{
 use std::io::Write;let mut file=std::fs::File::create(out).map_err(|e|e.to_string())?;let mut peak=0f32;let mut energy=0f64;let mut n=0usize;
 for frame in 0..360{
  let airborne=(180..210).contains(&frame);let material=match frame/60{0=>1,1=>2,2=>5,3=>8,_=>3};
  let s=AudioState{dt:1./60.,ground_speed:6.,com_velocity:[6.,0.,0.],board_velocity:[6.,0.,0.],wheel_count:if airborne{0}else{4},wheel_contact:[!airborne;4],wheel_material:[material;4],wheel_position:[[frame as f32*0.1,0.,0.];4],com_position:[frame as f32*0.1,1.,0.],board_position:[frame as f32*0.1,0.,0.],airborne,air_time:if airborne{(frame-180) as f32/60.}else if frame>=210{0.5}else{0.},jump_velocity:0.8,local:true,time_scale:1.,feet_in_deck_box:[true;2],..Default::default()};
  let r=Riding{speed:6.,surface:material+1,wheels:s.wheel_count,audio:s,deck_up:1.,..Default::default()};engine.update(&r,[s.board_position[0]-3.,2.,0.],[1.,0.,0.]);
  for _ in 0..if frame%3==0{4}else{3}{let block=engine.rt.render_block();for i in 0..skate_audio::BLOCK{for ch in [0,2]{let v=block[ch][i];if !v.is_finite(){return Err("Non-finite rendered audio".into())}peak=peak.max(v.abs());energy+=f64::from(v*v);n+=1;file.write_all(&v.to_le_bytes()).map_err(|e|e.to_string())?;}}}
 }
 eprintln!("Audio render verified: {n} samples, peak {peak:.5}, RMS {:.5}",(energy/n as f64).sqrt());if peak<0.001{return Err("Player sounds are silent".into())}Ok(())
}
