//! Inert source-game extension records; the Bully host has no Lua 5.4 VM.
#[derive(Clone,Debug,Default)]
pub struct JointOverride {
 pub swing_limit:Option<f32>,pub twist_limit:Option<f32>,pub free_swing:Option<bool>,pub free_twist:Option<bool>,
 pub drive_enabled:Option<bool>,pub enabled:Option<bool>,pub descendants:bool,pub possession_enabled:Option<bool>,
}
#[derive(Clone,Debug,Default)]
pub struct PartOverride {
 pub motion:Option<String>,pub collision:Option<bool>,pub friction:Option<f32>,
 pub animation_drives:Option<bool>,pub possession_drives:Option<bool>,
}
