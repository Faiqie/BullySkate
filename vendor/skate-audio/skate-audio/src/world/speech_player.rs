//! The speech streams of one speech channel (0 = the main cast, 1 = the living world): the lines the speech manager
//! starts ([`super::speech_manager`]), played as stream voices whose level, send, pitch, pan and
//! filters follow the speaker's MixMap owner every console frame. Read from the TU3 recompilation
//! (reference only; addresses are facts):
//!
//! - **Owner → stream values.** The speaker's speech owner (`SFXObj_PedestrianSpeech` update
//!   `sub_824D9370`; a skater's `SFXObj_PlayerSpeech` update `sub_824DA300`) copies its outputs into
//!   a parameter block the line's stream voice reads (`sub_824A84B8` finds it by the speaker's id
//!   when the line starts): the main level, a second level, the azimuth (raw 0), the pitch (1) and
//!   two filters. Which level depends on the speaker and on the clip: a clip whose name ends in
//!   `_f` (`sub_824A89E8`, the string `"_f"` at `0x8224EC48`) takes the next output (the far
//!   variant). For a regular ped that is PedestrianSpeech out2 / out3 with out15 as the second;
//!   the filters are out13 (high pass) and out14 (low pass) ([`ped_outputs`]). For a skater with
//!   a living-world voice (model ≥ 41) PlayerSpeech out2 / out3, second out10, filters out8 / out9
//!   ([`skater_outputs`]). The block also carries the echo send (ped out21, skater out13), the
//!   echo's filters (ped 22 / 23, skater 14 / 15) and, every [`SpeechVoiceTuning::delay_frames`]
//!   console frames, the echo delay from the camera distance.
//! - **Block → voice** (the stream system, `sub_82C5CEF0`, every frame): the voice gain = the
//!   level / 32767 × the speaker's per-voice float (`S+152`, `aud_characteristics`
//!   `2087A3290483BB4F`, 0.8–1.4); the post-filter send (`desc+32`, the second level × the float)
//!   goes to the environment bus; the pre-gain send (`desc+16`, the echo send × the float) feeds the
//!   stream slot's echo submix (`crate::bus::speech_echo`: HPF → delay → LPF → the environment bus);
//!   PEAK = [`SpeechVoiceTuning::peak`] of the azimuth.
//! - **The recomp agrees** (sessions 163809 / 164620 / 180430, the local tool `speech_levels.py`,
//!   skate-game test `speech_levels_follow_the_recomp`, 39 lines rebuilt at their geometry): a
//!   speech stream's LPF / HPF sit at exactly 24956 / 77 Hz near the speaker and 3489 / 379 Hz far
//!   away, our out14 / out13; its first GAIN over ours has median 1.005 (p10 0.55, p90 1.43); `_f`
//!   lines at 30 / 40 m play at 0.092 / 0.044 against our out3's 0.085 / 0.056, where out2 has fallen
//!   to 0.030 / 0.001. With the voice float the gain median is 0.988 (p10 0.55, p90 1.36); the
//!   pre-gain send (`+0x570`) is out21 within 25 % in 17 of 29 lines (out15: 10), the env send
//!   (`+0x7D0`) out15 in 19 of 27; every recomp PEAK lies on the curves. Note: the level lookups
//!   measure the distance to the followed skater (3DObjPos input 0), the near / far flag the
//!   camera distance.
//! - **Two streams per channel** (`sub_824A73F0` indexes the channel's stream records as
//!   `channel × 2 + k`; the recomp's living-world lines play on two stream players). A request
//!   takes a free stream. When none is free, the event's tuning decides (`sub_824A73F0`): `+13`
//!   lets it stop a playing line of lower priority, `+14` the same when the channel is full;
//!   otherwise it waits in the library's 16-request queue until its event's queue timeout
//!   (`.evt` `+2`) runs out. Its unit: the queue clock is `[[0x830CFD94]+16]`, the game's visual
//!   tick (the `GetVisualGameTick` Lua binding, one per rendered frame); the port counts console
//!   frames (the console renders at its ~30 fps cadence). Which of the two streams a request targets (`k`) is not traced: the
//!   lower-priority one is taken (provisional).
//! - **The cut** (`sub_824D9370`): while a line plays, a speaker whose main level stays at or below
//!   200 (vault `6995C510258C9AF6`) for more than 60 console frames (`3D8CD05C962FF399`) has its
//!   line stopped. A speaker that loses its MixMap instance stops its line too (deactivation
//!   `sub_824D92B0` / `sub_824DA110`).
//!
//! Not modelled: the "focus speaker" levels (out24 / out16 by a game global), the echo graph's
//! two-channel Pn21 / Sen0 routing gains (modelled as a mono unity tap) and which of the two
//! streams a full channel targets.
use std::collections::VecDeque;

