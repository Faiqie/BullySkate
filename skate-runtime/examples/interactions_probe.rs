use bully_skate_runtime::SkateHost;
use skate_core::input::xbox::XboxState;
fn pad(buttons:u16)->XboxState{XboxState{buttons,left:[0;2],right:[0;2],triggers:[0;2]}}
fn run()->Result<(),String>{
 let assets=std::env::args().nth(1).unwrap_or_else(||r"prepared/skate-assets".into());
 let root=std::path::Path::new(&assets);
 let mut host=SkateHost::flat(root)?;
 for _ in 0..90{host.tick(pad(0))?;}
 println!("Settled {} {:?}",host.status(),host.position());
 for i in 0..120{host.tick(pad(if i<3{0x8000}else{0}))?;if i%30==0{println!("Dismount {}",host.status());}}
 if !(500..600).contains(&host.state()){return Err("Y did not enter the source off-board state".into())}
 let native=host.rider_root();
 // Flat host faces +Z in core, corresponding to native -Y.
 let actor=[123.,native[0],native[1]-1.0,native[2],0.,0.,0.,1.56,0.28];
 let mut shove_seen=false;let before=host.interaction_status()[2];
 for i in 0..75{
  if i%3==0{host.actors(&[actor]);}
  host.tick(pad(if i<45{0x200}else{0}))?;
  shove_seen|=host.status().contains("shove=true");
  if i%10==0{println!("Swing {i} {} board {:?} npc {:?}",host.status(),host.position(),actor);}
 }
 if !shove_seen{return Err("The source Shove channel never started".into())}
 if host.interaction_status()[2]!=before+1{return Err(format!("Board strike must produce exactly one contact event: {:?}",host.interaction_status()))}
 println!("PASS source Y dismount, held board, source Shove animation and one swept board/NPC impact while RB held");
 host.actors(&[]);host.suspend_interactions();
 let mut tow=SkateHost::flat(root)?;
 for _ in 0..90{tow.tick(pad(0))?;}
 let start=tow.position();let mut acquired=false;let mut detached=false;
 // Flat source heads +Z; native traffic therefore travels -Y. Place its real
 // box ahead of the board, leaving space for the full deck behind the bumper.
 let mut vehicle=[77.,start[0],-start[2]-2.1,start[1]+0.7,0.,-1.,0.,0.,-1.5,0.,0.75,1.5,0.7,0.,0.,0.];
 for i in 0..120{
  vehicle[2]-=1.5*tow.period();if i%3==0{tow.vehicles(&[vehicle]);}
  tow.tick(pad(0x200))?;
  acquired|=tow.interaction_status()[1]==77;
  if i%30==0{println!("Tow {i} {} board {:?}",tow.status(),tow.position());}
 }
 let travelled=tow.position()[2]-start[2];
 if !acquired||travelled<1.0{return Err(format!("Tow failed acquired={acquired} travelled={travelled}"))}
 let velocity_before=tow.output()[7];tow.tick(pad(0))?;
 if tow.interaction_status()[1]!=u32::MAX{return Err("RB release did not detach".into())}
 if (tow.output()[7]-velocity_before).abs()>2.0{return Err("Detachment discarded tow momentum".into())}
 for _ in 0..30{tow.tick(pad(0x200))?;detached|=tow.interaction_status()[1]==u32::MAX;}
 if !detached{return Err("A stale/unloaded car did not detach".into())}
 println!("PASS moving vehicle acquisition, bounded towing ({travelled:.3}m), RB release with momentum, stale-car detach");
 for (height,radius,side) in [(0.95,0.20,0.),(1.3,0.24,0.2),(1.9,0.32,-0.2),(2.3,0.4,0.)]{
  let mut hit=SkateHost::flat(root)?;
  for _ in 0..90{hit.tick(pad(0))?;}
  for i in 0..120{hit.tick(pad(if i<3{0x8000}else{0}))?;}
  let p=hit.rider_root();let target=[124.,p[0]+side,p[1]-0.85,p[2],0.,0.,0.,height,radius];
  for i in 0..75{if i%3==0{hit.actors(&[target]);}hit.tick(pad(if i<45{0x200}else{0}))?;}
  if hit.interaction_status()[2]!=1{return Err(format!("Strike missed height={height} radius={radius} side={side}: {}",hit.status()))}
  println!("PASS swept strike height={height} radius={radius} side={side}");
 }
 let mut motor=SkateHost::flat(root)?;let mut preferences=motor_preferences();
 motor.configure(preferences);
 for _ in 0..90{motor.tick(pad(0x1000))?;}
 if !motor.status().contains("mode=3 "){return Err("Motorized physics selector was not published".into())}
 let before=motor.position();preferences=Default::default();motor.configure(preferences);
 motor.tick(pad(0))?;
 if !motor.status().contains("mode=0 ")||motor.position().iter().zip(before).any(|(a,b)|(a-b).abs()>1.){return Err("Motorized toggle reset or failed to restore the active simulation".into())}
 println!("PASS live native Motorized profile, regular profile restore and preserved physical position");
 let geometry=std::env::args().nth(2).unwrap_or_else(||"prepared/bully-assets/world.bmgeo".into());
 let raw=std::fs::read(geometry).map_err(|e|e.to_string())?;
 let exterior=raw[12..].chunks_exact(44).filter(|r|u16::from_le_bytes(r[38..40].try_into().unwrap())==0).count();
 if exterior==0||exterior>=289152{return Err(format!("Navigation volumes were not excluded: exterior triangle count {exterior}"))}
 println!("PASS BMGEO2 area IDs: {exterior} exterior triangles; interior geometry excluded");
 Ok(())
}
fn motor_preferences()->bully_skate_runtime::SkaterPreferences{
 let mut wire=bully_skate_runtime::SkaterPreferences::default().to_wire();wire[10]=1.;
 bully_skate_runtime::SkaterPreferences::from_wire(wire).unwrap()
}
fn main()->Result<(),String>{
 std::thread::Builder::new().stack_size(32*1024*1024).spawn(run).map_err(|e|e.to_string())?.join().map_err(|_|"Interaction probe panicked".to_string())?
}
