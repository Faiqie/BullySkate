use bully_skate_runtime::SkateHost;
use skate_core::input::xbox::XboxState;
use bevy::math::{Mat3,Vec3};
fn pad(buttons:u16,left:[i16;2])->XboxState{XboxState{buttons,left,right:[0;2],triggers:[0;2]}}
fn run()->Result<(),String>{
 let root=std::env::args().nth(1).ok_or("Core assets path required")?;
 let mut host=SkateHost::bully_visible(std::path::Path::new(&root),std::path::Path::new("prepared/bully-assets/world.bmgeo"),[298.36,5.8026,72.23],std::f32::consts::PI,5)?;
 host.relocate([298.36,5.8026,72.23],std::f32::consts::PI)?;
 for _ in 0..90{host.tick(pad(0,[0;2]))?;}
 for i in 0..140{host.tick(pad(if i<3{0x8000}else{0},[0;2]))?;}
 let capture=|host:&SkateHost,label:&str|->Result<(),String>{
  let native=host.native_pose().ok_or("Missing native skin")?;let source=host.world_pose();let names=host.bone_names();let board=host.native_board_pose();
  let snapshot=serde_json::json!({"names":host.native_bone_names(),"pose":native.as_slice(),"board":board,"sourceNames":names,"sourceWorld":source,"status":host.status(),"heldHand":host.held_board_hand()});
  std::fs::write(format!("work/held-rig-{label}.json"),serde_json::to_vec_pretty(&snapshot).unwrap()).map_err(|e|e.to_string())?;
  let bind:Vec<serde_json::Value>=serde_json::from_slice(&std::fs::read("prepared/bully-assets/jimmy-bind.json").map_err(|e|e.to_string())?).unwrap();
  for (upper,elbow,hand,name) in [(20,21,22,"LEFTHAND"),(28,29,30,"RIGHTHAND")]{
   let source_index=names.iter().position(|n|n.eq_ignore_ascii_case(name)).unwrap();let p=source[source_index][3];let target=Vec3::new(p[0],-p[2],p[1]);let wrist=Vec3::from_slice(&native[hand][9..12]);
   let bind_p=|i:usize|Vec3::from_array(std::array::from_fn(|j|bind[i]["matrix"][3][j].as_f64().unwrap() as f32));
   let length=bind_p(upper).distance(bind_p(elbow))+bind_p(elbow).distance(bind_p(hand));
   println!("{label} {name}: wrist gap {:.4}m, target reach {:.4}m / limb {:.4}m",wrist.distance(target),Vec3::from_slice(&native[upper][9..12]).distance(target),length);
   if host.held_board_hand()==Some(if hand==22{0}else{1}) {
    let r=Mat3::from_cols_slice(&native[hand][..9]).transpose();
    let br=Mat3::from_cols_slice(&board[0][..9]).transpose();
    let local=br.inverse()*(wrist-Vec3::from_slice(&board[0][9..12]));
    let gap=((local.x.abs()-0.1475)*0.95-0.022148).hypot((local.z-0.130224)*0.62-0.082912);
    let across=br.x_axis.normalize()*local.x.signum();let normal=br.z_axis.normalize();
    let expected=Mat3::from_cols(-normal,across,(-normal).cross(across))*Mat3::from_rotation_z(0.214719);
    println!("{label}: deck-relative wrist error {gap:.5}m; thumb and finger skin verified separately");
    if gap>0.002||(r-expected).to_cols_array().iter().any(|v|v.abs()>0.002){return Err(format!("Held-board rim grip missed its fitted transform in {label}: {gap}"))}
   }
   for (a,b) in [(upper-1,upper),(upper,elbow),(elbow,hand)] {
    let actual=Vec3::from_slice(&native[a][9..12]).distance(Vec3::from_slice(&native[b][9..12]));
    if (actual-bind_p(a).distance(bind_p(b))).abs()>0.00015{return Err(format!("Native arm stretched in {label}"))}
   }
  }
  for (i,m) in native.iter().enumerate() {if m.iter().any(|v|!v.is_finite())||Mat3::from_cols_slice(&m[..9]).determinant()<0.9{return Err(format!("Invalid native skin {label} bone {i}"))}}
  let snapshot=serde_json::json!({"names":host.native_bone_names(),"pose":native.as_slice(),"board":board,"sourceNames":names,"sourceWorld":source,"status":host.status(),"heldHand":host.held_board_hand()});
  std::fs::write(format!("work/held-rig-{label}.json"),serde_json::to_vec_pretty(&snapshot).unwrap()).map_err(|e|e.to_string())?;
  println!("{label}: {}",host.status());Ok(())
 };
 capture(&host,"idle")?;
 for _ in 0..45{host.tick(pad(0,[0,16000]))?;}capture(&host,"walk")?;
 for _ in 0..60{host.tick(pad(0,[0;2]))?;}
 let root=host.rider_root();let world=host.world_pose();let forward=world[0][2];
 host.actors_sized(&[[123.,root[0]+forward[0]*0.95,root[1]-forward[2]*0.95,root[2],0.,0.,0.,1.56,0.28]]);
 let mut swung=false;
 for i in 0..60{host.tick(pad(if i<20{512}else{0},[0;2]))?;
  if i>=6&&host.status().contains("shove=true"){capture(&host,"swing")?;swung=true;}
 }
 if !swung{return Err("Probe never entered the actual board-swing animation".into())}
 host.actors_sized(&[]);
 for _ in 0..120{host.tick(pad(0,[0;2]))?;}capture(&host,"released")?;
 Ok(())
}
fn main()->Result<(),String>{std::thread::Builder::new().stack_size(32*1024*1024).spawn(run).map_err(|e|e.to_string())?.join().map_err(|_|"Probe panicked".to_string())?}