use super::speech::{Line, SpeechIndex};
use crate::player::Outputs;

/// Streams of one channel.
pub const STREAMS: usize = 2;
/// The library's request queue.
pub const QUEUE: usize = 16;
/// The cut: a speaker level at or below this …
pub const CUT_LEVEL: i32 = 200;
/// … for more than this many console frames stops its line.
pub const CUT_FRAMES: u32 = 60;

const INV_32767: f32 = f32::from_bits(0x3800_0100);
const INV_4096: f32 = f32::from_bits(0x3980_0000);
/// 360 / 65535 (`0x822F8C64`): raw azimuth → degrees.
const DEGREES: f32 = f32::from_bits(0x3BB4_00B4);

/// The ped audio state words that pick PedestrianSpeech's level outputs (`sub_824D9370`). `S+100`,
/// `S+104` and `S+112` are not traced (the recomp's peds were not logged with them): 0 = a regular
/// ped, out2 / out3.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct PedLevelSelect {
    pub s100: i32,
    pub s104: i32,
    pub s112: i32,
    /// `S+96 == 64`: a security guard (out4 / out5; its radio event 8277 out6).
    pub security: bool,
}

/// PedestrianSpeech's (main level, second level) output ids for a line (`sub_824D9370`).
pub fn ped_level_ids(sel: PedLevelSelect, event: u16, far: bool) -> (usize, usize) {
    let b = usize::from(far);
    if sel.s100 != 0 {
        (9 + b, 19)
    } else if sel.s104 == 0 {
        if sel.s112 != 0 {
            (7 + b, 18)
        } else if sel.security {
            if event == 8277 { (6, 17) } else { (4 + b, 16) }
        } else {
            (2 + b, 15)
        }
    } else if matches!(sel.s104, 2 | 4) {
        (11 + b, 20)
    } else {
        (7 + b, 18)
    }
}

/// PlayerSpeech's (main level, second level) output ids (`sub_824DA300`): by the skater's model
/// (the pros below 30, 30–40, the living-world voices from 41).
pub fn skater_level_ids(model: u32, far: bool) -> (usize, usize) {
    let b = usize::from(far);
    if model < 30 {
        (4 + b, 12)
    } else if model < 41 {
        (6 + b, 11)
    } else {
        (2 + b, 10)
    }
}

/// The filter outputs (high pass, low pass) each owner kind copies (read with the MixMap's filter
/// reader).
pub const PED_FILTERS: [usize; 2] = [13, 14];
pub const SKATER_FILTERS: [usize; 2] = [8, 9];
/// The level each owner kind copies as the stream voice's pre-gain send (into the stream slot's
/// echo submix, `crate::bus::speech_echo`): PedestrianSpeech out21 (`sub_824D9370` → block `+80`),
/// PlayerSpeech out13 (`sub_824DA300`). Recomp: the voice's first send module (`+0x570`, before
/// the gain) follows it. The echo's filters: ped 22 / 23, skater 14 / 15.
pub const PED_SEND_A: usize = 21;
pub const SKATER_SEND_A: usize = 13;
pub const PED_ECHO_FILTERS: [usize; 2] = [22, 23];
pub const SKATER_ECHO_FILTERS: [usize; 2] = [14, 15];
/// Every filter output a snapshot must read with the filter reader.
pub const PED_SNAPSHOT_FILTERS: [usize; 4] = [13, 14, 22, 23];
pub const SKATER_SNAPSHOT_FILTERS: [usize; 4] = [8, 9, 14, 15];

