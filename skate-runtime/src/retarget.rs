//! Retarget global Skate poses onto Jimmy's original native bind skeleton.
//! Rotational bind offsets and native bone lengths preserve his skin proportions.
use bevy::math::{Mat3,Vec3,Quat};
use skate_core::animation::output::{NativeMatrix,Sqt};
use super::physics::SkaterRuntime;
#[derive(serde::Deserialize)]
struct Bone {name:String,parent:i32,matrix:[[f32;3];4]}
pub(crate) struct Retarget {bones:Vec<Bone>,source:Vec<(Mat3,Vec3)>,mapping:Vec<Option<usize>>}
fn parts(m:&NativeMatrix)->(Mat3,Vec3){
 (Mat3::from_cols_array_2d(&std::array::from_fn(|i|m[i][..3].try_into().unwrap())),Vec3::from_array(m[3][..3].try_into().unwrap()))
}
fn native(m:&Bone)->(Mat3,Vec3){(Mat3::from_cols_array_2d(&m.matrix[..3].try_into().unwrap()),Vec3::from_array(m.matrix[3]))}
fn convert()->Mat3{Mat3::from_cols(Vec3::X,Vec3::Z,-Vec3::Y)}
fn mapped(name:&str)->Option<&'static str>{Some(match name {
 "Root"|"Root Pelvis"=>"HIPS","Root01"|"Root Spine"=>"SPINE",
 "Root Spine1"=>"SPINE2","Root Spine2"=>"SPINE3","Root Neck"=>"NECK","Root Head"=>"HEAD",
 "Root L Thigh"=>"LEFTUPLEG","Root L Calf"=>"LEFTLEG","Root L Foot"=>"LEFTFOOT",
 "Root R Thigh"=>"RIGHTUPLEG","Root R Calf"=>"RIGHTLEG","Root R Foot"=>"RIGHTFOOT",
 "Root L Clavicle"=>"LEFTSHOULDER","Root L UpperArm"=>"LEFTARM","Root L Forearm"=>"LEFTFOREARM","Root L Hand"=>"LEFTHAND",
 "Root R Clavicle"=>"RIGHTSHOULDER","Root R UpperArm"=>"RIGHTARM","Root R Forearm"=>"RIGHTFOREARM","Root R Hand"=>"RIGHTHAND",
 _=>return None})}
