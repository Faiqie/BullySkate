//! Nearby Bully pedestrians enter the source shared contact solve as kinematic
//! solids. Bully retains ownership of NPC motion; only the skater reacts.
use skate_core::{math::Vector3,physics::{board_world::{ExternalQueries,ExternalLineHit,WorldLineHit},triangle_query::TriangleLineHit}};
use skate_dynamics::{SolidBody,SolidCollider,solid::{sweep_sphere,collider_triangles},rapier3d::prelude::{Pose,Rotation,Vector,SharedShape}};
use std::sync::{Arc,OnceLock};
#[derive(Clone)]
pub struct Actor {pub id:u64,pub position:Vector,pub velocity:Vector,pub height:f32,pub radius:f32,shape:SharedShape}
impl Actor{
 pub fn new(id:u64,position:Vector,velocity:Vector,height:f32,radius:f32)->Self{
  let height=height.clamp(0.6,2.8);let radius=radius.clamp(0.18,0.5).min(height*0.45);
  Self{id,position,velocity,height,radius,shape:SharedShape::capsule_y(height*0.5-radius,radius)}
 }
}
pub fn solid(actor:&Actor)->SolidBody {
 let shape=actor.shape.clone();let center=actor.position+Vector::Y*(actor.height*0.5);
 let pose=Pose::from_translation(center);
 SolidBody{id:actor.id,pose,center_of_mass:center,inertia_rotation:Rotation::IDENTITY,
  inverse_mass:0.,inverse_inertia:Vector::ZERO,linvel:actor.velocity,angvel:Vector::ZERO,
  contact_group:8,colliders:vec![SolidCollider{shape,pose,friction:0.6}]}
}
pub struct MovingQueries(pub Vec<SolidBody>);
fn capsule_triangles()->&'static Vec<[[f32;3];3]>{
 static TRIANGLES:OnceLock<Vec<[[f32;3];3]>>=OnceLock::new();
 TRIANGLES.get_or_init(||collider_triangles(&SolidCollider{shape:SharedShape::capsule_y(0.50,0.28),pose:Pose::IDENTITY,friction:0.6}))
}
fn intersects(min:Vector,max:Vector,a:Vector,b:Vector)->bool{min.cmple(b).all()&&a.cmple(max).all()}
impl ExternalQueries for MovingQueries {
 fn line(&self,start:Vector3,end:Vector3,radius:f32)->Option<ExternalLineHit>{
  let from=Vector::new(start.x,start.y,start.z);let to=Vector::new(end.x,end.y,end.z);
  if !from.is_finite()||!to.is_finite()||!radius.is_finite()||radius<0.{return None}
  let min=from.min(to)-Vector::splat(radius);let max=from.max(to)+Vector::splat(radius);
  let mut nearest=None;
  for body in &self.0{
   if !body.colliders.iter().any(|c|{let b=c.shape.compute_aabb(&c.pose);intersects(min,max,b.mins,b.maxs)}){continue}
   if let Some((id,c))=sweep_sphere(std::slice::from_ref(body),from.to_array(),to.to_array(),radius){
    if nearest.as_ref().is_none_or(|(_,old):&(_,skate_dynamics::solid::SolidContact)|c.time_of_impact<old.time_of_impact){nearest=Some((id,c))}
   }
  }
  let(id,c)=nearest?;let body=self.0.iter().find(|b|b.id==id)?;
  let convert=|v:Vector|Vector3::new(v.x,v.y,v.z);
  let mut frame=skate_core::physics::skeleton_animation_record::IDENTITY;
  let rotation=skate_dynamics::rapier3d::prelude::Matrix::from_quat(body.pose.rotation);
  for (i,col) in rotation.to_cols_array_2d().iter().enumerate(){frame[i][..3].copy_from_slice(col);}
  frame[3]=[body.pose.translation.x,body.pose.translation.y,body.pose.translation.z,1.];
  Some(ExternalLineHit{hit:WorldLineHit{geometry:TriangleLineHit{position:convert(c.point_b),normal:convert(c.normal),fraction:c.time_of_impact,volume_parameter:[0.;3]},tag:0},surface:0,geometry_id:0x8000_0000|(id as u32&0x7fff_ffff),frame})
 }
 fn nearby(&self,center:Vector3,radius:f32)->Vec<[Vector3;3]>{
  let center=Vector::new(center.x,center.y,center.z);let mut output=Vec::new();
  if !center.is_finite()||!radius.is_finite()||radius<0.{return output}
  for body in &self.0 {for collider in &body.colliders {
   let bounds=collider.shape.compute_aabb(&collider.pose);
   if (center-center.clamp(bounds.mins,bounds.maxs)).length_squared()>radius*radius{continue}
   let owned;let local=if collider.shape.as_capsule().is_some(){capsule_triangles()}else{
    owned=collider_triangles(&SolidCollider{shape:collider.shape.clone(),pose:Pose::IDENTITY,friction:collider.friction});&owned
   };
   for triangle in local{
    let points=triangle.map(|p|{
     let mut point=Vector::from_array(p);
     if let Some(capsule)=collider.shape.as_capsule(){let radial=capsule.radius/0.28;let half=(capsule.segment.b.y-capsule.segment.a.y).abs()*0.5;
      point.x*=radial;point.z*=radial;point.y=if point.y>0.5{half+(point.y-0.5)*radial}else if point.y< -0.5{-half+(point.y+0.5)*radial}else{point.y*half/0.5};
     }collider.pose*point
    });
    let lo=points[0].min(points[1]).min(points[2]);let hi=points[0].max(points[1]).max(points[2]);
    if (center-center.clamp(lo,hi)).length_squared()>radius*radius{continue}
    output.push(points.map(|p|Vector3::new(p.x,p.y,p.z)));
    if output.len()>=64{return output}
   }
  }}output
 }
}
pub fn queries(actors:&[Actor],vehicles:&[super::bully_vehicles::Vehicle])->Arc<dyn ExternalQueries>{
 Arc::new(MovingQueries(actors.iter().map(solid).chain(vehicles.iter().map(|v|v.solid())).collect()))
}