/// An 8-point vault curve (`Sk8::PointNegGraphData8`: x at `+16`, y at `+48`), evaluated as
/// `sub_82481E10(8, …)`: below x0 → y0, from x7 on → y7, else the linear piece (`fmadds`).
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Graph8 {
    pub x: [f32; 8],
    pub y: [f32; 8],
}

impl Graph8 {
    pub fn eval(&self, v: f32) -> f32 {
        let (x, y) = (&self.x, &self.y);
        if v < x[0] {
            return y[0];
        }
        if !(v < x[7]) {
            return y[7];
        }
        for i in 1..8 {
            if v < x[i] {
                let dx = x[i] - x[i - 1];
                if dx > 0.0 {
                    return ((y[i] - y[i - 1]) / dx).mul_add(v - x[i - 1], y[i - 1]);
                }
                return y[i];
            }
        }
        y[0]
    }
}

/// The stream voice's PEAK filter by the speaker's azimuth (both speech owners' updates,
/// `sub_824D9370` / `sub_824DA300`): the owner's raw azimuth (output 0, 0..65535) folded to the
/// front / back angle (above 32767 → 65536 − raw), then three curves of the speech record
/// (class `B29C3B2C13D96482` `default`, holder `*(0x830CFDA4)+44`): centre `2C166907CF51DB88`
/// (600 Hz at the front, 4000 at the side, 600 behind), gain `EA2C18D9CE5CBA3A` (0.4 → 0.1),
/// Q `CF8679F540B82B2B` (3). Setup export `world_tuning.speech_voice`; the defaults are the shipped
/// curves. Recomp (163809): every speech voice's PEAK (centre, gain) lies on the two curves at one
/// azimuth.
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct SpeechVoiceTuning {
    pub peak_freq: Graph8,
    pub peak_gain: Graph8,
    pub peak_q: Graph8,
    /// The echo delay (`sub_824D9370`): camera distance × this (field `BF48032DB145C5B4`, 1.0) ×
    /// [`Self::per_metre`] (the image's 1/344 s at `0x822F940C`), at most [`Self::max_delay`]
    /// (0.15 s), recomputed every [`Self::delay_frames`] console frames (field `510BAFA32A76B340`, 4).
    pub delay_factor: f32,
    pub per_metre: f32,
    pub max_delay: f32,
    pub delay_frames: u32,
}

impl Default for SpeechVoiceTuning {
    fn default() -> Self {
        Self {
            peak_freq: Graph8 {
                x: [0.0, 2614.956, 5603.478, 8325.166, 15796.47, 21666.78, 25829.36, 32767.0],
                y: [600.0, 1195.0, 1802.143, 2336.429, 4000.0, 2676.428, 1838.571, 600.0],
            },
            peak_gain: Graph8 {
                x: [0.0, 4162.583, 8378.532, 12114.18, 16063.3, 21079.75, 26416.39, 32767.0],
                y: [0.4, 0.34, 0.291786, 0.2575, 0.222143, 0.185714, 0.143929, 0.1],
            },
            peak_q: Graph8 { x: [0.0, 4095.875, 8191.75, 12287.63, 16383.5, 20479.38, 24575.25, 28671.13], y: [3.0; 8] },
            delay_factor: 1.0,
            per_metre: f32::from_bits(0x3B3E_82FA),
            max_delay: f32::from_bits(0x3E19_999A),
            delay_frames: 4,
        }
    }
}

impl SpeechVoiceTuning {
    /// (centre Hz, linear gain, Q) for an owner's raw azimuth.
    pub fn peak(&self, raw_azimuth: i32) -> [f32; 3] {
        let a = raw_azimuth as f32;
        let a = if a > 32767.0 { 65536.0 - a } else { a };
        [self.peak_freq.eval(a), self.peak_gain.eval(a), self.peak_q.eval(a)]
    }

