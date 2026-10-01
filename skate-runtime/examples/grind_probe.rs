fn main()->Result<(),String>{
 let root=std::path::Path::new(r"prepared/skate-assets");
 let geometry=std::path::Path::new("prepared/bully-assets/world.bmgeo");
 // Actual convex school entrance kerb, extracted native edge 8766.
 let mut host=bully_skate_runtime::SkateHost::bully(root,geometry,[291.,5.971,65.031],std::f32::consts::FRAC_PI_2)?;
 host.seed_velocity([4.,0.,0.]);let mut acquired=false;
 for tick in 0..240 {
  let pad=skate_core::input::xbox::XboxState{buttons:0,triggers:[0;2],left:[0;2],right:[0;2]};host.tick(pad)?;
  acquired|=(400..406).contains(&host.state());
  if tick%15==0{println!("tick={tick} {} deck={:?}",host.status(),host.position());}
 }
 println!("native grind acquired={acquired}");if !acquired{return Err("Native kerb probe did not acquire a grind".into())}Ok(())
}
