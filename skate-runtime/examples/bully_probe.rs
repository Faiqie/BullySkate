fn main()->Result<(),String>{
 use bevy::math::{Mat3,Vec3};
 let selected=std::env::args().nth(1).unwrap_or_else(||r"prepared/skate-assets".into());
 let root=std::path::Path::new(&selected);
 let map=std::path::Path::new("prepared/bully-assets/world.bmgeo");
 let mut host=bully_skate_runtime::SkateHost::bully(root,map,[298.36,5.8026,72.23],std::f32::consts::PI)?;
 if std::env::args().any(|v|v=="--mounted") {host.relocate([298.36,5.8026,72.23],std::f32::consts::PI)?;host.actors(&[]);}
 let bind:Vec<serde_json::Value>=serde_json::from_slice(&std::fs::read(map.with_file_name("jimmy-bind.json")).map_err(|e|e.to_string())?).map_err(|e|e.to_string())?;
 let bind_pos=|i:usize|Vec3::from_array(std::array::from_fn(|j|bind[i]["matrix"][3][j].as_f64().unwrap() as f32));
 let bind_rot=|i:usize|Mat3::from_cols_array_2d(&std::array::from_fn(|j|std::array::from_fn(|k|bind[i]["matrix"][j][k].as_f64().unwrap() as f32)));
 let mut minimum_dot=1f32;let mut maximum_length_error=0f32;let mut airborne=false;let mut landed=false;let mut base=0f32;let mut peak=0f32;
 let start=std::time::Instant::now();
 for tick in 0..600 {
  // Test the flick on clear pavement before the push sequence approaches a curb.
  let flick=if (80..89).contains(&tick){[0,-32767]}else if(89..94).contains(&tick){[0,32767]}else{[0;2]};
  let pad=skate_core::input::xbox::XboxState{buttons:if (180..360).contains(&tick){4096}else{0},triggers:[0;2],left:[0;2],right:flick};
  host.tick(pad).map_err(|e|format!("tick {tick}: {e}"))?;
  let p=host.position();if p.iter().any(|v|!v.is_finite())||p[1]<4.0{return Err(format!("World support failed at tick {tick}: {p:?}"))}
  let pose=host.native_pose().ok_or("Retarget pose missing")?;
  if pose.iter().flatten().any(|v|!v.is_finite()){return Err(format!("Invalid native pose at {tick}"))}
  let position=|i:usize|Vec3::from_slice(&pose[i][9..12]);
  let rotation=|i:usize|Mat3::from_cols_slice(&pose[i][..9]).transpose();
  for (joint,child) in [(3,4),(4,5),(6,7),(7,8),(10,11),(11,12),(12,13),(13,14),(19,20),(20,21),(21,22),(27,28),(28,29),(29,30)] {
   let offset=bind_pos(child)-bind_pos(joint);
   let predicted=rotation(joint)*bind_rot(joint).inverse()*offset;
   let actual=position(child)-position(joint);
   let dot=predicted.normalize().dot(actual.normalize());let error=(actual.length()-offset.length()).abs();
   if dot<0.9995||error>0.00015{return Err(format!("Native limb mismatch tick {tick} joint {joint}: dot={dot} length error={error}"))}
   minimum_dot=minimum_dot.min(dot);maximum_length_error=maximum_length_error.max(error);
  }
  for i in 0..36 {if rotation(i).determinant()<0.9{return Err(format!("Reflected/singular native skin tick {tick} bone {i}"))}}
  let board=host.native_board_pose();let r=Mat3::from_cols_slice(&board[0][..9]).transpose();let output=host.output();
  let(yaw,pitch)=(output[3],output[4]);let expected=Vec3::new(-yaw.sin()*pitch.cos(),yaw.cos()*pitch.cos(),pitch.sin());
  if r.y_axis.normalize().dot(expected)<0.99999{return Err(format!("Board heading mismatch tick {tick}"))}
  if tick==70{base=p[1];}
  if tick>80&&tick<180{airborne|=output[13]<0.5;landed|=airborne&&output[13]>0.5;peak=peak.max(p[1]);}
  if tick==60 {std::fs::write("work/jimmy-retarget-pose.json",serde_json::to_vec_pretty(&serde_json::json!({"names":host.native_bone_names(),"pose":pose.as_slice(),"sourceNames":host.bone_names(),"sourceWorld":host.world_pose(),"board":host.native_board_pose()})).unwrap()).map_err(|e|e.to_string())?;}
  if tick==181||tick==421 {std::fs::write(format!("work/jimmy-pose-{tick}.json"),serde_json::to_vec_pretty(&serde_json::json!({"names":host.native_bone_names(),"pose":pose.as_slice(),"board":host.native_board_pose()})).unwrap()).map_err(|e|e.to_string())?;}
  if tick%60==0{println!("{} position={:?}",host.status(),host.position());}
 }
 if !airborne||!landed||peak-base<0.25{return Err(format!("Rig probe failed ollie/landing: airborne={airborne}, landed={landed}, deck rise={}",peak-base))}
 println!("600 Bullworth gameplay ticks in {:?}; rig minimum direction dot {minimum_dot}, maximum length error {maximum_length_error} m; airborne={airborne} landed={landed} deck rise={}m",start.elapsed(),peak-base);Ok(())
}
