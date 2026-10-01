fn main()->Result<(),String>{
 let root=std::path::Path::new(r"prepared/skate-assets");
 let mut host=bully_skate_runtime::SkateHost::flat(root)?;let mut low=1000f32;let mut high=-1000f32;
 for tick in 0..420 {
  let right=if (120..129).contains(&tick){[0,-32767]}else if(129..134).contains(&tick){[0,32767]}else{[0;2]};
  let pad=skate_core::input::xbox::XboxState{buttons:0,triggers:[0;2],left:[0;2],right};host.tick(pad)?;
  if tick>110&&tick<300{low=low.min(host.position()[1]);high=high.max(host.position()[1]);}
  if tick%15==0{println!("tick={tick} {} deck={:?}",host.status(),host.position());}
 }
 println!("flick deck range {low}..{high}");if high-low<0.25{return Err("Right-stick flick did not launch the physical deck".into())}Ok(())
}
