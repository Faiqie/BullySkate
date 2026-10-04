//! The local player's sounds on the native runtime (`skate_audio::player`), hosted by
//! `native::mixmap_frame` once per frame that took physics steps (`native::HostClock`):
//! 1. inputs: PlayerPhysics, the two 3DObjPos blocks, Jitter, Contacts, Rail, OffBoard;
//! 2. components' process (posts / releases) — before the MixMap tick;
//! 3. components' update (packets rewritten from the MixMap outputs, redelivered) — after it.
//!
//! The player components run whenever their banks are in the install (2026-10-03: the interim
//! cue tables and their opt-outs are gone; without the banks the skater's sounds are silent and the
//! host logs an error).
use std::collections::HashMap;

use skate_audio::eval::NodeId;
use skate_audio::mixmap::{MixMap, keys};
use skate_audio::player::collision::{self as collisions, CollisionManager};
use skate_audio::player::components::{self, Command, FootDrag, Grind, SenseOfSpeed, Skid, Slot, Squeaks};
use skate_audio::player::contacts::{self as board_contacts, ContactsTuning};
use skate_audio::player::inputs::{self, Contacts, Physics};
use skate_audio::player::jitter::Jitter;
use skate_audio::player::objpos::{Listener, ObjPos};
use skate_audio::player::tuning::PlayerTuning;
use skate_audio::player::clothing::{Clothing, ClothingTuning};
use skate_audio::player::footsteps::{FootstepTuning, Footsteps, LandingBucket};
use skate_audio::player::globals::Globals;
use skate_audio::player::rolling::{self, BoardSlide, Rattle, Rolling, RollingInputs, Routed};
use skate_audio::player::treatment::{self, Treatment, TreatmentGlobals};
use skate_audio::player::tricks::{self, Tricks};
use skate_audio::player::wheels::{Wheels, WheelsTuning};
use skate_audio::player::{AudioState, Owner};
use skate_audio::runtime::Runtime;

pub(crate) struct PlayerAudio {
    pub(crate) tuning: PlayerTuning,
    physics: Physics,
    contacts: Contacts,
    jitter: Jitter,
    /// The Jitter walk steps for the next [`Self::write_inputs`]: `Some(n)` under the MixMap's
    /// console cadence (n console evaluations in this pass, `native::mixmap_frame`), `None`: one
    /// step per call (the old 60 Hz host and the tests).
    pub(crate) jitter_steps: Option<usize>,
    positions: [ObjPos; 2],
    was_grinding: bool,
    last_camera: Option<[f32; 3]>,
    /// The components run (banks loaded, classes bound).
    pub(crate) components: bool,
    grind: Grind,
    speed: SenseOfSpeed,
    foot_drag: FootDrag,
    skid: Skid,
    squeaks: Squeaks,
    /// `Class_Seams` (the wheels' seam / crack hits, `Seams_Bank`).
    seams: skate_audio::player::seams::Seams,
    /// The rendered board's interpolation factor for this call (the fixed step's overstep fraction):
    /// Class_Seams reads the wheel positions interpolated between the last two physics samples, as
    /// retail's per-rendered-frame process does (Listening test 9). `None`: the physics positions.
    pub(crate) seam_alpha: Option<f32>,
    /// The last two physics samples of the wheel positions (previous, current).
    seam_wheels: Option<([[f32; 3]; 4], [[f32; 3]; 4])>,
    /// `SFXObj_Contacts`' Splice one-shots (pops, landings, touchdowns); on when the install has
    /// the `Skate_Collisions` patch tree ([`PlayerAudio::contacts_on`]).
    board: board_contacts::Contacts,
    pub(crate) contact_tuning: ContactsTuning,
    pub(crate) contacts_on: bool,
    /// The collision manager (`CSTATEMGR_Collision`): the contact pairs the board contacts post
    /// (grind start, landing pair, deck impacts); on with the contacts.
    collision: CollisionManager,
    /// `SFXObj_Wheels` (the spin-down streams); on when the install has both recordings
    /// ([`WHEEL_STREAMS`], decoded at start).
    wheels: Wheels,
    pub(crate) wheels_tuning: WheelsTuning,
    pub(crate) wheels_on: bool,
    /// `SFXObj_SkateBoard`'s rolling layers (`player::rolling`): the two-truck surface routing with
    /// its Class_rolling patches and held layers (on with `PatchBank_Rolling_Surfaces`), the rattle
    /// (`Rolling_Rattles`) and the loose-board slide (`board_scrapes`).
    rolling: Rolling,
    rattle: Rattle,
    slide: BoardSlide,
    pub(crate) rolling_on: bool,
    pub(crate) rattle_on: bool,
    pub(crate) slide_on: bool,
    /// The routing's grain binds / stops since the bed last took them (the bed follows the
    /// routing while [`PlayerAudio::rolling_on`]).
    pub(crate) routed: Routed,
    /// The Tricks component (Class_Flips, cloth_trick; `Sk8_Air_Flip_Tricks`, `Foley_Cloth`) and
    /// Class_Treatment (`Treatments`), each on when its banks are in the install.
    tricks: Tricks,
    treatment: Treatment,
    pub(crate) tricks_on: bool,
    pub(crate) treatment_on: bool,
    /// The game-mode globals those read (free skate).
    globals: Globals,
    /// SFXObj_OffBoard's footsteps (packets + the foot-down / walking / jump Splice sounds) and the
    /// Clothing component (cloth falls, push foley, body slide): on with the board contacts (their
    /// Splice banks), plus OffBoard's water splash.
    footsteps: Footsteps,
    footstep_tuning: FootstepTuning,
    clothing: Clothing,
    pub(crate) clothing_tuning: ClothingTuning,
    pub(crate) footsteps_on: bool,
    /// `+300` the landing bucket (it reads the resolved audio trick), this frame's value.
    landing: LandingBucket,
    landing_bucket: i32,
    /// The last listener the position writers used (the collision slots' 3-D blocks).
    last_listener: Option<Listener>,
    nodes: HashMap<Slot, NodeId>,
    classes: HashMap<&'static str, usize>,
    /// Commands of the last frame (for the log on change and the tests).
    pub(crate) posts: u64,
}

