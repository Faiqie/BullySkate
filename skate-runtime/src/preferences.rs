//! Validated host preferences, independent of the source physics constants.
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct SkaterPreferences {
 pub(crate) stance:u32,pub(crate) trucks:f32,pub(crate) wheels:f32,
 pub(crate) style:u32,pub(crate) posture:u32,pub(crate) gestures:[u32;4],pub(crate) fov:f32,
}
impl Default for SkaterPreferences {
 fn default()->Self{Self{stance:1,trucks:0.7,wheels:0.7,style:0,posture:0,gestures:[0,1,2,3],fov:67.}}
}
impl SkaterPreferences {
 pub fn from_wire(v:[f32;10])->Result<Self,String>{
  if v.iter().any(|x|!x.is_finite())||!(0. ..=1.).contains(&v[0])||v[0].fract()!=0.||
   !(0. ..=1.).contains(&v[1])||!(0. ..=1.).contains(&v[2])||
   v[3..9].iter().enumerate().any(|(i,x)|x.fract()!=0.||*x<0.||*x>if i<2{3.}else{36.})||
   !(40. ..=110.).contains(&v[9]){return Err("Invalid skater preferences".into())}
  Ok(Self{stance:v[0] as u32,trucks:v[1],wheels:v[2],style:v[3] as u32,posture:v[4] as u32,
   gestures:std::array::from_fn(|i|v[5+i] as u32),fov:v[9]})
 }
 pub fn to_wire(self)->[f32;10]{[self.stance as f32,self.trucks,self.wheels,self.style as f32,
  self.posture as f32,self.gestures[0] as f32,self.gestures[1] as f32,self.gestures[2] as f32,self.gestures[3] as f32,self.fov]}
}