impl Retarget {
 pub fn load(path:&std::path::Path,skater:&SkaterRuntime)->Result<Self,String>{
  let bones:Vec<Bone>=serde_json::from_slice(&std::fs::read(path).map_err(|e|format!("{}: {e}",path.display()))?).map_err(|e|e.to_string())?;
  if bones.len()!=36{return Err("Jimmy bind skeleton must contain 36 native bones".into())}
  for(i,b)in bones.iter().enumerate(){if b.parent>=i as i32||b.parent< -1||b.matrix.iter().flatten().any(|v|!v.is_finite()){return Err("Invalid Jimmy bind transform".into())}}
  let reference=skater.animation.evaluator.frames.named_pose("RIG_TPOSE")?;
  let pose:Vec<Sqt>=reference.samples.iter().map(|w|Sqt{scale:[f32::from_bits(w[0]),f32::from_bits(w[1]),f32::from_bits(w[2]),0.],rotation:std::array::from_fn(|i|f32::from_bits(w[3+i])),translation:[f32::from_bits(w[7]),f32::from_bits(w[8]),f32::from_bits(w[9]),0.]}).collect();
  let source=skater.animation.evaluator.hierarchy(&pose)?.iter().map(parts).collect();
  let mapping=bones.iter().map(|b|mapped(&b.name).map(|name|skater.animation.evaluator.frames.bone_names.iter().position(|n|n.eq_ignore_ascii_case(name)).ok_or_else(||format!("Missing retarget bone {name}"))).transpose()).collect::<Result<Vec<_>,_>>()?;
  Ok(Self{bones,source,mapping})
 }
 pub fn names(&self)->impl Iterator<Item=&str>{self.bones.iter().map(|b|b.name.as_str())}
 pub fn world_pose(&self,skater:&SkaterRuntime,board:&[[f32;13];2],hand_target:Option<super::bully_vehicles::HandAnchor>)->[[f32;13];36]{
  let c=convert();let (animation,origin)=parts(&skater.animated_skeleton.roots.animation_to_world);
  let mut globals=[(Mat3::IDENTITY,Vec3::ZERO);36];let pose=&skater.render_pose;
  for(i,bone)in self.bones.iter().enumerate(){
   let (bind,bind_pos)=native(bone);let parent=(bone.parent>=0).then_some(bone.parent as usize);
   let (rotation,position)=if let Some(source)=self.mapping[i]{
    let (motion,motion_pos)=parts(&pose[source]);
    let rotation=c*animation*motion*self.source[source].0.inverse()*c.transpose()*bind;
    let position=if let Some(p)=parent.filter(|&p|self.mapping[p].is_some()){
     let ps=self.mapping[p].unwrap();let (_,parent_motion)=parts(&pose[ps]);
     let length=bind_pos.distance(native(&self.bones[p]).1);
     globals[p].1+(c*animation*(motion_pos-parent_motion)).normalize_or_zero()*length
    }else{c*(animation*motion_pos+origin)};
    (rotation,position)
   }else if let Some(p)=parent{
    let (pb,pp)=native(&self.bones[p]);let delta=globals[p].0*pb.inverse();
    (delta*bind,globals[p].1+delta*(bind_pos-pp))
   }else{(c*animation*c.transpose()*bind,c*origin)};
   globals[i]=(rotation,position);
  }
  // Jimmy's legs are shorter than the source rig. Keep the original physical
  // foot targets and solve his two native limb lengths, so soles remain on the
  // deck instead of inheriting the source character's proportions.
  let legs=[(3usize,4usize,5usize),(6,7,8)];let mut lower=0f32;
  for &(hip,knee,foot) in &legs {
   let target=c*(animation*parts(&pose[self.mapping[foot].unwrap()]).1+origin);
   let l1=native(&self.bones[hip]).1.distance(native(&self.bones[knee]).1);
   let l2=native(&self.bones[knee]).1.distance(native(&self.bones[foot]).1);
   let offset=globals[hip].1-target;let horizontal=offset.x*offset.x+offset.y*offset.y;
   let reach=(l1+l2)*0.995;
   if horizontal<reach*reach {lower=lower.max(offset.z-(reach*reach-horizontal).sqrt());}
  }
  lower=lower.clamp(0.,0.25);
  for entry in &mut globals[1..35]{entry.1.z-=lower;}
  for &(hip,knee,foot) in &legs {
   let target=c*(animation*parts(&pose[self.mapping[foot].unwrap()]).1+origin);
   let anchor=globals[hip].1;let delta=target-anchor;let distance=delta.length();
   if distance<0.001{continue}
   let axis=delta/distance;
   let l1=native(&self.bones[hip]).1.distance(native(&self.bones[knee]).1);
   let l2=native(&self.bones[knee]).1.distance(native(&self.bones[foot]).1);
   let d=distance.clamp((l1-l2).abs()+0.0001,l1+l2-0.0001);
   let projection=(l1*l1-l2*l2+d*d)/(2.*d);
   let height=(l1*l1-projection*projection).max(0.).sqrt();
   let source_knee=c*(animation*parts(&pose[self.mapping[knee].unwrap()]).1+origin);
   let hint=source_knee-anchor;let bend=(hint-axis*hint.dot(axis)).normalize_or_zero();
   let bend=if bend.length_squared()<0.5{axis.cross(Vec3::X).normalize_or_zero()}else{bend};
   let next_knee=anchor+axis*projection+bend*height;let next_foot=anchor+axis*d;
   globals[knee].1=next_knee;globals[foot].1=next_foot;
  }
  // Solve a native rim grip in deck space. Jimmy's palm sits outside the
  // edge, his thumb meets the upper face and his fingers wrap underneath.
  // These offsets and joint angles were fitted to his original skinned hand,
  // rather than assuming the wrist or an arbitrary palm point is the contact.
  let holding=skater.player_input.physical.off_board.flag_311!=0;
  let selected=skater.board_possession.state.selected_hand_424;
  let grip=if holding {match selected {0=>Some((20,21,22)),1=>Some((28,29,30)),_=>None}}else{None};
  if let Some((upper,elbow,hand))=grip {
   let source_wrist=c*(animation*parts(&pose[self.mapping[hand].unwrap()]).1+origin);
   let rotation=Mat3::from_cols_slice(&board[0][..9]).transpose();
   let board_origin=Vec3::from_slice(&board[0][9..12]);
   let local=rotation.inverse()*(source_wrist-board_origin);
   let side=if local.x<0.{-1.}else{1.};
   let across=rotation.x_axis.normalize()*side;let normal=rotation.z_axis.normalize();
   let wrist_rotation=Mat3::from_cols(-normal,across,(-normal).cross(across))*Mat3::from_rotation_z(0.214719);
   let contact=board_origin+rotation*Vec3::new(side*0.1475,local.y.clamp(-0.38,0.38),0.130224);
   let target=contact+across*0.022148+normal*0.082912;
   globals[hand].0=wrist_rotation;
   let source_elbow=c*(animation*parts(&pose[self.mapping[elbow].unwrap()]).1+origin);
   let l1=native(&self.bones[upper]).1.distance(native(&self.bones[elbow]).1);
   let l2=native(&self.bones[elbow]).1.distance(native(&self.bones[hand]).1);
   // A swing can exceed the two arm segments' reach. Rotate the native
   // clavicle toward the grip before solving the elbow, keeping all three
   // segment lengths instead of stretching the arm or dropping the board.
   let clavicle=upper-1;let pivot=globals[clavicle].1;
   let shoulder_length=native(&self.bones[clavicle]).1.distance(native(&self.bones[upper]).1);
   let reach=l1+l2-0.001;let to_target=target-pivot;let distance=to_target.length();
   if globals[upper].1.distance(target)>reach&&distance>0.001&&distance<shoulder_length+reach {
    let axis=to_target/distance;
    let projection=((shoulder_length*shoulder_length+distance*distance-reach*reach)/(2.*distance)).clamp(-shoulder_length,shoulder_length);
    let hint=globals[upper].1-pivot;let bend=(hint-axis*hint.dot(axis)).normalize_or_zero();
    let bend=if bend.length_squared()>0.5{bend}else{axis.cross(Vec3::Z).normalize_or_zero()};
    globals[upper].1=pivot+axis*projection+bend*(shoulder_length*shoulder_length-projection*projection).max(0.).sqrt();
   }
   let anchor=globals[upper].1;let delta=target-anchor;let axis=delta.normalize_or_zero();
   if axis.length_squared()>0.5 {
    let d=delta.length().clamp((l1-l2).abs()+0.0001,l1+l2-0.0001);
    let projection=(l1*l1-l2*l2+d*d)/(2.*d);
    let height=(l1*l1-projection*projection).max(0.).sqrt();
    let hint=source_elbow-anchor;let bend=(hint-axis*hint.dot(axis)).normalize_or_zero();
    let bend=if bend.length_squared()>0.5{bend}else{axis.cross(Vec3::Z).normalize_or_zero()};
    globals[elbow].1=anchor+axis*projection+bend*height;
    globals[hand].1=anchor+axis*d;
   }
  }
  if let Some(car)=hand_target{
   let mut target=Vec3::from_array(car.center);let (upper,elbow,hand)=(28,29,30);
   let facing=(target-globals[2].1)*Vec3::new(1.,1.,0.);
   let axis=Vec3::Z.cross(facing.normalize_or_zero());
   if axis.length_squared()>0.5{
    let lean=Mat3::from_quat(Quat::from_axis_angle(axis,0.62));let pivot=globals[2].1;
    for entry in &mut globals[9..35]{entry.1=pivot+lean*(entry.1-pivot);entry.0=lean*entry.0;}
   }
   // Choose a reachable point on the rear panel, rather than requiring Jimmy's
   // shorter arm to cross his chest to the car's exact centre. It remains on
   // the same physical bumper plane and within the original model bounds.
   let right=Vec3::from_array(car.right);
   target+=right*(globals[upper].1-target).dot(right).clamp(-car.half_width,car.half_width);
   let up=Vec3::from_array(car.up);
   target+=up*(globals[upper].1-target).dot(up).clamp(car.min_up,car.max_up.max(car.min_up));
   let anchor=globals[upper].1;let delta=target-anchor;let axis=delta.normalize_or_zero();
   let l1=native(&self.bones[upper]).1.distance(native(&self.bones[elbow]).1);
   let l2=native(&self.bones[elbow]).1.distance(native(&self.bones[hand]).1);
   if axis.length_squared()>0.5{
    let d=delta.length().clamp((l1-l2).abs()+0.0001,l1+l2-0.0001);
    let projection=(l1*l1-l2*l2+d*d)/(2.*d);let height=(l1*l1-projection*projection).max(0.).sqrt();
    let hint=globals[elbow].1-anchor;let bend=(hint-axis*hint.dot(axis)).normalize_or_zero();
    let bend=if bend.length_squared()>0.5{bend}else{axis.cross(Vec3::Z).normalize_or_zero()};
    globals[elbow].1=anchor+axis*projection+bend*height;
    let old=globals[hand].1;globals[hand].1=anchor+axis*d;let shift=globals[hand].1-old;
    for i in 31..34{globals[i].1+=shift;}
   }
  }
  // The two reference rigs use different relaxed arm/leg angles. Preserve
  // source twist, then swing each skinned limb onto its actual native child
  // position. This also applies the IK knee correction to the rendered skin.
  for (joint,child) in [(3,4),(4,5),(6,7),(7,8),(10,11),(11,12),(12,13),
                        (13,14),(19,20),(20,21),(21,22),(27,28),(28,29),(29,30)] {
   let (bind,anchor)=native(&self.bones[joint]);
   let predicted=(globals[joint].0*bind.inverse()*(native(&self.bones[child]).1-anchor)).normalize_or_zero();
   let desired=(globals[child].1-globals[joint].1).normalize_or_zero();
   if predicted.length_squared()>0.5 && desired.length_squared()>0.5 {
    globals[joint].0=Mat3::from_quat(Quat::from_rotation_arc(predicted,desired))*globals[joint].0;
   }
  }
  // Recompose unanimated native fingers and shoulder helpers after IK and
  // limb rotations. Otherwise skin weights still follow their old pose.
  for i in 1..35 {
   if self.mapping[i].is_some(){continue}
   let parent=self.bones[i].parent;
   if parent<0{continue}
   let p=parent as usize;let (bind,bind_pos)=native(&self.bones[i]);
   let (parent_bind,parent_pos)=native(&self.bones[p]);
   let delta=globals[p].0*parent_bind.inverse();
   globals[i]=(delta*bind,globals[p].1+delta*(bind_pos-parent_pos));
   if let Some((_,_,hand))=grip {
    if i==hand+1 {
     // Bring the thumb onto the deck while keeping the two sides mirrored.
     globals[i].0=globals[i].0*Mat3::from_rotation_z(-1.630023)*Mat3::from_rotation_x(if hand==30{-0.88}else{0.88});
    }else if i==hand+2 {
     globals[i].0=globals[i].0*Mat3::from_rotation_z(-1.353924);
    }else if i==hand+3 {
     globals[i].0=globals[i].0*Mat3::from_rotation_z(-1.776923);
    }
   }
  }
  // Unlike Skate NativeMatrix, Bully's compact HAnim stores ROWS of the
  // column-vector operator. This is validated against native idle limb
  // directions (thigh->calf and upper arm->forearm), not guessed from facing.
  std::array::from_fn(|i|{let(r,p)=globals[i];let mut out=[0.;13];out[..9].copy_from_slice(&r.transpose().to_cols_array());out[9..12].copy_from_slice(&p.to_array());out[12]=1.;out})
 }
}