impl PlayerAudio {
    pub(crate) fn new(tuning: PlayerTuning, components: bool) -> Self {
        let jitter = Jitter::new(&tuning.jitter, skate_audio::grain::GrainBed::SEED);
        // The collision voices play through their Collision SubMix (`sub_824D25E0`: mono, env send
        // at the category's Collision output, eEQChain bus).
        let mut collision = CollisionManager::default();
        collision.submix = true;
        let mut board = board_contacts::Contacts::default();
        // Session review 2026-10-03: the push foot's plant / lift (#2), the body poster (#4) and the
        // grind on / off sounds (#5); the bridge's speed graph on the body impacts (`sub_824B0DA8`).
        board.plant_lift_on = true;
        board.body_on = true;
        board.body_speed_on = true;
        let mut grind = Grind::default();
        grind.onoff = true;
        Self {
            tuning,
            physics: Physics::default(),
            contacts: Contacts::default(),
            jitter,
            jitter_steps: None,
            positions: [ObjPos::default(); 2],
            was_grinding: false,
            last_camera: None,
            components,
            grind,
            speed: SenseOfSpeed::default(),
            foot_drag: FootDrag::default(),
            skid: Skid::default(),
            squeaks: Squeaks::default(),
            seams: Default::default(),
            seam_alpha: None,
            seam_wheels: None,
            board,
            contact_tuning: ContactsTuning::default(),
            contacts_on: false,
            collision,
            wheels: Wheels::default(),
            wheels_tuning: WheelsTuning::default(),
            wheels_on: false,
            rolling: Rolling::default(),
            rattle: Rattle::default(),
            slide: BoardSlide::default(),
            rolling_on: false,
            rattle_on: false,
            slide_on: false,
            routed: Routed::default(),
            tricks: Tricks::default(),
            treatment: Treatment::default(),
            tricks_on: false,
            treatment_on: false,
            globals: Globals::default(),
            footsteps: Footsteps::default(),
            footstep_tuning: FootstepTuning::default(),
            clothing: Clothing::default(),
            clothing_tuning: ClothingTuning::default(),
            footsteps_on: false,
            landing: LandingBucket::default(),
            landing_bucket: 1,
            last_listener: None,
            nodes: HashMap::new(),
            classes: HashMap::new(),
            posts: 0,
        }
    }

