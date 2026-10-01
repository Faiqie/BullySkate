//! Native traffic owns its motion. Only snapshots and physical contacts enter
//! Skate's solver; street cars are not replaced or resimulated here.
use skate_dynamics::{SolidBody,SolidCollider,rapier3d::prelude::{Pose,Rotation,Vector,SharedShape}};
#[derive(Clone)]
pub struct Vehicle {pub id:u64,pub position:Vector,pub forward:Vector,pub velocity:Vector,pub half:Vector,pub center:Vector,pub age:f32}
#[derive(Clone,Copy)]
pub struct HandAnchor {pub center:[f32;3],pub right:[f32;3],pub up:[f32;3],pub half_width:f32,pub min_up:f32,pub max_up:f32}
impl Vehicle {
 pub fn from_record(r:&[f32;16])->Option<Self>{
  if r.iter().any(|v|!v.is_finite())||r[0]<0.||r[10..13].iter().any(|v|*v<0.1||*v>8.){return None}
  let forward=Vector::new(r[4],r[6],-r[5]).try_normalize()?;
  if forward.y.abs()>0.45{return None}
  Some(Self{id:r[0] as u64,position:Vector::new(r[1],r[3],-r[2]),forward,
   velocity:Vector::new(r[7],r[9],-r[8]).clamp_length_max(40.),
   half:Vector::new(r[10],r[12],r[11]),center:Vector::new(r[13],r[15],-r[14]),age:0.})
 }
 pub fn rotation(&self)->Rotation{
  let right=self.forward.cross(Vector::Y).normalize();let up=right.cross(self.forward);
  Rotation::from_mat3(&skate_dynamics::rapier3d::prelude::Matrix::from_cols(right,up,-self.forward))
 }
 pub fn rear(&self)->Vector{self.position+self.rotation()*Vector::new(self.center.x,self.center.y-self.half.y+0.65,self.center.z+self.half.z+0.06)}
 pub fn tow_position(&self,side:f32)->Vector{self.rear()-self.forward*0.55+self.rotation()*Vector::X*(side.clamp(-1.,1.)*self.half.x*0.6)}
 pub fn solid(&self)->SolidBody{
  let center=self.position+self.rotation()*self.center;let pose=Pose::from_parts(center,self.rotation());
  SolidBody{id:0x100000000|self.id,pose,center_of_mass:center,inertia_rotation:self.rotation(),inverse_mass:0.,inverse_inertia:Vector::ZERO,
   linvel:self.velocity,angvel:Vector::ZERO,contact_group:8,
   colliders:vec![SolidCollider{shape:SharedShape::cuboid(self.half.x,self.half.y,self.half.z),pose,friction:0.4}]}
 }
}
