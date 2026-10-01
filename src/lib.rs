//! Input normalization for the x86 bridge. The x64 worker owns skating.
fn finite(value:f32)->f32 {if value.is_finite(){value}else{0.}}
pub fn deadzone(x:f32,y:f32,zone:f32)->(f32,f32) {
 let x=finite(x).clamp(-1.,1.);let y=finite(y).clamp(-1.,1.);
 let length=x.hypot(y);let zone=finite(zone).clamp(0.,0.5);
 if length<=zone{return (0.,0.)}
 let scale=((length-zone)/(1.-zone)).clamp(0.,1.)/length;(x*scale,y*scale)
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn fs_deadzone(x:f32,y:f32,zone:f32,out:*mut f32)->u32 {
 if out.is_null(){return 0}let(x,y)=deadzone(x,y,zone);
 unsafe{*out=x;*out.add(1)=y};1
}
#[cfg(test)] mod tests {
 use super::*;
 #[test] fn input_is_finite_and_radially_bounded(){
  assert_eq!(deadzone(f32::NAN,f32::INFINITY,0.2),(0.,0.));
  assert_eq!(deadzone(0.1,0.1,0.2),(0.,0.));
  let(x,y)=deadzone(1.,1.,0.2);assert!((x.hypot(y)-1.).abs()<1e-6);
 }
}