    /// The body poster's messages so far and their digest (`Contacts::body_posts` / `body_digest`).
    #[cfg(test)]
    pub(crate) fn body_trace(&self) -> (u64, u64) {
        (self.board.body_posts, self.board.body_digest)
    }

    /// The deck poster's messages so far and their digest (`Contacts::deck_posts` / `deck_digest`).
    #[cfg(test)]
    pub(crate) fn deck_trace(&self) -> (u64, u64) {
        (self.board.deck_posts, self.board.deck_digest)
    }

    /// Diagnostics (the e2e harness's `E2E_BODY_LOG`): keep every body-poster message.
    #[cfg(test)]
    pub(crate) fn set_body_log(&mut self, on: bool) {
        self.board.body_log = on.then(Vec::new);
    }

    /// The body-poster messages since the last call (region, impact read, message).
    #[cfg(test)]
    pub(crate) fn take_body_log(&mut self) -> Vec<(usize, f32, skate_audio::player::collision::Message)> {
        self.board.body_log.as_mut().map(std::mem::take).unwrap_or_default()
    }

    /// Collision messages of another Player-slot owner (an NPC skater, `npc_skaters.rs`): retail
    /// has one `CSTATEMGR_Collision`, so they join the local player's (processed at its next
    /// `process`). No messages, no change.
    pub(crate) fn post_collisions(&mut self, msgs: Vec<skate_audio::player::collision::Message>, rt: &mut Runtime) {
        if msgs.is_empty() || !self.contacts_on {
            return;
        }
        let mut host = rt.splice_host();
        for msg in msgs {
            self.collision.post(msg, &mut host);
        }
    }

    /// The materials' footstep layers (`Library::footstep_materials`).
    pub(crate) fn set_footstep_materials(&mut self, materials: Vec<skate_audio::player::footsteps::FootstepMaterial>) {
        self.footstep_tuning.materials = materials;
    }

    /// The listener as the position controllers see it: the camera (position, view, velocity from
    /// the last frame) and the followed point (the skater's COM).
    /// `dt`: the time since the last call.
    pub(crate) fn listener(&mut self, camera: [f32; 3], view: [f32; 3], dt: f32, s: &AudioState) -> Listener {
        let velocity = match self.last_camera {
            Some(last) if dt > 0.0 => std::array::from_fn(|i| (camera[i] - last[i]) / dt),
            _ => [0.0; 3],
        };
        self.last_camera = Some(camera);
        Listener {
            camera,
            view,
            camera_velocity: velocity,
            followed: s.com_position,
            // Frame B's direction is only read by the skater-frame azimuth (input 2), which no B
            // lookup of the MixMap uses; the COM velocity stands in for the facing.
            facing: s.com_velocity,
            followed_velocity: s.com_velocity,
        }
    }

    /// Step 1: every input the player's writers supply (mixmap-spec §7).
    pub(crate) fn write_inputs(&mut self, m: &mut MixMap, s: &AudioState, listener: Option<&Listener>) -> bool {
        self.last_listener = listener.copied();
        self.physics.write(m, s);
        if let Some(l) = listener {
            // 60010010 follows the skater (COM, COM velocity); 60010020 the board (deck, its
            // velocity), as `sub_824B0C48` binds them.
            self.positions[0].write(m, keys::obj_pos(0), l, Some((s.com_position, s.com_velocity)));
            self.positions[1].write(m, keys::obj_pos2(0), l, Some((s.board_position, s.board_velocity)));
        }
        for _ in 0..self.jitter_steps.unwrap_or(1) {
            // The MixMap writes don't touch the walk: the same draws and writes as collecting first.
            self.jitter.process_each(|id, word| m.set_input(keys::JITTER, id, word));
        }
        let landed = self.contacts.write(m, s, &self.tuning);
        inputs::write_rail(m, s.grinding, self.was_grinding);
        self.was_grinding = s.grinding;
        inputs::write_off_board(m, s);
        landed
    }

