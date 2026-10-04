use bully_skate_runtime::SkateHost;
use skate_core::input::xbox::XboxState;
fn input(right:[i16;2])->XboxState{XboxState{buttons:0,left:[0;2],right,triggers:[0;2]}}
fn run()->Result<(),String>{
 let root=std::env::args().nth(1).ok_or("Core assets path required")?;
 let mut host=SkateHost::flat(std::path::Path::new(&root))?;
 for _ in 0..90{host.tick(input([0;2]))?;}
 for mode in [1,2,3,4,0] {
  let before=host.position();host.configure_difficulty(mode)?;
  if host.active_difficulty()!=mode||host.position()!=before{return Err("Mode change replaced active board state".into())}
  for _ in 0..15{host.tick(input([0;2]))?;}
  if host.output().iter().any(|v|!v.is_finite()){return Err("Mode switch returned invalid physics".into())}
  println!("PASS: live difficulty {mode}, {}",host.status());
 }
 if host.configure_difficulty(5).is_ok()||host.active_difficulty()!=0{return Err("Invalid difficulty modified host".into())}
 // The hybrid must keep the same Easy air/ollie selectors rather than inherit
 // the lower jump profile from standard Motorized. Use the same flat course.
 let mut peaks=[0f32;2];
 for (i,mode) in [0,4].into_iter().enumerate(){
  host.configure_difficulty(mode)?;host.relocate([0.,0.,0.],0.)?;
  for _ in 0..90{host.tick(input([0;2]))?;}
  let base=host.output()[2];
  for _ in 0..9{host.tick(input([0,-32767]))?;}
  for _ in 0..5{host.tick(input([0,32767]))?;peaks[i]=peaks[i].max(host.output()[2]-base);}
  for _ in 0..120{host.tick(input([0;2]))?;peaks[i]=peaks[i].max(host.output()[2]-base);}
 }
 if peaks[0]<0.3||(peaks[0]-peaks[1]).abs()>0.015{return Err(format!("Hybrid changed Easy ollie height: {peaks:?}"))}
 println!("PASS: Easy and Easy + Motorized ollie heights {peaks:?}");Ok(())
}
fn main()->Result<(),String>{std::thread::Builder::new().stack_size(32*1024*1024).spawn(run).map_err(|e|e.to_string())?.join().map_err(|_|"Probe panicked".to_string())?}