    /// The echo delay for a speaker `distance` m from the camera (`fsel`: the smaller of the two).
    pub fn delay(&self, distance: f32) -> f32 {
        let d = self.delay_factor * distance * self.per_metre;
        if self.max_delay - d >= 0.0 { d } else { self.max_delay }
    }
}

/// One frame's stream values.
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct VoiceParams {
    /// The owner's main level (raw, 0..32767: the cut reads it).
    pub level: i32,
    /// The voice's gain: main level / 32767 × the speaker's per-voice float (`S+152`,
    /// `aud_characteristics` `2087A3290483BB4F`; `sub_82C5CEF0` multiplies the gain and both sends).
    pub gain: f32,
    /// The post-filter send into the environment bus: the owner's second level (ped out15…, skater
    /// out10…; recomp send module `+0x7D0`) × the voice float.
    pub send: f32,
    /// The pre-gain send into the stream slot's echo submix ([`PED_SEND_A`] / [`SKATER_SEND_A`];
    /// recomp send module `+0x570`) × the voice float, and the echo's filters (Hz).
    pub echo: f32,
    pub echo_hpf: f32,
    pub echo_lpf: f32,
    /// The echo delay (s) when it is due ([`SpeechVoiceTuning::delay`]); None = unchanged.
    pub delay: Option<f32>,
    /// The echo slot (the channel's stream: channel × 2 + k), set by the player.
    pub slot: u8,
    pub pitch: f32,
    /// Degrees.
    pub azimuth: f32,
    pub hpf: f32,
    pub lpf: f32,
    /// The PEAK filter (centre Hz, linear gain, Q) ([`SpeechVoiceTuning::peak`]).
    pub peak: [f32; 3],
}

#[allow(clippy::too_many_arguments)]
fn params(out: &dyn Outputs, ids: (usize, usize), send_a: usize, filters: [usize; 2], echo_filters: [usize; 2], voice: &SpeechVoiceTuning, scale: f32) -> VoiceParams {
    let level = out.level(ids.0).clamp(0, 32767);
    VoiceParams {
        level,
        gain: level as f32 * INV_32767 * scale,
        send: out.level(ids.1).clamp(0, 32767) as f32 * INV_32767 * scale,
        echo: out.level(send_a).clamp(0, 32767) as f32 * INV_32767 * scale,
        echo_hpf: out.level(echo_filters[0]) as f32,
        echo_lpf: out.level(echo_filters[1]) as f32,
        delay: None,
        slot: 0,
        pitch: out.pitch(1).max(1) as f32 * INV_4096,
        azimuth: out.raw(0) as f32 * DEGREES,
        hpf: out.level(filters[0]) as f32,
        lpf: out.level(filters[1]) as f32,
        peak: voice.peak(out.raw(0)),
    }
}

/// A ped speaker's values (`out` = a PedestrianSpeech snapshot with [`PED_SNAPSHOT_FILTERS`] read
/// as filters; `scale` = the speaker's per-voice float).
pub fn ped_outputs(out: &dyn Outputs, sel: PedLevelSelect, event: u16, far: bool, voice: &SpeechVoiceTuning, scale: f32) -> VoiceParams {
    params(out, ped_level_ids(sel, event, far), PED_SEND_A, PED_FILTERS, PED_ECHO_FILTERS, voice, scale)
}

/// A skater speaker's values (`out` = a PlayerSpeech snapshot with [`SKATER_SNAPSHOT_FILTERS`]).
pub fn skater_outputs(out: &dyn Outputs, model: u32, far: bool, voice: &SpeechVoiceTuning, scale: f32) -> VoiceParams {
    params(out, skater_level_ids(model, far), SKATER_SEND_A, SKATER_FILTERS, SKATER_ECHO_FILTERS, voice, scale)
}

/// `sub_824A89E8`: the clip is a far line (its name ends in `_f`).
pub fn far_clip(name: &str) -> bool {
    name.strip_suffix(".dat").unwrap_or(name).ends_with("_f")
}

