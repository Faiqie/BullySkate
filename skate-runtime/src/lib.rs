#![allow(dead_code,unused_imports)]
mod animation;
mod animation_pose;
mod camera;
mod crash_context;
mod custom_difficulty;
mod difficulty;
mod graph_host;
mod graph_runtime;
mod grind_world;
mod input;
mod physics;
mod scoring_runtime;
mod skater_animation;
mod modding;
mod host_extensions;
mod skate_world;
mod bully_map;
mod retarget;
mod bully_actors;
mod bully_vehicles;
mod bully_interactions;
mod preferences;
mod session_marker;
pub use preferences::SkaterPreferences;
pub struct SkateHost {
 physics:physics::GamePhysics,skater:physics::SkaterRuntime,
 controls:physics::PlayerControls,graphs:graph_runtime::StockGraphs,
 camera:camera::CameraRuntime,input:input::ControllerInput,
 retarget:Option<retarget::Retarget>,
 actors:Vec<bully_actors::Actor>,
 preferences:SkaterPreferences,
 marker:session_marker::SessionMarker,
 geometry:Option<std::path::PathBuf>,area:u32,
 vehicles:Vec<bully_vehicles::Vehicle>,interactions:bully_interactions::Interactions,actor_age:f32,
}
impl SkateHost {
 fn headless_assets(root:&std::path::Path)->Result<skate_data::GameAssets,String>{
  // Jimmy's original Bully skin is rendered by the game. Loading the source
  // skater mesh is unnecessary; the graph and animation readers validate
  // the source assets they actually consume.
  let json=std::fs::read_to_string(root.join("private/game.json")).map_err(|e|e.to_string())?;
  skate_data::GameAssets::parse(&json).map_err(|e|e.to_string())
 }
 pub fn bully(root:&std::path::Path,geometry:&std::path::Path,spawn:[f32;3],heading:f32)->Result<Self,String>{
  let assets=Self::headless_assets(root)?;
  let graphs=graph_runtime::StockGraphs::load(root,&assets)?;
  let map=bully_map::load(geometry,spawn,heading,0)?;
  let physics=physics::GamePhysics::load_with_difficulty(root,Some(&map),difficulty::Difficulty::Easy)?;
  let skater=physics::SkaterRuntime::load(root,&graphs,&physics,"easy")?;
  let controls=physics::PlayerControls::load(root)?;
  let camera=camera::CameraRuntime::load(root)?;
  let retarget=Some(retarget::Retarget::load(&geometry.with_file_name("jimmy-bind.json"),&skater)?);
  let marker=session_marker::SessionMarker::load(root)?;
  Ok(Self{physics,skater,controls,graphs,camera,input:Default::default(),retarget,actors:Vec::new(),preferences:Default::default(),marker,geometry:Some(geometry.into()),area:0,vehicles:Vec::new(),interactions:Default::default(),actor_age:0.})
 }
 pub fn bone_names(&self)->&[String]{&self.skater.animation.evaluator.frames.bone_names}
 pub fn pose(&self)->&[skate_core::animation::output::NativeMatrix]{&self.skater.render_pose}
 /// Source world transforms for offline rig/contact diagnostics.
 pub fn world_pose(&self)->Vec<skate_core::animation::output::NativeMatrix>{
  use bevy::math::{Mat3,Vec3};
  let root=&self.skater.animated_skeleton.roots.animation_to_world;
  let a=Mat3::from_cols_array_2d(&std::array::from_fn(|i|root[i][..3].try_into().unwrap()));
  let o=Vec3::from_array(root[3][..3].try_into().unwrap());
  self.pose().iter().map(|m|{
   let r=a*Mat3::from_cols_array_2d(&std::array::from_fn(|i|m[i][..3].try_into().unwrap()));
   let p=a*Vec3::from_array(m[3][..3].try_into().unwrap())+o;
   let mut n=[[0.;4];4];for(i,col)in r.to_cols_array_2d().iter().enumerate(){n[i][..3].copy_from_slice(col);}n[3][..3].copy_from_slice(&p.to_array());n
  }).collect()
 }
 pub fn native_pose(&self)->Option<[[f32;13];36]>{self.retarget.as_ref().map(|r|r.world_pose(&self.skater,self.interactions.hand))}
 pub fn native_board_pose(&self)->[[f32;13];2]{
  use bevy::math::{Mat3,Vec3};let deck=self.physics.board.part_transforms()[6];
  let convert=|p:[f32;3]|Vec3::new(p[0],-p[2],p[1]);
  let basis=Mat3::from_cols(-convert(deck.basis.columns[0]),convert(deck.basis.columns[2]),convert(deck.basis.columns[1]));
  // The original board is 1.04 m long and 0.14 m deep; match the recovered
  // deck's 0.83 m length and wheel/deck clearance while keeping its native skin.
  let rotation=basis*Mat3::from_diagonal(Vec3::new(0.95,0.8,0.62));
  let position=convert([deck.translation.x,deck.translation.y,deck.translation.z])-rotation*Vec3::new(0.,0.,0.13020806);
  std::array::from_fn(|i|{let p=position+if i==1{rotation*Vec3::new(-1.1511099e-8,-0.26334324,0.09619658)}else{Vec3::ZERO};let mut m=[0.;13];m[..9].copy_from_slice(&rotation.transpose().to_cols_array());m[9..12].copy_from_slice(&p.to_array());m[12]=1.;m})
 }
 pub fn native_bone_names(&self)->Option<Vec<&str>>{self.retarget.as_ref().map(|r|r.names().collect())}
 pub fn period(&self)->f32{self.physics.period().as_secs_f32()}
 pub fn camera(&mut self,aspect:f32)->Option<[f32;7]>{
  self.camera.set_aspect_ratio(aspect);self.camera.presentation_frame().map(|f|{
   let p=f.position;let at=f.basis.columns[2];[p[0],-p[2],p[1],at[0],-at[2],at[1],self.preferences.fov]
  })
 }
 pub fn rider_root(&self)->[f32;3]{let p=self.skater.animated_skeleton.roots.animation_to_world[3];[p[0],-p[2],p[1]]}
 pub fn relocate(&mut self,position:[f32;3],heading:f32)->Result<(),String>{
  let rotation=bevy::math::Mat3::from_rotation_y(heading);
  let mut transform=skate_core::physics::skeleton_animation_record::IDENTITY;
  for (col,values) in rotation.to_cols_array_2d().iter().enumerate(){transform[col][..3].copy_from_slice(values);}
  let clearance=self.physics.settings.wheel_radius-self.physics.settings.authored[0].translation.y;
  transform[3]=[position[0],position[1]+clearance,position[2],0.];
  self.skater.travel_to(transform)
 }
 pub fn select_area(&mut self,area:u32)->Result<(),String>{
  if area==self.area{return Ok(())}
  let path=self.geometry.as_ref().ok_or("This host has no Bully area map")?;
  let map=bully_map::load(path,self.position(),0.,area)?;
  self.physics.replace_map(&map)?;self.area=area;self.actors.clear();self.marker.clear();
  Ok(())
 }
 pub fn output(&self)->[f32;14]{
  let p=self.physics.board.part_transforms()[6];let velocity=self.physics.board.bodies()[6].rates.linear_velocity;
  let forward=p.basis.columns[2];let up=p.basis.columns[1];
  let yaw=(-forward[0]).atan2(-forward[2]);let pitch=forward[1].clamp(-1.,1.).asin();
  let(sy,cy)=yaw.sin_cos();let(sp,cp)=pitch.sin_cos();
  let roll=-(up[0]*cy-up[2]*sy).atan2(up[0]*sy*sp+up[2]*cy*sp+up[1]*cp);
  let clearance=self.physics.settings.wheel_radius-self.physics.settings.authored[0].translation.y;
  [p.translation.x,-p.translation.z,p.translation.y,yaw,pitch,roll,velocity.x,-velocity.z,velocity.y,0.,self.physics.contact_count as f32,self.physics.ticks as f32,clearance,if self.skater.player_state.current().category()==100{1.}else{0.}]
 }
 pub fn flat(root:&std::path::Path)->Result<Self,String>{
  let assets=Self::headless_assets(root)?;
  let graphs=graph_runtime::StockGraphs::load(root,&assets)?;
  let physics=physics::GamePhysics::host_flat(root)?;
  let skater=physics::SkaterRuntime::load(root,&graphs,&physics,"easy")?;
  let controls=physics::PlayerControls::load(root)?;
  let camera=camera::CameraRuntime::load(root)?;
  let marker=session_marker::SessionMarker::load(root)?;
  Ok(Self{physics,skater,controls,graphs,camera,input:Default::default(),retarget:None,actors:Vec::new(),preferences:Default::default(),marker,geometry:None,area:0,vehicles:Vec::new(),interactions:Default::default(),actor_age:0.})
 }
 pub fn configure(&mut self,preferences:SkaterPreferences){
  self.physics.set_difficulty(if preferences.motorized{difficulty::Difficulty::Motorized}else{difficulty::Difficulty::Easy});
  self.physics.set_equipment_preferences(preferences.trucks,preferences.wheels);
  self.physics.set_gesture_preferences(Some(preferences.gestures));
  self.skater.animation.set_customisation(preferences.stance,preferences.style);
  self.skater.animation.motion.animation.posture.set_profile(preferences.posture);
  self.preferences=preferences;
 }
 pub fn marker_status(&self)->(u32,u32,u32,f32){(self.marker.flags,self.marker.sets,self.marker.returns,self.marker.progress)}
 pub fn clear_marker(&mut self){self.marker.clear();}
 pub fn suspend_marker(&mut self){self.marker.suspend();}
 pub fn actors(&mut self,records:&[[f32;9]]){
  use skate_dynamics::rapier3d::prelude::Vector;
  self.actors.clear();self.actor_age=0.;
  for r in records.iter().take(24){
   if r.iter().any(|v|!v.is_finite())||r[0]<0.{continue}
   self.actors.push(bully_actors::Actor::with_dimensions(r[0] as u64,Vector::new(r[1],r[3],-r[2]),Vector::new(r[4],r[6],-r[5]).clamp_length_max(15.),r[7],r[8]));
  }
 }
 pub fn vehicles(&mut self,records:&[[f32;16]]){self.vehicles=records.iter().take(8).filter_map(bully_vehicles::Vehicle::from_record).collect();}
 pub fn interaction_status(&self)->[u32;4]{[self.state(),self.interactions.tow.map(|id|id as u32).unwrap_or(u32::MAX),self.interactions.hits,self.interactions.hit_id]}
 pub fn suspend_interactions(&mut self){self.interactions.suspend();}
 pub fn tick(&mut self,mut state:skate_core::input::xbox::XboxState)->Result<(),String>{
  self.marker.update(&self.physics,&mut self.skater,state.buttons);
  if state.buttons&0x100!=0{state.buttons&=!(0x100|0x0f);}
  if self.actor_age>0.2{self.actors.clear();}
  self.vehicles.retain(|v|v.age<=0.2);
  self.interactions.before(&mut self.physics,&mut self.skater,&self.actors,&self.vehicles,&state);
  let mut proxies=std::mem::take(&mut self.physics.network_proxies);
  proxies.bodies.clear();proxies.volumes.clear();proxies.solids.clear();proxies.groups.clear();proxies.actors.clear();proxies.dynamics_before.clear();proxies.dynamics_deltas.clear();
  for actor in &self.actors{proxies.append_solid(bully_actors::solid(actor),&self.physics,&self.skater,false);}
  for vehicle in &self.vehicles{proxies.append_solid(vehicle.solid(),&self.physics,&self.skater,false);}
  let queries=if proxies.solids.is_empty(){None}else{Some(std::sync::Arc::new(bully_actors::MovingQueries(proxies.solids.iter().map(|(_,body)|body.clone()).collect())) as std::sync::Arc<dyn skate_core::physics::board_world::ExternalQueries>)};
  self.physics.set_external_queries(queries);
  self.physics.network_proxies=proxies;
  let published=self.input.host_sample(state);let mut actions=published.actions();
  self.controls.update_for_physics(&mut actions,&self.physics,&self.skater,&self.camera)?;
  self.controls.publish_gestures(self.physics.difficulty_index(),self.skater.player_input.physical.state.state_16);
  let result=self.physics.host_tick(&mut self.skater,&mut self.controls,&self.graphs,&mut actions,&mut self.camera);
  self.interactions.after(&self.physics,&self.skater,&self.actors);
  let period=self.period();self.actor_age+=period;for actor in &mut self.actors{actor.position+=actor.velocity*period;}
  for vehicle in &mut self.vehicles{vehicle.position+=vehicle.velocity*period;vehicle.age+=period;}
  result
 }
 pub fn position(&self)->[f32;3]{let p=self.physics.board.bodies()[6].rates.position;[p.x,p.y,p.z]}
 pub fn state(&self)->u32{self.skater.player_state.current() as u32}
 pub fn actor_contacts(&self)->usize{self.physics.network_contacts}
 /// Explicit initial velocity for offline host probes; ordinary gameplay uses
 /// the recovered state/force owners to produce every subsequent velocity.
 pub fn seed_velocity(&mut self,value:[f32;3]){for b in self.physics.board.bodies_mut(){b.rates.linear_velocity=skate_core::math::Vector3::new(value[0],value[1],value[2]);}}
 pub fn status(&self)->String{format!("ticks={} mode={} state={:?} contacts={} clip={:?} pose_bones={} rider_com={:?} held={} shove={} tow={:?} hits={}",self.physics.ticks,self.physics.difficulty_index(),self.skater.player_state.current(),self.physics.contact_count,self.skater.animation.motion.animation.current_name,self.skater.render_pose.len(),self.skater.skeleton.record.centre_of_mass,self.skater.player_input.physical.off_board.flag_311,self.skater.animation.motion.animation.channels.has("Shove"),self.interactions.tow,self.interactions.hits)}
}
