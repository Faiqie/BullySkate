#[path="../src/bully_map.rs"]mod map;
fn rail(area:u16,flags:u16,points:&[[f32;3]])->Vec<u8>{let mut r=Vec::new();r.extend(area.to_le_bytes());r.extend(flags.to_le_bytes());r.extend((points.len() as u32).to_le_bytes());for p in points{for v in p{r.extend(v.to_le_bytes());}}r}
fn main(){
 let masks=map::parse_model_masks("BMMODELS1\n6658 196\n6664 255\n").unwrap();assert_eq!(masks[6658]&(1<<5),0);assert_ne!(masks[6658]&(1<<2),0);
 for invalid in ["", "BMMODELS1\n", "BMMODELS1\n2 256\n", "BMMODELS1\n65536 1\n", "BMMODELS1\n2 1\n2 3\n", "BMMODELS1\n2 3 extra\n"]{assert!(map::parse_model_masks(invalid).is_err());}
 let mut data=b"BMRL3\0\0\0".to_vec();data.extend(2u32.to_le_bytes());data.extend(rail(0,0,&[[0.,1.,0.],[1.,1.,0.],[2.,1.,0.]]));data.extend(rail(1,1,&[[0.,1.,0.],[1.,1.,0.],[1.,1.,1.]]));
 let r=map::parse_rails(&data,0).unwrap();assert_eq!(r.len(),1);assert_eq!(r[0].points.len(),3);assert!(!r[0].closed);assert!(map::parse_rails(&data,1).unwrap()[0].closed);
 for n in 0..data.len(){assert!(map::parse_rails(&data[..n],0).is_err(),"accepted truncated rail at {n}");}
 let mut bad=data.clone();bad.push(0);assert!(map::parse_rails(&bad,0).is_err());
 let mut bad=data.clone();bad[14..16].copy_from_slice(&2u16.to_le_bytes());assert!(map::parse_rails(&bad,0).is_err());
 let mut bad=data.clone();bad[20..24].copy_from_slice(&f32::NAN.to_le_bytes());assert!(map::parse_rails(&bad,0).is_err());
 println!("PASS: continuous/closed area-specific rails; every truncation, invalid flags, NaN and trailing bytes refused.");
 if let Some(path)=std::env::args().nth(1){
  let path=std::path::Path::new(&path);let summer=map::load_visible(path,[334.787,4.634,-212.743],0.,0,5).unwrap();let winter=map::load_visible(path,[334.787,4.634,-212.743],0.,0,2).unwrap();
  let masks=map::parse_model_masks(&std::fs::read_to_string(path.with_file_name("world-models.txt")).unwrap()).unwrap();let data=std::fs::read(path).unwrap();
  let rows:Vec<_>=data[12..].chunks_exact(44).filter(|r|u16::from_le_bytes(r[38..40].try_into().unwrap())==0).collect();
  for (phase,loaded) in [(5,&summer),(2,&winter)]{
   let expected=rows.iter().filter(|r|masks[u32::from_le_bytes(r[40..44].try_into().unwrap()) as usize]&(1<<phase)!=0).count();assert_eq!(loaded.geometry.collision.len(),expected);
  }
  let tree=rows.iter().find(|r|u32::from_le_bytes(r[40..44].try_into().unwrap())==6658).unwrap();let points:[[f32;3];3]=std::array::from_fn(|p|std::array::from_fn(|a|f32::from_le_bytes(tree[(p*3+a)*4..(p*3+a+1)*4].try_into().unwrap())));
  assert!(!summer.geometry.collision.iter().any(|c|c.points==points));assert!(winter.geometry.collision.iter().any(|c|c.points==points));
  assert!(summer.geometry.collision.len()>100000&&summer.rails.len()>4000);
  println!("PASS: native appearance phases load {} summer / {} winter collision triangles and {} exterior grind chains",summer.geometry.collision.len(),winter.geometry.collision.len(),summer.rails.len());
 }
}
