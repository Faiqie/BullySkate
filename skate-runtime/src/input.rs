use bevy::prelude::*;
pub(crate) mod gesture_catalog;
mod controllers;
mod gesture_mapping_data;
pub(crate) mod gesture_mapping;
pub(crate) mod gesture_input;
pub(crate) mod platform;
pub(crate) use controllers::{ControllerInput, ControllerStatus, RawInput};
use skate_core::input::tick::TickInput;
#[derive(Resource)]
pub(crate) struct PublishedTickInput(pub TickInput);

impl ControllerInput {
 pub(crate) fn host_sample(&mut self,state:skate_core::input::xbox::XboxState)->TickInput {
  let mut samples=std::array::from_fn(|_|Err(platform::DeviceError::Disconnected));
  samples[0]=Ok(platform::DevicePacket{number:self.publications as u32,state,subtype:1});
  self.collect(samples);self.publish_actions();self.tick_input()
 }
}

