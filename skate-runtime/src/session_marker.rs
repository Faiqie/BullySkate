//! Source session-marker eligibility, stance preservation and distance hold.
mod state;
mod validation;
use crate::physics::{GamePhysics,SkaterRuntime};
use skate_core::physics::{board::BodyId,skeleton_animation_record::AnimationPartTransform};
#[derive(Clone,Copy)]
struct Marker {transform:AnimationPartTransform,on_board:bool,foot_forward:bool}
pub(crate) struct SessionMarker {
 marker:Option<Marker>,hold:state::Hold,validation:validation::Validation,last_buttons:u16,
 pub flags:u32,pub sets:u32,pub returns:u32,pub progress:f32,
}
impl SessionMarker {
 pub fn load(root:&std::path::Path)->Result<Self,String>{Ok(Self{marker:None,hold:Default::default(),
  validation:validation::Validation::load(root)?,last_buttons:0,flags:0,sets:0,returns:0,progress:0.})}
 pub fn clear(&mut self){self.marker=None;self.hold.cancel();self.last_buttons=0;self.flags=0;self.progress=0.;}
 pub fn suspend(&mut self){self.hold.cancel();self.last_buttons=0;self.progress=0.;}
 pub fn update(&mut self,physics:&GamePhysics,skater:&mut SkaterRuntime,buttons:u16){
  let modifier=buttons&0x100!=0;
  let set=modifier&&buttons&2!=0&&self.last_buttons&2==0;
  let held=modifier&&buttons&1!=0;self.last_buttons=buttons;
  if !modifier{
   self.flags=u32::from(self.marker.is_some());
   self.progress=self.hold.update(false,self.marker.is_some(),0.,true).progress;return;
  }
  let p=&skater.player_input.physical;let processed=&skater.player_input.processed;
  let on_board=p.state.category_12!=500;let deck=physics.board.part_transforms()[BodyId::Deck.index()];
  let mut transform=skater.animated_skeleton.roots.animation_to_world;
  if on_board{
   for i in 0..3{transform[i][..3].copy_from_slice(&deck.basis.columns[i]);}
   transform[3]=[deck.translation.x,deck.translation.y+0.2,deck.translation.z,0.];
  }
  transform=crate::physics::facing_from_visual(transform,processed.flags_2468,processed.flags_2476,on_board);
  let state=p.state.state_16;
  let allowed=(p.state.category_12==100&&p.collision.wheel_count_0>=2&&state!=104&&deck.basis.columns[1][1]>0.71)
   ||(p.state.category_12==500&&state==500);
  // Geometry checks are needed for placement, never for each ordinary tick.
  let can_place=modifier&&allowed&&p.surface_default_mode!=8;
  self.flags=u32::from(self.marker.is_some())|u32::from(can_place)*2|u32::from(modifier)*4;
  if set&&can_place&&self.validation.check(physics.world(),transform[3]){
   self.marker=Some(Marker{transform,on_board,foot_forward:skater.animation.foot_forward()});
   self.sets=self.sets.wrapping_add(1);self.flags|=1;
  }
  let ready=p.state.flag_69==0&&state!=702&&!(physics.board_wiping_out&&p.skeleton.teleport_pending_604!=0)
   &&skater.player_input.pending_teleport().is_none();
  let distance=self.marker.map_or(0.,|m|bevy::math::Vec3::from_slice(&skater.animated_skeleton.roots.animation_to_world[3][..3])
   .distance(bevy::math::Vec3::from_slice(&m.transform[3][..3])));
  let step=self.hold.update(held,self.marker.is_some()&&!matches!(state,104|502),distance,ready);
  self.progress=step.progress;
  if step.relocate{if let Some(target)=self.marker{
   if skater.player_input.request_teleport(target.transform).is_ok(){
    skater.animation.restore_foot_forward(target.foot_forward);
    skater.teleport_state.request_manual(target.transform,target.on_board);
    self.returns=self.returns.wrapping_add(1);
   }else{self.hold.cancel();}
  }}
 }
}
