fn main()->Result<(),String>{
 let root=std::path::Path::new(r"prepared/skate-assets");
 let map=std::path::Path::new("prepared/bully-assets/world.bmgeo");
 let mut host=bully_skate_runtime::SkateHost::bully(root,map,[298.36,5.8026,72.23],std::f32::consts::PI)?;
 let pad=||skate_core::input::xbox::XboxState{buttons:0,left:[0;2],right:[0;2],triggers:[0;2]};
 for _ in 0..60{host.tick(pad())?;}
 host.actors(&[[42.,298.36,-70.8,5.803,0.,0.,0.]]);
 host.seed_velocity([0.,0.,-4.]);let mut contacts=0;
 for tick in 0..300{
  host.tick(pad()).map_err(|e|format!("actor tick {tick}: {e}"))?;
  contacts=contacts.max(host.actor_contacts());
  assert!(host.native_pose().unwrap().iter().flatten().all(|v|v.is_finite()));
  if tick%60==0{println!("actor tick {tick} contacts={} {}",host.actor_contacts(),host.status());}
 }
 if contacts==0{return Err("NPC never entered the shared contact solve".into())}
 host.actors(&[]);host.tick(pad())?;
 println!("NPC shared solve passed: {contacts} peak contacts; removal safe");Ok(())
}
