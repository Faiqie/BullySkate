fn main()->Result<(),String>{
 use skate_core::animation::output::{self,Sqt};
 let root=std::path::Path::new(r"prepared/skate-assets");
 let banks=skate_data::animation_banks::AnimationBanks::load(root)?;
 let frames=skate_data::animation_frames::AnimationFrames::from_banks(&banks)?;
 let reference=frames.named_pose("RIG_TPOSE")?;
 let mut matrices:Vec<_>=reference.samples.iter().map(|w|output::sqt_to_matrix(Sqt{scale:[f32::from_bits(w[0]),f32::from_bits(w[1]),f32::from_bits(w[2]),0.],rotation:std::array::from_fn(|i|f32::from_bits(w[3+i])),translation:[f32::from_bits(w[7]),f32::from_bits(w[8]),f32::from_bits(w[9]),0.]})).collect();
 output::compose_hierarchy_in_place(matrices.len() as i32,&frames.parents,0,&mut matrices).map_err(|e|format!("{e:?}"))?;
 std::fs::write("work/source-reference.json",serde_json::to_vec_pretty(&serde_json::json!({"names":frames.bone_names,"pose":matrices})).unwrap()).map_err(|e|e.to_string())?;
 for(n,m)in frames.bone_names.iter().zip(matrices){println!("{n} {:?}",&m[3][..3]);}
 Ok(())
}