    fn apply(&mut self, rt: &mut Runtime, cmds: Vec<Command>) {
        for cmd in cmds {
            match cmd {
                Command::Post { slot, class, words } => {
                    let id = *self.classes.entry(class).or_insert_with(|| rt.eval.class_id(class).unwrap_or(usize::MAX));
                    if id == usize::MAX {
                        continue;
                    }
                    if let Some(old) = self.nodes.remove(&slot) {
                        rt.release(old);
                    }
                    self.nodes.insert(slot, rt.post(id, &words));
                    self.posts += 1;
                    if trace() {
                        eprintln!("AUDIO_NATIVE post {class} {slot:?} words={words:?}");
                    }
                }
                Command::Redeliver { slot, words } => {
                    if let Some(&node) = self.nodes.get(&slot) {
                        rt.redeliver(node, &words);
                    }
                }
                Command::Release { slot } => {
                    if let Some(node) = self.nodes.remove(&slot) {
                        rt.release(node);
                        if trace() {
                            eprintln!("AUDIO_NATIVE release {slot:?}");
                        }
                    }
                }
            }
        }
    }

    /// The state with the host-resolved fields (`+348` / `+352` the audio tricks of the scorable).
    fn resolved(&self, s: &AudioState) -> AudioState {
        let mut s = *s;
        let name = usize::try_from(s.scorable).ok().and_then(|id| crate::catalog::IDENTIFIERS.get(id)).map(|(name, ..)| name);
        s.audio_trick = name.map_or(-1, |n| self.tuning.audio_trick(n));
        s.audio_trick_2 = name.map_or(-1, |n| self.tuning.audio_trick_2(n));
        s
    }

    /// The loose-board state (`+780`, `rolling::loose_board`): the deck in contact while the rider
    /// bails or walks, by how the deck lies (`skate_events::Riding`: `up_dot` = SkateboardReckoning
    /// +80 · Ground+80, the deck contact Collision+3475 and its material Collision+12 − 1).
    pub(crate) fn loose_board(s: &AudioState, riding: &super::skate_events::Riding) -> u32 {
        rolling::loose_board(s.bail || s.on_foot, riding.deck_contact, riding.deck_material, riding.deck_up)
    }

    /// The six jitter values the jittered eEQChain buses 5–7 read (`sub_82491180`), when the
    /// install's tuning names all six channels.
    pub(crate) fn eq_jitter(&self) -> Option<[f32; 6]> {
        let idx = self.tuning.eq_jitter;
        if idx.iter().any(Option::is_none) {
            return None;
        }
        let mut out = [0.0f32; 6];
        for (o, i) in out.iter_mut().zip(idx) {
            *o = self.jitter.channels.get(i?)?.value;
        }
        Some(out)
    }

    /// Class_Seams' hits so far and how many were material changes (diagnostics, e2e).
    #[cfg(test)]
    pub(crate) fn seam_hits(&self) -> (u64, u64) {
        (self.seams.hits, self.seams.transitions)
    }

    fn seam_commands(cmds: Vec<skate_audio::player::seams::SeamCommand>) -> Vec<Command> {
        use skate_audio::player::seams::{CLASS, SeamCommand};
        cmds.into_iter()
            .map(|c| match c {
                SeamCommand::Post { wheel, words } => Command::Post { slot: Slot::Seam(wheel as u8), class: CLASS, words },
                SeamCommand::Redeliver { wheel, words } => Command::Redeliver { slot: Slot::Seam(wheel as u8), words },
                // Scheduled on the audio clock by `seam_frame`; never reaches here.
                SeamCommand::RedeliverAt { wheel, words, .. } => Command::Redeliver { slot: Slot::Seam(wheel as u8), words },
            })
            .collect()
    }

