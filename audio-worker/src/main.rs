//! GPL-3.0-only sound sidecar; the game sends a pointer-free physics observation.
#![allow(dead_code,unused_imports)]
mod library;mod player_audio;mod grain_bed;mod surfaces;mod wire;mod catalog;
use library::Library;
use skate_audio::{runtime::Runtime,formats::{Bank,Project},mixmap::{MixMap,keys},player::AudioState};
use std::{path::Path,sync::{Arc,Mutex,atomic::{AtomicBool,Ordering}},time::{Instant,Duration}};
use cpal::traits::{HostTrait,DeviceTrait,StreamTrait};
pub mod skate_events {
 #[derive(Clone,Copy,Debug,Default)]
 pub struct Riding {pub speed:f32,pub surface:u32,pub grinding:bool,pub braking:bool,pub wheels:u32,pub pushes:u32,pub audio:skate_audio::player::AudioState,pub deck_contact:bool,pub deck_material:u32,pub deck_up:f32}
}
struct Engine {library:Library,rt:Runtime,m:MixMap,p:player_audio::PlayerAudio,bed:grain_bed::Bed,cadence:f32}
impl Engine {
 fn load(root:&Path)->Result<Self,String>{
  let library=Library::load(root)?;let mut rt=Runtime::new();
  for file in &library.aems().projects {rt.install_project(&Project::parse(file,&library.read(file).map_err(|e|e.to_string())?).map_err(|e|e.to_string())?);}
  for (stem,file) in &library.aems().banks {rt.load_bank(Bank::parse(stem,library.read(file).map_err(|e|e.to_string())?).map_err(|e|e.to_string())?,library.bank_pcm(stem));}
  for class in ["c_emitter_utility",skate_audio::player::seams::UTILITY,skate_audio::player::tricks::FOLEY_UTILITY] {if let Some(id)=rt.eval.class_id(class){rt.post(id,&[]);}}
  for stem in player_audio::SPLICE_BANKS {let(bank,pcm)=library.splice_bank(stem).ok_or_else(||format!("Missing player sound bank {stem}"))?;rt.splice.load_bank(stem,bank,pcm,&mut rt.mixer);}
  rt.load_streams(player_audio::WHEEL_STREAMS.iter().map(|name|library.wheels_pcm(name)).collect());
  let(presets,eq)=library.bus_tuning();rt.mixer.buses.env.presets=presets;rt.mixer.buses.eq.set_records(&eq);rt.mixer.buses.env.request(skate_audio::bus::env::DEFAULT_PRESET);
  if let Some(p)=library.flange_presets(){rt.mixer.buses.flange.set_presets(p[0],p[1]);}
  rt.mixer.buses.submix.enabled=true;
  let m=MixMap::from_bytes(&library.read(library.aems().mixmap.as_ref().ok_or("Missing Skate 3 MixMap")?).map_err(|e|e.to_string())?).map_err(|e|e.to_string())?;
  let mut p=player_audio::PlayerAudio::new(library.player_tuning(),true);
  p.contacts_on=true;p.contact_tuning=library.contacts_tuning();p.wheels_on=true;
  p.set_footstep_materials(library.footstep_materials());p.footsteps_on=false;
  p.rolling_on=true;p.rattle_on=true;p.slide_on=true;p.tricks_on=true;p.treatment_on=true;
  let bed=grain_bed::Bed::new(&library).ok_or("Missing native rolling recordings or tuning")?;
  Ok(Self{library,rt,m,p,bed,cadence:0.})
 }
 fn update(&mut self,r:&skate_events::Riding,camera:[f32;3],view:[f32;3]){
  let s=&r.audio;self.cadence+=s.dt;
  let calls=(self.cadence/skate_audio::mixmap::cadence::CONSOLE_DT).floor() as usize;
  self.cadence-=calls as f32*skate_audio::mixmap::cadence::CONSOLE_DT;
  self.p.jitter_steps=Some(calls);self.bed.slew_calls=Some(calls);
  for id in 1..=4{self.m.set_input(keys::MASTER,id,32767);}for id in [1,2,5]{self.m.set_input(keys::MUSIC,id,32767);}self.m.set_input(keys::REVERB,5,32767);
  for(i,v)in self.rt.mixer.buses.env.reverb_inputs().into_iter().enumerate(){self.m.set_input(keys::REVERB,i,v);}
  if calls>0{self.rt.mixer.buses.eq.clear(self.p.eq_jitter());}
  let l=self.p.listener(camera,view,s.dt,s);
  self.p.write_inputs(&mut self.m,s,Some(&l));self.bed.write_inputs(&mut self.m,s,false);
  let scale=self.bed.push_scale();let loose=player_audio::PlayerAudio::loose_board(s,r);
  self.p.process(&mut self.m,s,&mut self.rt,scale,loose);
  for _ in 0..calls{self.m.tick(skate_audio::mixmap::cadence::CONSOLE_DT);}
  self.p.update(&self.m,s,&mut self.rt,scale,loose);
  self.p.seam_frame(&self.m,s,s.dt,&mut self.rt);
  let routed=Some((std::mem::take(&mut self.p.routed.grains),self.p.routed.primary));
  let seams=skate_audio::player::tuning::PlayerTuning{seam_wobbles:self.p.tuning.seam_wobbles.clone(),..Default::default()};
  grain_bed::step_with(&mut self.bed,&self.library,&self.m,r,s.dt,&seams,routed,|apply|apply(&mut self.rt));
  self.rt.mixer.buses.flange.frame(std::array::from_fn(|i|self.m.level(keys::REVERB,i)));
 }
}
struct Output {engine:Arc<Mutex<Engine>>,enabled:Arc<AtomicBool>,gain:f32,block:[[f32;skate_audio::BLOCK];6],cursor:usize,phase:f64,current:[f32;2],next:[f32;2],rate:u32,channels:usize}
impl Output {
 fn frame(&mut self)->[f32;2]{
  if !self.enabled.load(Ordering::Relaxed)&&self.gain<0.001{return [0.;2]}
  if self.cursor>=skate_audio::BLOCK{if let Ok(mut e)=self.engine.lock(){self.block=*e.rt.render_block();}else{self.block=[[0.;skate_audio::BLOCK];6];}self.cursor=0;}
  let i=self.cursor;self.cursor+=1;
  // Retail's stereo output table (L,C,R,Ls,Rs,LFE; LFE omitted).
  [0.707*self.block[0][i]+0.5*self.block[1][i]+0.5*self.block[3][i],0.707*self.block[2][i]+0.5*self.block[1][i]+0.5*self.block[4][i]]
 }
 fn fill<T:cpal::SizedSample+cpal::FromSample<f32>>(&mut self,data:&mut[T]){
  let step=48000.0/self.rate as f64;let slew=1.0-(-1.0/(self.rate as f32*0.025)).exp();
  for frame in data.chunks_mut(self.channels){
   while self.phase>=1.0{self.phase-=1.0;self.current=self.next;self.next=self.frame();}
   self.gain+=((self.enabled.load(Ordering::Relaxed) as u8 as f32)*0.75-self.gain)*slew;
   let lr=std::array::from_fn::<_,2,_>(|i|{let v=(self.current[i]*(1.0-self.phase as f32)+self.next[i]*self.phase as f32)*self.gain;if v.is_finite(){v.clamp(-1.,1.)}else{0.}});self.phase+=step;
   for(i,out)in frame.iter_mut().enumerate(){*out=T::from_sample(if self.channels==1{(lr[0]+lr[1])*0.5}else if i<2{lr[i]}else{0.});}
  }
 }
}
fn stream(engine:Arc<Mutex<Engine>>,enabled:Arc<AtomicBool>)->Result<cpal::Stream,String>{
 let device=cpal::default_host().default_output_device().ok_or("No Windows audio output")?;
 let supported=device.default_output_config().map_err(|e|e.to_string())?;let format=supported.sample_format();let config:cpal::StreamConfig=supported.into();
 let mut output=Output{engine,enabled,gain:0.,block:[[0.;skate_audio::BLOCK];6],cursor:skate_audio::BLOCK,phase:1.,current:[0.;2],next:[0.;2],rate:config.sample_rate.0,channels:config.channels as usize};
 let error=|e|eprintln!("Audio output: {e}");
 let result=match format{cpal::SampleFormat::F32=>device.build_output_stream(&config,move|d:&mut[f32],_|output.fill(d),error,None),cpal::SampleFormat::I16=>device.build_output_stream(&config,move|d:&mut[i16],_|output.fill(d),error,None),cpal::SampleFormat::U16=>device.build_output_stream(&config,move|d:&mut[u16],_|output.fill(d),error,None),_=>return Err("Unsupported Windows output sample format".into())};
 result.map_err(|e|e.to_string())
}
fn main(){if let Err(e)=run(){eprintln!("Skate 3 audio stopped: {e}");std::process::exit(1);}}
fn run()->Result<(),String>{
 let args:Vec<_>=std::env::args().collect();
 let root=std::fs::read_to_string("_derpy_script_loader/scripts/BullyMotion/audio-path.txt").map_err(|e|e.to_string())?;
 let engine=Engine::load(Path::new(root.trim()))?;
 if args.len()==3&&args[1]=="--render-test"{return wire::render_test(engine,Path::new(&args[2]));}
 if args.len()!=3||args[1]!="--parent"{return Err("Start this sound worker through BullySkate".into())}
 let pid:u32=args[2].parse().map_err(|_|"Invalid parent")?;
 let mut channel=wire::Channel::open(pid)?;
 let map=surfaces::Map::load(Path::new("_derpy_script_loader/scripts/BullyMotion/assets/world.bmgeo"))?;
 let engine=Arc::new(Mutex::new(engine));let enabled=Arc::new(AtomicBool::new(false));let stream=stream(engine.clone(),enabled.clone())?;stream.play().map_err(|e|e.to_string())?;
 channel.ready();eprintln!("Skate 3 audio ready; native mixer, material routing, separate audio thread");
 let(mut seen,mut last,mut bridge)=(0,Instant::now(),wire::Bridge::default());
 while channel.parent_alive(){
  if let Some((sequence,active,frame,camera))=channel.read(){
   if sequence!=seen{seen=sequence;last=Instant::now();enabled.store(active,Ordering::Relaxed);
    if active{if bridge.new_tick(frame[0]){let r=bridge.observe(&frame,&map);if let Ok(mut e)=engine.lock(){e.update(&r,camera[..3].try_into().unwrap(),camera[3..].try_into().unwrap());}}}
    else{bridge.reset();}
   }
  }
  if last.elapsed()>Duration::from_millis(250){enabled.store(false,Ordering::Relaxed);}
  std::thread::sleep(Duration::from_millis(4));
 }
 Ok(())
}
