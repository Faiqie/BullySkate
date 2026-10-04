//! Pointer-free observation of the published simulation for the audio sidecar.
impl super::SkateHost {
 pub fn audio_frame(&self)->[f32;64]{
  let mut a=[0.;64];let p=&self.skater.player_input.physical;
  let ground=&self.physics.riding.ground;let parts=self.physics.board.part_transforms();
  let v=self.physics.board.bodies()[6].rates.linear_velocity;
  a[0]=self.physics.ticks as f32;a[1]=self.period();a[2]=self.skater.player_state.current() as u32 as f32;a[3]=self.area as f32;
  a[4]=self.physics.riding.motion.ground_speed;
  a[5..8].copy_from_slice(&[parts[6].translation.x,parts[6].translation.y,parts[6].translation.z]);
  a[8..11].copy_from_slice(&[v.x,v.y,v.z]);
  for i in 0..4{
   a[12+i]=ground.parts[i].in_contact as u8 as f32;a[11]+=a[12+i];
   let w=parts[i].translation;a[16+i*3..19+i*3].copy_from_slice(&[w.x,w.y,w.z]);
  }
  a[28..31].copy_from_slice(&std::array::from_fn::<_,3,_>(|i|f32::from_bits(p.reckoning.vector_64[i])));
  a[31]=self.skater.animation_input.fields.turn;a[32]=self.skater.ground.pumping.absorption;
  let flags=&self.skater.player_state.state_flags;
  for (i,index) in [52,54,60,59,56,57].iter().enumerate(){a[33+i]=flags.get(index-52).copied().unwrap_or(false) as u8 as f32;}
  a[39]=self.preferences.wheels;a[40]=p.grinds.words_136_140[0] as f32;a[41]=p.grinds.impact_speed_128;
  a[42]=(p.skeleton.flag_600!=0) as u8 as f32;a[43]=(p.skeleton.flag_601!=0) as u8 as f32;
  a[44]=self.skater.ground.steering.deck_tilt;
  let av=self.physics.board.bodies()[6].rates.angular_velocity;
  for i in 0..3{let axis=parts[6].basis.columns[i];a[45+i]=av.x*axis[0]+av.y*axis[1]+av.z*axis[2];}
  let delta:[f32;3]=std::array::from_fn(|i|f32::from_bits(p.air.jump_velocity_delta_112[i]));
  a[48]=(delta.iter().map(|v|v*v).sum::<f32>().sqrt()/2.65).clamp(0.,1.);
  let score=&self.skater.animation.motion.score_packet;
  a[49]=if score.flags&0x0300_0000!=0{score.trick_names.first.and_then(|name|self.skater.scoring.data.definitions.iter().find(|d|d.encoded_name==name).map(|d|d.metadata.id as f32)).unwrap_or(-1.)}else{-1.};
  a[50]=ground.parts[6].in_contact as u8 as f32;
  let acceleration=ground.accelerations[6];let normal=ground.parts[6].normal;
  a[51]=if a[50]>0.{((acceleration.x*normal.x+acceleration.y*normal.y+acceleration.z*normal.z).abs()*0.00125).clamp(0.,1.)}else{0.};
  a[52]=p.off_board.flag_311 as f32;a[53]=p.air.scalar_184;a[54]=p.air.jump_height_200;
  a[55]=(self.marker.returns) as f32;
  let up=parts[6].basis.columns[1];a[56]=up[1];
  let right=parts[6].basis.columns[0];a[57]=v.x*right[0]+v.y*right[1]+v.z*right[2];
  a[58..61].copy_from_slice(&std::array::from_fn::<_,3,_>(|i|f32::from_bits(p.reckoning.vector_16[i])));
  a[61]=flags.get(55-52).copied().unwrap_or(false) as u8 as f32;
  a[62]=((p.grinds.grinding_316!=0&&p.grinds.leaving_317==0) as u8
   | ((p.skeleton.over_599!=0) as u8)<<1
   | ((flags.get(66-52).copied().unwrap_or(false)) as u8)<<2) as f32;
  a
 }
}
