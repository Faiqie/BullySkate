//! Bully's material IDs mapped to Skate's authored audio surface tags.
use std::{collections::HashMap,path::Path};
pub fn tag(material:u16)->u32{
 match material {
  1=>2, // Tarmac: rough asphalt.
  2|4|5|27|54=>8, // Loose dirt, gravel, mud and sand.
  3|43=>67, // Grass and vegetation: soft ground layer.
  6|7|9|10|28|29|41|42|55|63=>3, // Paving, smooth concrete, tile, ice, snow and lino.
  0|8|11|12|13|45|52|64|65=>4, // Concrete, masonry and stairs.
  14|17..=25|46..=48|53|56=>9, // Metal fixtures, vehicle bodies, stairs and gates.
  32..=37|44|59|62=>6, // Wood, benches, dock, boards and branches.
  16|26|38..=40|49..=51|57|58|60=>68, // Cloth, rubber, plastic, paper and carpet: soft surface.
  15|66=>37, // Glass / mirror.
  30|31|61=>69, // Water and puddles.
  _=>3,
 }
}
#[derive(Clone,Copy)]struct Triangle {p:[[f32;3];3],material:u16}
pub struct Map {triangles:Vec<Triangle>,cells:HashMap<(u16,i32,i32),Vec<usize>>}
impl Map {
 pub fn load(path:&Path)->Result<Self,String>{
  let data=std::fs::read(path).map_err(|e|e.to_string())?;
  Self::parse(&data)
 }
 pub(crate) fn parse(data:&[u8])->Result<Self,String>{
  if data.len()<12||&data[..8]!=b"BMGEO2\0\0"{return Err("Invalid audio surface map".into())}
  let count=u32::from_le_bytes(data[8..12].try_into().unwrap()) as usize;
  if count>1_000_000||data.len()!=12+count*44{return Err("Invalid audio surface map length".into())}
  let mut map=Self{triangles:Vec::new(),cells:HashMap::new()};
  for r in data[12..].chunks_exact(44){
   let p:[[f32;3];3]=std::array::from_fn(|v|std::array::from_fn(|a|f32::from_le_bytes(r[(v*3+a)*4..(v*3+a+1)*4].try_into().unwrap())));
   if p.iter().flatten().any(|v|!v.is_finite()){return Err("Non-finite audio surface map".into())}
   let area=u16::from_le_bytes(r[38..40].try_into().unwrap());let material=u16::from_le_bytes(r[36..38].try_into().unwrap());
   let x=p.map(|p|p[0]);let z=p.map(|p|p[2]);let low=[x.into_iter().fold(f32::INFINITY,f32::min),z.into_iter().fold(f32::INFINITY,f32::min)];let high=[x.into_iter().fold(f32::NEG_INFINITY,f32::max),z.into_iter().fold(f32::NEG_INFINITY,f32::max)];
   let first=low.map(|v|(v/10.).floor() as i32);let last=high.map(|v|(v/10.).floor() as i32);
   let dx=i64::from(last[0])-i64::from(first[0])+1;let dz=i64::from(last[1])-i64::from(first[1])+1;
   if dx>1000||dz>1000||dx*dz>4096{return Err("Audio surface bounds too large".into())}
   let id=map.triangles.len();map.triangles.push(Triangle{p,material});
   for x in first[0]..=last[0]{for z in first[1]..=last[1]{map.cells.entry((area,x,z)).or_default().push(id);}}
  }
  Ok(map)
 }
 pub fn material(&self,area:u16,point:[f32;3])->Option<u32>{
  let ids=self.cells.get(&(area,(point[0]/10.).floor() as i32,(point[2]/10.).floor() as i32))?;let mut best=None;let mut highest=point[1]-1.2;
  for id in ids{let t=self.triangles[*id];let[a,b,c]=t.p;
   let denominator=(b[2]-c[2])*(a[0]-c[0])+(c[0]-b[0])*(a[2]-c[2]);if denominator.abs()<1e-7{continue}
   let u=((b[2]-c[2])*(point[0]-c[0])+(c[0]-b[0])*(point[2]-c[2]))/denominator;
   let v=((c[2]-a[2])*(point[0]-c[0])+(a[0]-c[0])*(point[2]-c[2]))/denominator;
   if u< -0.001||v< -0.001||u+v>1.001{continue}
   let y=u*a[1]+v*b[1]+(1.-u-v)*c[1];
   if y>=highest&&y<=point[1]+0.25{highest=y;best=Some(tag(t.material)-1);}
  }
  best
 }
}
#[cfg(test)]mod tests{
 use super::*;
 #[test]fn major_surfaces_are_distinct(){assert_eq!(tag(1),2);assert_eq!(tag(7),3);assert_eq!(tag(32),6);assert_eq!(tag(56),9);assert_ne!(tag(3),tag(7));for id in 0..67{assert!((1..=94).contains(&tag(id)));}}
 fn record(out:&mut Vec<u8>,y:f32,material:u16,area:u16){for p in [[0.,y,0.],[5.,y,0.],[0.,y,5.]]{for f in p{out.extend(f.to_le_bytes());}}out.extend(material.to_le_bytes());out.extend(area.to_le_bytes());out.extend(0u32.to_le_bytes());}
 #[test]fn stacked_surfaces_choose_the_nearest_floor_in_the_correct_area(){
  let mut data=b"BMGEO2\0\0".to_vec();data.extend(3u32.to_le_bytes());record(&mut data,0.,1,0);record(&mut data,2.,32,0);record(&mut data,2.,56,1);
  let map=Map::parse(&data).unwrap();assert_eq!(map.material(0,[1.,2.1,1.]),Some(5));assert_eq!(map.material(1,[1.,2.1,1.]),Some(8));assert_eq!(map.material(0,[1.,0.1,1.]),Some(1));assert_eq!(map.material(0,[9.,0.,9.]),None);
 }
 #[test]fn damaged_maps_are_refused(){assert!(Map::parse(b"BMGEO2\0\0").is_err());let mut data=b"BMGEO2\0\0".to_vec();data.extend(1u32.to_le_bytes());record(&mut data,f32::NAN,1,0);assert!(Map::parse(&data).is_err());data.pop();assert!(Map::parse(&data).is_err());}
}