    /// Step 2 (before the tick): the components' process. `speed_scale`: the bed's push
    /// speed-scale envelope while it runs; `loose`: the loose-board state ([`Self::loose_board`]).
    pub(crate) fn process(&mut self, m: &mut MixMap, s: &AudioState, rt: &mut Runtime, speed_scale: Option<f32>, loose: u32) {
        if !self.components {
            return;
        }
        let m_mut = m;
        let mut resolved = self.resolved(s);
        self.landing_bucket = self.landing.update(resolved.com_velocity[1], resolved.offboard_air, resolved.trick_active, resolved.audio_trick);
        resolved.landing_bucket = self.landing_bucket;
        let s = &resolved;
        let mut rolling_cmds = Vec::new();
        if self.rolling_on {
            // SFXObj_SkateBoard's process: the routing, then the rattle (it reads the routing),
            // then the board slide; SkateBoard inputs 0 / 6 from the routing.
            let (cmds, routed) = self.rolling.process(s, &self.tuning, &RollingInputs { speed_scale });
            rolling_cmds = cmds;
            let board = keys::skateboard(0);
            m_mut.set_input(board, 0, if routed.surface_pulse { 32767 } else { 0 });
            m_mut.set_input(board, 6, if routed.on_metal { 32767 } else { 0 });
            self.routed.grains.extend(routed.grains);
            self.routed.primary = routed.primary;
            self.routed.surface_pulse = routed.surface_pulse;
            self.routed.on_metal = routed.on_metal;
        }
        if self.rattle_on {
            rolling_cmds.extend(self.rattle.process(s, &self.rolling, &self.tuning.rolling));
        }
        if self.slide_on {
            rolling_cmds.extend(self.slide.process(loose, &self.tuning.rolling));
        }
        let seam_state = self.seam_state(s);
        // Class_Seams' process runs on the console cadence in `seam_frame`; the tick only creates
        // the packets and writes Cracks.in0.
        let seams = Self::seam_commands(self.seams.process_tick(&seam_state, &self.tuning, m_mut));
        let m: &MixMap = m_mut;
        let rail = Owner { mixmap: m, key: keys::rail(0) };
        let mut cmds = self.grind.process(s, &self.tuning, &rail);
        cmds.extend(self.speed.process(s));
        cmds.extend(self.foot_drag.process(s, &self.tuning));
        let board = Owner { mixmap: m, key: keys::skateboard(0) };
        cmds.extend(self.skid.process(s, &self.tuning, &board));
        cmds.extend(self.squeaks.process(s));
        cmds.extend(seams);
        cmds.extend(rolling_cmds);
        if self.tricks_on {
            cmds.extend(self.tricks.process(s, &self.tuning.tricks, &self.globals, &Owner { mixmap: m, key: keys::tricks(0) }));
        }
        if self.treatment_on {
            cmds.extend(self.treatment.process(s, &self.tuning.treatment, &self.globals, &Owner { mixmap: m, key: keys::treatments(0) }));
        }
        self.apply(rt, cmds);
        if self.contacts_on {
            // The grind on / off contact sounds of this frame's Rail posts (Splice).
            self.grind.sounds(s, &self.tuning, &mut rt.splice_host());
            let before = self.board.starts;
            // The body / deck posters run once per console evaluation of this pass (their 15 / 6-frame
            // cooldowns = 0.5 / 0.2 s at any frame rate; `jitter_steps` None = per call, the tests).
            self.board.body_calls = self.jitter_steps;
            self.board.deck_calls = self.jitter_steps;
            self.board.process(s, self.contacts.buckets(), &self.tuning, &self.contact_tuning, &mut rt.splice_host());
            self.posts += self.board.starts - before;
            // The collision manager runs after the player's components (its own state manager).
            let mut host = rt.splice_host();
            for msg in std::mem::take(&mut self.board.outbox) {
                self.collision.post(msg, &mut host);
            }
            self.collision.process(m_mut, self.last_listener.as_ref());
        }
        if self.footsteps_on {
            let splashes = self.footsteps.splash.starts;
            let mut cmds = self.footsteps.process(s, &self.tuning, &self.footstep_tuning, &mut rt.splice_host());
            if self.footsteps.splash.starts != splashes {
                // OffBoard's water splash (`player::footsteps::Splash`, retail `sub_824EBB58`).
                eprintln!("AUDIO_EVENT splash native Skate_Collisions:{}", self.footsteps.splash.last_id);
            }
            cmds.extend(self.clothing.process(s, &self.tuning, &self.clothing_tuning, &mut rt.splice_host()));
            self.apply(rt, cmds);
        }
    }