#[cfg(test)]
mod tests{
 use super::*;
 #[test]fn broad_phase_preserves_nearest_exact_sphere_cast(){
  let bodies:Vec<_>=(0..24).map(|i|solid(&Actor::new(i,Vector::new((i%6) as f32*2.,0.,(i/6) as f32*2.),Vector::ZERO,1.2+(i%4) as f32*0.2,0.22+(i%3) as f32*0.06))).collect();
  let optimized=MovingQueries(bodies.clone());
  let mut seed=19u32;
  for _ in 0..2000{
   let mut random=||{seed=seed.wrapping_mul(1664525).wrapping_add(1013904223);(seed>>8) as f32/16777216.};
   let start=Vector::new(random()*16.-3.,random()*3.,random()*12.-3.);
   let end=Vector::new(random()*16.-3.,random()*3.,random()*12.-3.);let radius=random()*0.5;
   let old=sweep_sphere(&bodies,start.to_array(),end.to_array(),radius);
   let new=optimized.line(Vector3::new(start.x,start.y,start.z),Vector3::new(end.x,end.y,end.z),radius);
   assert_eq!(old.is_some(),new.is_some());
   if let(Some((_,old)),Some(new))=(old,new){assert!((old.time_of_impact-new.hit.geometry.fraction).abs()<1e-5);}
  }
 }
 #[test]fn a_local_query_does_not_publish_distant_capsule_faces(){
  let body=solid(&Actor::new(1,Vector::ZERO,Vector::ZERO,1.56,0.28));
  let query=MovingQueries(vec![body]);let near=query.nearby(Vector3::new(0.,0.1,0.),0.2);
  assert!(!near.is_empty());assert!(query.nearby(Vector3::new(30.,0.,0.),0.2).is_empty());
  for tri in near{let p=tri.map(|p|Vector::new(p.x,p.y,p.z));let lo=p[0].min(p[1]).min(p[2]);let hi=p[0].max(p[1]).max(p[2]);
   let center=Vector::new(0.,0.1,0.);assert!((center-center.clamp(lo,hi)).length_squared()<=0.04+1e-6);
  }
 }
}