/// What plays the takes (the host's mixer and decoded speech).
pub trait SpeechVoices {
    /// Start `line`'s take with these values; None when it cannot play (no decoded take).
    fn open(&mut self, line: Line, p: &VoiceParams) -> Option<u32>;
    fn set(&mut self, voice: u32, p: &VoiceParams);
    fn alive(&self, voice: u32) -> bool;
    fn stop(&mut self, voice: u32);
}

/// A line the manager started for a speaker.
#[derive(Clone, Debug, PartialEq)]
pub struct Request {
    /// The speaker (the owner id the host knows it by).
    pub speaker: u64,
    pub event: u16,
    /// The event's tuning `+16` and interrupt bytes `+13` / `+14`.
    pub priority: i32,
    pub interrupt: bool,
    pub interrupt_when_full: bool,
    /// The record's clips in order.
    pub lines: Vec<Line>,
    /// The `.evt` queue timeout (console frames, provisional).
    pub timeout: u32,
}

#[derive(Clone, Debug)]
struct Stream {
    req: Request,
    at: usize,
    voice: Option<u32>,
    low: u32,
}

/// What [`SpeechPlayer::request`] did.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Outcome {
    /// On stream `k`.
    Playing(usize),
    /// On stream `k`, stopping the lower-priority line there.
    Interrupted(usize),
    Queued,
    /// The queue was full.
    Dropped,
}

/// Something that happened during [`SpeechPlayer::frame`] (logs, the summary counts, hooks).
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Event {
    /// A take started on stream `k`.
    Started { k: usize, speaker: u64, line: Line },
    /// The line ended (its last take finished).
    Finished { k: usize, speaker: u64 },
    /// The cut stopped the line (the speaker's level stayed low).
    Cut { k: usize, speaker: u64 },
    /// The speaker went away (lost its instance).
    Gone { k: usize, speaker: u64 },
    /// A queued request timed out.
    Expired { speaker: u64 },
}

/// One speech channel's streams and queue.
#[derive(Clone, Debug, Default)]
pub struct SpeechPlayer {
    /// The speech channel (0 = the main cast, 1 = the living world): its streams' echo slots are
    /// channel × 2 + k.
    pub channel: u8,
    streams: [Option<Stream>; STREAMS],
    queue: VecDeque<(Request, u32)>,
    /// Counters (diagnostics).
    pub started: u64,
    pub interrupted: u64,
    pub dropped: u64,
    pub cut: u64,
}

impl SpeechPlayer {
    /// A player for speech channel `channel`.
    pub fn on_channel(channel: u8) -> Self {
        Self { channel, ..Default::default() }
    }