    /// The wheel positions of the last two physics steps (`Riding::wheels_before`, this sample's),
    /// which the rendered board interpolates between: the pair [`Self::seam_state`] reads. Hosts
    /// set it before every call (2026-10-03: the pair follows the physics steps, so a frame that
    /// takes two steps interpolates between the last two, and a board at rest has no spread);
    /// `before = now` after a camera cut / teleport. Without it the pair follows the samples' changes.
    pub(crate) fn step_wheels(&mut self, before: [[f32; 3]; 4], now: [[f32; 3]; 4]) {
        self.seam_wheels = Some((before, now));
    }

    /// Forget the camera's last position (teleport, camera cut, map change): the next listener
    /// has no velocity instead of the jump's (Doppler).
    pub(crate) fn reset_listener(&mut self) {
        self.last_camera = None;
    }

    /// The state Class_Seams reads: with [`Self::seam_alpha`], the wheel positions of the rendered
    /// board (the last two physics samples interpolated), else the physics sample.
    fn seam_state(&mut self, s: &AudioState) -> AudioState {
        let Some(alpha) = self.seam_alpha else { return *s };
        let now = s.wheel_position;
        let (prev, cur) = match self.seam_wheels {
            // The host's pair for this sample ([`Self::step_wheels`]).
            Some(pair) if pair.1 == now => pair,
            Some((_, cur)) => (cur, now),
            None => (now, now),
        };
        self.seam_wheels = Some((prev, cur));
        let mut out = *s;
        let u = alpha.clamp(0.0, 1.0);
        out.wheel_position = std::array::from_fn(|w| std::array::from_fn(|c| prev[w][c] + (cur[w][c] - prev[w][c]) * u));
        out
    }

    /// Class_Seams on the console cadence (Listening test 9): called on every rendered frame of
    /// `dt` seconds, before [`Self::process`] on frames that tick. Runs the seams' process on a 30 Hz
    /// virtual grid at the rendered wheel positions (`seams::Seams::frame`) and their update once
    /// per console frame; the tick only creates the packets and writes Cracks.in0.
    pub(crate) fn seam_frame(&mut self, m: &MixMap, s: &AudioState, dt: f32, rt: &mut Runtime) {
        if !self.components {
            return;
        }
        let seam_state = self.seam_state(s);
        let calls = self.seams.calls;
        let (now, later): (Vec<_>, Vec<_>) = self
            .seams
            .frame(&seam_state, &self.tuning, dt, rt.blocks)
            .into_iter()
            .partition(|c| !matches!(c, skate_audio::player::seams::SeamCommand::RedeliverAt { .. }));
        if !now.is_empty() {
            self.apply(rt, Self::seam_commands(now));
        }
        for c in later {
            if let skate_audio::player::seams::SeamCommand::RedeliverAt { wheel, words, block } = c {
                if let Some(&node) = self.nodes.get(&Slot::Seam(wheel as u8)) {
                    rt.redeliver_at(node, &words, block);
                }
            }
        }
        // The update half once per console frame too (its turn word slews per packet write), with
        // the last tick's Cracks outputs.
        if self.seams.calls != calls {
            let cracks = Owner { mixmap: m, key: keys::cracks(0) };
            let cmds = Self::seam_commands(self.seams.update(&seam_state, &cracks));
            self.apply(rt, cmds);
        }
    }

