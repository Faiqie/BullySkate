fn main()->Result<(),String>{
 let root=std::path::Path::new(r"prepared/skate-assets");
 let mut host=bully_skate_runtime::SkateHost::flat(root)?;
 let start=std::time::Instant::now();
 for tick in 0..600 {
  let pad=skate_core::input::xbox::XboxState{buttons:if (120..420).contains(&tick){4096}else{0},triggers:[0;2],left:[0;2],right:[0;2]};
  host.tick(pad).map_err(|e|format!("tick {tick}: {e}"))?;
  if tick%60==0{println!("{} position={:?}",host.status(),host.position());}
  if tick==60{std::fs::write("work/skate-retarget-pose.json",serde_json::to_vec_pretty(&serde_json::json!({"names":host.bone_names(),"pose":host.pose()})).unwrap()).map_err(|e|e.to_string())?;}
 }
 println!("600 complete gameplay ticks in {:?}",start.elapsed());Ok(())
}
