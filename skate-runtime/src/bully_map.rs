//! Bully's compiled static collision, transformed from COL/IPB into core space.
use skate_data::skate_map::{SkateMap,Geometry,Collision,Material,Rail};
pub fn load(path:&std::path::Path,spawn:[f32;3],heading:f32,area:u32)->Result<SkateMap,String>{
 let data=std::fs::read(path).map_err(|e|format!("{}: {e}",path.display()))?;
 if area>127||data.len()<12||&data[..8]!=b"BMGEO2\0\0"{return Err("Invalid Bully collision header or area".into())}
 let count=u32::from_le_bytes(data[8..12].try_into().unwrap()) as usize;
 if count>1_000_000||data.len()!=12+count*44{return Err("Invalid Bully collision length".into())}
 let mut collision=Vec::with_capacity(count);
 for record in data[12..].chunks_exact(44){
  if u16::from_le_bytes(record[38..40].try_into().unwrap()) as u32!=area{continue}
  let points=std::array::from_fn(|p|std::array::from_fn(|axis|f32::from_le_bytes(record[(p*3+axis)*4..(p*3+axis+1)*4].try_into().unwrap())));
  if points.iter().flatten().any(|x|!x.is_finite()){return Err("Non-finite Bully triangle".into())}
  // Native Bully materials do not share Skate's numeric surface namespace.
  // Generic hard-surface contact uses the recovered core's floor material.
  collision.push(Collision{points,surface:0,material:1,native_edges:None});
 }
 if collision.is_empty(){return Err(format!("No skating collision for Bully area {area}"))}
 let rails=load_rails(&path.with_extension("bmrails"),area)?;
 Ok(SkateMap{version:15,name:"Bullworth".into(),spawn,heading,environment:Vec::new(),
  materials:vec![Material{name:"Bully hard surface".into(),flags:0,friction:0.8,restitution:0.0,color:[1.;3],roughness:1.,emissive:0.,textures:[0;5],indirect_strength:0.,alpha_mode:0,alpha_cutoff:0.,audio:0,physics:0,pattern:0,depth_layer:None,retail_definition:None}],
  textures:Vec::new(),geometry:Geometry{vertices:Vec::new(),indices:Vec::new(),collision},rails,doors:Vec::new(),lights:Vec::new(),routes:Vec::new(),extensions:Vec::new()})
}
fn load_rails(path:&std::path::Path,area:u32)->Result<Vec<Rail>,String>{
 let data=std::fs::read(path).map_err(|e|format!("{}: {e}",path.display()))?;
 if data.len()<12||&data[..8]!=b"BMRL2\0\0\0"{return Err("Invalid Bully grind header".into())}
 let count=u32::from_le_bytes(data[8..12].try_into().unwrap()) as usize;
 if count>65535||data.len()!=12+count*28{return Err("Invalid Bully grind length".into())}
 data[12..].chunks_exact(28).enumerate().filter(|(_,r)|u16::from_le_bytes(r[24..26].try_into().unwrap()) as u32==area).map(|(i,record)|{
  let points:Vec<[f32;3]>=record[..24].chunks_exact(12).map(|p|std::array::from_fn(|axis|f32::from_le_bytes(p[axis*4..axis*4+4].try_into().unwrap()))).collect();
  if points.iter().flatten().any(|v|!v.is_finite()){return Err("Non-finite Bully grind edge".into())}
  Ok(Rail{name:format!("Bullworth convex edge {i}"),points,closed:false,native:None})
 }).collect()
}