    /// Step 3 (after the tick): the components' update.
    pub(crate) fn update(&mut self, m: &MixMap, s: &AudioState, rt: &mut Runtime, speed_scale: Option<f32>, loose: u32) {
        if !self.components {
            return;
        }
        let mut resolved = self.resolved(s);
        resolved.landing_bucket = self.landing_bucket;
        let s = &resolved;
        let rail = Owner { mixmap: m, key: keys::rail(0) };
        let sos = Owner { mixmap: m, key: keys::sense_of_speed(0) };
        let contacts = Owner { mixmap: m, key: keys::contacts(0) };
        let mut cmds = self.grind.update(s, &self.tuning, &rail);
        cmds.extend(self.speed.update(s, &sos));
        cmds.extend(self.foot_drag.update(s, &self.tuning, &contacts));
        let board = Owner { mixmap: m, key: keys::skateboard(0) };
        cmds.extend(self.skid.update(s, &self.tuning, &board));
        cmds.extend(self.squeaks.update(s, &board));
        // The seams' update runs in `seam_frame` (the console cadence).
        if self.rolling_on {
            cmds.extend(self.rolling.update(s, &self.tuning, &RollingInputs { speed_scale }, &board));
        }
        if self.rattle_on {
            cmds.extend(self.rattle.update(&board));
        }
        if self.slide_on {
            cmds.extend(self.slide.update(s, loose, &self.tuning.rolling, &board));
        }
        if self.tricks_on {
            cmds.extend(self.tricks.update(s, &self.tuning.tricks, &Owner { mixmap: m, key: keys::tricks(0) }));
        }
        if self.treatment_on {
            cmds.extend(self.treatment.update(s, &TreatmentGlobals::default(), &self.tuning.treatment, &Owner { mixmap: m, key: keys::treatments(0) }));
        }
        self.apply(rt, cmds);
        if self.contacts_on {
            // The Rail updater's end (`sub_824C42A8`): the grind on / off sounds, after its packets
            // (the family-change starts of `Grind::update` first).
            self.grind.sounds(s, &self.tuning, &mut rt.splice_host());
            self.grind.update_sounds(s, &rail, &mut rt.splice_host());
            self.board.update(s, &contacts, &self.contact_tuning, &mut rt.splice_host());
            let before = self.collision.starts;
            self.collision.update(m, &self.tuning.collision, s.dt, &mut rt.splice_host());
            self.posts += self.collision.starts - before;
        }
        if self.footsteps_on {
            let off = Owner { mixmap: m, key: keys::off_board(0) };
            let mut cmds = self.footsteps.update(s, &self.tuning, &self.footstep_tuning, &off, &mut rt.splice_host());
            let cloth = Owner { mixmap: m, key: keys::clothing(0) };
            cmds.extend(self.clothing.update(s, &self.tuning, &self.clothing_tuning, &cloth, &mut rt.splice_host()));
            self.apply(rt, cmds);
        }
        if self.wheels_on {
            let owner = Owner { mixmap: m, key: keys::wheels(0) };
            let before = self.wheels.starts;
            self.wheels.update(s, &owner, &self.wheels_tuning, &mut rt.stream_host());
            self.posts += self.wheels.starts - before;
        }
    }
}

/// `SKATE_AUDIO_TRACE=1`: the `AUDIO_NATIVE post` / `release` lines (one per component post or
/// release, written under the runtime lock; off by default since 2026-10-03).
fn trace() -> bool {
    static ON: std::sync::OnceLock<bool> = std::sync::OnceLock::new();
    *ON.get_or_init(|| std::env::var_os("SKATE_AUDIO_TRACE").is_some_and(|v| v != "0"))
}

/// The banks the components post into.
pub(crate) const BANKS: &[&str] = components::BANKS;
/// Optional component banks: a component runs when its first bank loads (the rest add layers).
pub(crate) const ROLLING_BANKS: &[&str] = &["PatchBank_Rolling_Surfaces", "PatchBank_SpiderCracks", "PatchBank_Objects", "PatchBank_RocksBounce"];
pub(crate) const RATTLE_BANKS: &[&str] = &["Rolling_Rattles"];
pub(crate) const SLIDE_BANKS: &[&str] = &["board_scrapes"];
pub(crate) const TRICKS_BANKS: &[&str] = tricks::BANKS;
pub(crate) const TREATMENT_BANKS: &[&str] = treatment::BANKS;
pub(crate) const OPTIONAL_BANKS: [&[&str]; 5] = [ROLLING_BANKS, RATTLE_BANKS, SLIDE_BANKS, TRICKS_BANKS, TREATMENT_BANKS];
/// The Splice banks the components play (patch trees from `audio_export.splice_trees`): the board
/// contacts need the first; the collision manager's metal family and the shoe scuffs use the others
/// when present.
pub(crate) const SPLICE_BANKS: &[&str] = &[board_contacts::BANK, collisions::BANKS[1], board_contacts::FOLEY];
/// `SFXObj_Wheels`' recordings (stream 0 jump, 1 manual) in the manifest's `wheels`.
pub(crate) const WHEEL_STREAMS: [&str; 2] = ["Whls_spins_Jump_1", "Whls_spins_Man_1"];

