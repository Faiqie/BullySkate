fn main()->Result<(),String>{
 let root=std::path::Path::new(r"prepared/skate-assets");
 let map=std::path::Path::new("prepared/bully-assets/world.bmgeo");
 let mut h=bully_skate_runtime::SkateHost::bully(root,map,[298.36,5.8026,72.23],std::f32::consts::PI)?;
 for i in 0..180{
  h.tick(skate_core::input::xbox::XboxState{buttons:0,left:[0;2],right:[0;2],triggers:[0;2]})?;
  if i%60==0{println!("tick {i} deck={:?} root={:?} camera={:?}",h.output(),h.rider_root(),h.camera(16./9.));}
 }
 Ok(())
}