    /// The speakers whose lines play now.
    pub fn speakers(&self) -> impl Iterator<Item = u64> + '_ {
        self.streams.iter().flatten().map(|s| s.req.speaker)
    }

    /// Whether `speaker` has a line playing (on any stream).
    pub fn speaking(&self, speaker: u64) -> bool {
        self.speakers().any(|s| s == speaker)
    }

    pub fn busy(&self) -> usize {
        self.streams.iter().flatten().count()
    }

    pub fn queued(&self) -> usize {
        self.queue.len()
    }

    /// A line the manager started (see the module docs for the stream rule).
    pub fn request(&mut self, req: Request, voices: &mut dyn SpeechVoices) -> Outcome {
        if let Some(k) = self.streams.iter().position(Option::is_none) {
            self.streams[k] = Some(Stream { req, at: 0, voice: None, low: 0 });
            return Outcome::Playing(k);
        }
        // Full: the lower-priority playing line, if the event may interrupt it.
        let (k, lowest) = self
            .streams
            .iter()
            .enumerate()
            .filter_map(|(k, s)| s.as_ref().map(|s| (k, s.req.priority)))
            .min_by_key(|&(k, p)| (p, k))
            .expect("full");
        if (req.interrupt || req.interrupt_when_full) && lowest < req.priority {
            if let Some(old) = self.streams[k].take()
                && let Some(v) = old.voice
            {
                voices.stop(v);
            }
            self.streams[k] = Some(Stream { req, at: 0, voice: None, low: 0 });
            self.interrupted += 1;
            return Outcome::Interrupted(k);
        }
        if self.queue.len() >= QUEUE {
            self.dropped += 1;
            return Outcome::Dropped;
        }
        self.queue.push_back((req, 0));
        Outcome::Queued
    }

    /// Stop every line and forget the queue (a map change, the speech going off).
    pub fn clear(&mut self, voices: &mut dyn SpeechVoices) {
        for s in self.streams.iter_mut() {
            if let Some(v) = s.take().and_then(|s| s.voice) {
                voices.stop(v);
            }
        }
        self.queue.clear();
    }

    /// One console frame: the queue, then every stream: its speaker's values (`speaker(id, far,
    /// event)`, None = the speaker is gone), the cut, the next take of the line, the voice values.
    pub fn frame(&mut self, index: &SpeechIndex, speaker: &mut dyn FnMut(u64, bool, u16) -> Option<VoiceParams>, voices: &mut dyn SpeechVoices) -> Vec<Event> {
        let mut events = Vec::new();
        // The queue: age, expire, then fill free streams (highest priority first, then the newest).
        for q in self.queue.iter_mut() {
            q.1 += 1;
        }
        self.queue.retain(|(r, age)| {
            let keep = *age <= r.timeout;
            if !keep {
                events.push(Event::Expired { speaker: r.speaker });
            }
            keep
        });
        while let Some(k) = self.streams.iter().position(Option::is_none) {
            let Some(best) = self.queue.iter().enumerate().max_by_key(|(i, (r, _))| (r.priority, *i)).map(|(i, _)| i) else { break };
            let (req, _) = self.queue.remove(best).expect("index");
            self.streams[k] = Some(Stream { req, at: 0, voice: None, low: 0 });
        }
        for k in 0..STREAMS {
            let Some(mut s) = self.streams[k].take() else { continue };
            let line = s.req.lines[s.at];
            let far = index.clips.get(line.clip).is_some_and(|c| far_clip(&c.name));
            let slot = self.channel * 2 + k as u8;
            let Some(mut p) = speaker(s.req.speaker, far, s.req.event) else {
                if let Some(v) = s.voice {
                    voices.stop(v);
                }
                events.push(Event::Gone { k, speaker: s.req.speaker });
                continue;
            };
            // The cut.
            if p.level <= CUT_LEVEL {
                s.low += 1;
                if s.low > CUT_FRAMES {
                    if let Some(v) = s.voice {
                        voices.stop(v);
                    }
                    self.cut += 1;
                    events.push(Event::Cut { k, speaker: s.req.speaker });
                    continue;
                }
            } else {
                s.low = 0;
            }
            p.slot = slot;
            match s.voice {
                Some(v) if voices.alive(v) => voices.set(v, &p),
                Some(v) => {
                    voices.stop(v);
                    s.voice = None;
                    s.at += 1;
                    if s.at >= s.req.lines.len() {
                        events.push(Event::Finished { k, speaker: s.req.speaker });
                        continue;
                    }
                    let line = s.req.lines[s.at];
                    let far = index.clips.get(line.clip).is_some_and(|c| far_clip(&c.name));
                    let mut p = speaker(s.req.speaker, far, s.req.event).unwrap_or(p);
                    p.slot = slot;
                    s.voice = voices.open(line, &p);
                    if s.voice.is_some() {
                        events.push(Event::Started { k, speaker: s.req.speaker, line });
                    }
                }
                None => {
                    s.voice = voices.open(line, &p);
                    match s.voice {
                        Some(_) => {
                            self.started += 1;
                            events.push(Event::Started { k, speaker: s.req.speaker, line });
                        }
                        None => {
                            // Not playable (the take is not decoded): the line ends.
                            events.push(Event::Finished { k, speaker: s.req.speaker });
                            continue;
                        }
                    }
                }
            }
            self.streams[k] = Some(s);
        }
        events
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::world::speech::{Clip, Take};

    struct Out(i32);
    impl Outputs for Out {
        fn level(&self, id: usize) -> i32 {
            match id {
                2 => self.0,
                3 => 2903,
                13 => 77,
                14 => 24956,
                15 => 1196,
                _ => 100 * id as i32,
            }
        }
        fn raw(&self, _: usize) -> i32 {
            16384
        }
        fn pitch(&self, _: usize) -> i32 {
            4086
        }
    }

    #[derive(Default)]
    struct Voices {
        live: Vec<u32>,
        opened: Vec<Line>,
        next: u32,
        last: Option<VoiceParams>,
    }
    impl SpeechVoices for Voices {
        fn open(&mut self, line: Line, p: &VoiceParams) -> Option<u32> {
            self.next += 1;
            self.live.push(self.next);
            self.opened.push(line);
            self.last = Some(*p);
            Some(self.next)
        }
        fn set(&mut self, _: u32, p: &VoiceParams) {
            self.last = Some(*p);
        }
        fn alive(&self, v: u32) -> bool {
            self.live.contains(&v)
        }
        fn stop(&mut self, v: u32) {
            self.live.retain(|x| *x != v);
        }
    }

    fn index() -> SpeechIndex {
        let take = Take { offset: 0, size: 1, rate: 36000, samples: 1 };
        SpeechIndex::new(vec![
            Clip { name: "501_59_busm1_Warn_n.dat".into(), event: 501, voice: 59, voice_name: Some("busm1".into()), line: "Warn_n".into(), takes: vec![take; 2] },
            Clip { name: "501_59_busm1_Warn_f.dat".into(), event: 501, voice: 59, voice_name: Some("busm1".into()), line: "Warn_f".into(), takes: vec![take; 2] },
        ])
    }

    fn req(speaker: u64, clip: usize, priority: i32, interrupt: bool) -> Request {
        Request { speaker, event: 8210, priority, interrupt, interrupt_when_full: false, lines: vec![Line { clip, take: 0, event: 501 }], timeout: 3 }
    }

    #[test]
    fn the_level_outputs_follow_the_speaker_and_the_far_clip() {
        assert_eq!(ped_level_ids(PedLevelSelect::default(), 8210, false), (2, 15));
        assert_eq!(ped_level_ids(PedLevelSelect::default(), 8210, true), (3, 15));
        let guard = PedLevelSelect { security: true, ..Default::default() };
        assert_eq!(ped_level_ids(guard, 8210, true), (5, 16));
        assert_eq!(ped_level_ids(guard, 8277, false), (6, 17), "the guard's radio");
        assert_eq!(skater_level_ids(91, false), (2, 10));
        assert_eq!(skater_level_ids(12, true), (5, 12));
        assert!(far_clip("501_59_busm1_Warn_f.dat") && !far_clip("101_51_GenPos_Grn1_far.dat") && !far_clip("501_59_busm1_Warn_n.dat"));
        let p = ped_outputs(&Out(9804), PedLevelSelect::default(), 8210, false, &SpeechVoiceTuning::default(), 1.0);
        assert_eq!((p.level, p.hpf, p.lpf), (9804, 77.0, 24956.0));
        // The env send = out15 (post-filter), the echo send = out21 (pre-gain).
        assert!((p.send - 1196.0 / 32767.0).abs() < 1e-6 && (p.echo - 2100.0 / 32767.0).abs() < 1e-6 && (p.pitch - 4086.0 / 4096.0).abs() < 1e-6);
        assert!((p.azimuth - 16384.0 * 360.0 / 65535.0).abs() < 1e-2);
        // Raw 16384 = just past the side: the PEAK curves' 4000 Hz / 0.222 region.
        assert_eq!(p.peak, SpeechVoiceTuning::default().peak(16384));
        assert!((p.peak[0] - 3867.5).abs() < 1.0 && (p.peak[1] - 0.2198).abs() < 1e-3 && p.peak[2] == 3.0, "{:?}", p.peak);
        assert_eq!(ped_outputs(&Out(9804), PedLevelSelect::default(), 8210, true, &SpeechVoiceTuning::default(), 1.0).level, 2903);
    }

    #[test]
    fn the_echo_delay_is_the_sound_travel_time_up_to_150_ms() {
        let t = SpeechVoiceTuning::default();
        assert!((t.delay(34.4) - 0.1).abs() < 1e-5);
        assert_eq!(t.delay(100.0), t.max_delay);
        assert!((t.max_delay - 0.15).abs() < 1e-7);
        assert_eq!(t.delay(0.0), 0.0);
    }

    #[test]
    fn the_peak_follows_the_folded_azimuth_on_the_curves() {
        let t = SpeechVoiceTuning::default();
        // Recomp 163809 at 69.0 s: PEAK 1244.58 Hz, gain 0.3588, Q 3, with our owner's raw azimuth
        // 62673 at that line's geometry (folded: 2863).
        let p = t.peak(62673);
        assert!((p[0] - 1244.58).abs() < 1.5 && (p[1] - 0.3588).abs() < 2e-4 && p[2] == 3.0, "{p:?}");
        assert_eq!(t.peak(65536 - 5000), t.peak(5000), "front / back fold");
        assert_eq!(t.peak(0), [600.0, 0.4, 3.0]);
        assert_eq!(t.peak(32767), [600.0, 0.1, 3.0]);
    }

    #[test]
    fn two_streams_interrupts_queue_and_cut() {
        let ix = index();
        let mut p = SpeechPlayer::default();
        let mut v = Voices::default();
        assert_eq!(p.request(req(1, 0, 500, false), &mut v), Outcome::Playing(0));
        assert_eq!(p.request(req(2, 1, 520, false), &mut v), Outcome::Playing(1));
        assert_eq!(p.request(req(3, 0, 510, false), &mut v), Outcome::Queued, "no interrupt byte: it waits");
        assert_eq!(p.request(req(4, 0, 510, true), &mut v), Outcome::Interrupted(0), "beats the 500 line");
        let mut level = 9000;
        let ev = p.frame(&ix, &mut |_, _, _| Some(ped_outputs(&Out(level), PedLevelSelect::default(), 8210, false, &SpeechVoiceTuning::default(), 1.0)), &mut v);
        assert_eq!(ev.iter().filter(|e| matches!(e, Event::Started { .. })).count(), 2);
        assert_eq!(v.opened.len(), 2);
        // Speaker 2's line is far: out3.
        assert_eq!(p.speakers().collect::<Vec<_>>(), vec![4, 2]);
        // The queued request expires after its timeout (3 frames).
        for _ in 0..3 {
            p.frame(&ix, &mut |_, far, _| Some(ped_outputs(&Out(level), PedLevelSelect::default(), 8210, far, &SpeechVoiceTuning::default(), 1.0)), &mut v);
        }
        assert_eq!(p.queued(), 0);
        // The cut: 61 frames at or below 200.
        level = 150;
        let mut cut = 0;
        for _ in 0..61 {
            let ev = p.frame(&ix, &mut |s, far, _| (s == 4).then(|| ped_outputs(&Out(level), PedLevelSelect::default(), 8210, far, &SpeechVoiceTuning::default(), 1.0)), &mut v);
            cut += ev.iter().filter(|e| matches!(e, Event::Cut { .. })).count();
        }
        assert_eq!(cut, 1, "speaker 4 cut; speaker 2 went away at once");
        assert_eq!(p.busy(), 0);
        // A finished take ends the line.
        assert_eq!(p.request(req(5, 0, 500, false), &mut v), Outcome::Playing(0));
        p.frame(&ix, &mut |_, _, _| Some(ped_outputs(&Out(9000), PedLevelSelect::default(), 8210, false, &SpeechVoiceTuning::default(), 1.0)), &mut v);
        v.live.clear();
        let ev = p.frame(&ix, &mut |_, _, _| Some(ped_outputs(&Out(9000), PedLevelSelect::default(), 8210, false, &SpeechVoiceTuning::default(), 1.0)), &mut v);
        assert_eq!(ev, vec![Event::Finished { k: 0, speaker: 5 }]);
    }
}
