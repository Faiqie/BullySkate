//! Game-specific interaction producer. The supplied rewrite has the Shove
//! animation but no native NPC/traffic owner. It does not implement state 104;
//! towing therefore applies a bounded velocity impulse to the existing solver.
use super::{physics::{GamePhysics,SkaterRuntime},bully_actors::{Actor,solid},bully_vehicles::Vehicle};
use skate_core::{input::xbox::XboxState,math::Vector3};
use skate_dynamics::{solid::sweep_sphere,rapier3d::prelude::Vector};
#[derive(Default)]
pub struct Interactions {
 pub tow:Option<u64>,pub hand:Option<super::bully_vehicles::HandAnchor>,pub hits:u32,pub hit_id:u32,
 rb:bool,cooldown:f32,attack:Option<Attack>,
}
struct Attack {id:u64,age:f32,old:[Vector;9],hit:bool}
fn deck_points(physics:&GamePhysics)->[Vector;9]{
 let deck=physics.board.part_transforms()[6];let center=Vector::new(deck.translation.x,deck.translation.y,deck.translation.z);
 let forward=Vector::from_array(deck.basis.columns[2])*0.41;
 std::array::from_fn(|i|center+forward*(i as f32/4.-1.))
}
fn rider(skater:&SkaterRuntime)->(Vector,Vector){
 let root=skater.animated_skeleton.roots.animation_to_world;
 (Vector::from_array(root[3][..3].try_into().unwrap()),Vector::from_array(root[2][..3].try_into().unwrap()))
}
impl Interactions {
 pub fn suspend(&mut self){self.tow=None;self.hand=None;self.rb=false;self.attack=None;self.cooldown=0.;}
 pub fn before(&mut self,physics:&mut GamePhysics,skater:&mut SkaterRuntime,actors:&[Actor],vehicles:&[Vehicle],state:&XboxState){
  let dt=physics.period().as_secs_f32();self.cooldown=(self.cooldown-dt).max(0.);
  let rb=state.buttons&0x200!=0;let edge=rb&&!self.rb;self.rb=rb;
  let category=skater.player_state.current().category();let (root,forward)=rider(skater);
  if skater.player_input.pending_teleport().is_some(){self.suspend();return}
  if category==500&&edge&&self.cooldown==0.&&skater.player_input.physical.off_board.flag_311!=0{
   let target=actors.iter().filter(|a|{
    let d=a.position-root;d.x*d.x+d.z*d.z<2.1*2.1&&d.y.abs()<1.4&&d.dot(forward)>0.05
   }).min_by(|a,b|a.position.distance_squared(root).total_cmp(&b.position.distance_squared(root)));
   if let Some(a)=target{self.attack=Some(Attack{id:a.id,age:0.,old:deck_points(physics),hit:false});self.cooldown=0.75;}
  }
  if let Some(attack)=&mut self.attack{
   attack.age+=dt;
   if category!=500||attack.age>0.65{self.attack=None;}else if attack.age<0.09{
    if let Some(a)=actors.iter().find(|a|a.id==attack.id){
     let point=[a.position.x,a.position.y+a.height*0.6,a.position.z,1.].map(f32::to_bits);
     skater.player_input.player.probe.bytes_72_73[0]=1;
     skater.player_input.player.probe.vectors_16_32_48=[point;3];
    }
   }
  }
  // RB release, braking, dismounting, loss of ground or stale traffic detaches.
  if !rb||category!=100||state.buttons&(0x2000|0x8000)!=0||skater.player_input.processed.wheel_count_2556<2{
   self.tow=None;self.hand=None;return
  }
  let board=physics.board.bodies()[6].rates;let position=Vector::new(board.position.x,board.position.y,board.position.z);
  if self.tow.is_none(){
   self.tow=vehicles.iter().filter(|v|{
    let delta=position-v.rear();let gap=-delta.dot(v.forward);
    let side=delta.dot(v.rotation()*Vector::X);
    let speed=v.velocity.dot(v.forward);
    // Jimmy's native arm lengths require a close reach. Do not attach from a
    // distance that would leave his hand floating behind the vehicle.
    gap>0.42&&gap<0.70&&side.abs()<v.half.x*0.8&&delta.y.abs()<1.6&&speed>0.4&&speed<25.&&v.age<0.2
   }).min_by(|a,b|a.tow_position(0.).distance_squared(position).total_cmp(&b.tow_position(0.).distance_squared(position))).map(|v|v.id);
  }
  let Some(vehicle)=vehicles.iter().find(|v|Some(v.id)==self.tow&&v.age<0.2)else{self.tow=None;self.hand=None;return};
  let target=vehicle.tow_position(state.left[0] as f32/32768.);
  let offset=target-position;let velocity=Vector::new(board.linear_velocity.x,board.linear_velocity.y,board.linear_velocity.z);
  if offset.length_squared()>3.5*3.5||vehicle.velocity.dot(vehicle.forward)< -0.5{
   self.tow=None;self.hand=None;return
  }
  let desired=vehicle.velocity+Vector::new(offset.x,0.,offset.z)*7.;
  let correction=Vector::new(desired.x-velocity.x,0.,desired.z-velocity.z).clamp_length_max(12.*dt);
  let impulse=Vector3::new(correction.x,0.,correction.z);
  for body in physics.board.bodies_mut(){body.rates.linear_velocity.x+=impulse.x;body.rates.linear_velocity.z+=impulse.z;}
  for body in skater.skeleton.bodies_mut(){body.rates.linear_velocity.x+=impulse.x;body.rates.linear_velocity.z+=impulse.z;}
  let right=vehicle.rotation()*Vector::X;let up=vehicle.rotation()*Vector::Y;let hand=vehicle.rear()+up*0.30;
  self.hand=Some(super::bully_vehicles::HandAnchor{center:[hand.x,-hand.z,hand.y],right:[right.x,-right.z,right.y],up:[up.x,-up.z,up.y],half_width:vehicle.half.x*0.8,min_up:-0.55,max_up:2.*vehicle.half.y-1.05});
 }
 pub fn after(&mut self,physics:&GamePhysics,skater:&SkaterRuntime,actors:&[Actor]){
  let Some(attack)=&mut self.attack else{return};let points=deck_points(physics);
  if attack.age>=0.10&&!attack.hit&&skater.animation.motion.animation.channels.has("Shove"){
   for actor in actors{
    let body=solid(actor);
    if points.iter().zip(attack.old).any(|(to,from)|sweep_sphere(std::slice::from_ref(&body),from.to_array(),to.to_array(),0.19).is_some()){
     attack.hit=true;self.hits=self.hits.wrapping_add(1);self.hit_id=actor.id as u32;break;
    }
   }
  }
  attack.old=points;
 }
}
