//! Direct typed commands over the runtime, for humans, agents, tests and the
//! browser alike: one JSON command in, one JSON result out. Nothing here
//! simulates device input below the game's own interfaces; pads are set as the
//! BIOS reports them, and state is read and written through the schema.

use std::collections::HashMap;

use serde_json::{Value, json};

use crate::inspect::{InspectError, Schema, Type};
use crate::module::{GameModule, Trap};
use crate::pad::button;
use crate::runtime::{RESTART_DISPATCH, Runtime};

/// Pad states per vertical blank from where a recording started.
#[derive(Debug, Clone, Default)]
pub struct InputLog {
    pub start_vblank: u64,
    /// (vblank, port, buttons) whenever a pad changed.
    pub changes: Vec<(u64, u32, u16)>,
}

pub struct Session<M: GameModule> {
    pub runtime: Runtime<M>,
    pub schema: Option<Schema>,
    snapshots: HashMap<String, Vec<u8>>,
    recording: Option<InputLog>,
    /// Scheduled pad changes (vblank, port, buttons), applied before the frame
    /// that reaches the vertical blank.
    scheduled: Vec<(u64, u32, u16)>,
    /// The last trap; the game stays stopped after one.
    pub stopped: Option<String>,
}

const BUTTONS: [(&str, u16); 16] = [
    ("select", button::SELECT),
    ("l3", button::L3),
    ("r3", button::R3),
    ("start", button::START),
    ("up", button::UP),
    ("right", button::RIGHT),
    ("down", button::DOWN),
    ("left", button::LEFT),
    ("l2", button::L2),
    ("r2", button::R2),
    ("l1", button::L1),
    ("r1", button::R1),
    ("triangle", button::TRIANGLE),
    ("circle", button::CIRCLE),
    ("cross", button::CROSS),
    ("square", button::SQUARE),
];

fn buttons_of(value: &Value) -> Result<u16, String> {
    let mut bits = 0;
    for name in value.as_array().ok_or("buttons must be a list of names")? {
        let name = name.as_str().ok_or("button names are strings")?;
        bits |= BUTTONS.iter().find(|(n, _)| *n == name).ok_or_else(|| format!("unknown button {name}"))?.1;
    }
    Ok(bits)
}

fn names_of(bits: u16) -> Vec<&'static str> {
    BUTTONS.iter().filter(|(_, b)| bits & b != 0).map(|(n, _)| *n).collect()
}

impl<M: GameModule> Session<M> {
    pub fn new(runtime: Runtime<M>, schema: Option<Schema>) -> Self {
        Session { runtime, schema, snapshots: HashMap::new(), recording: None, scheduled: Vec::new(), stopped: None }
    }

    fn vblank(&mut self) -> u64 {
        self.runtime.services().clock.vblanks
    }

    fn set_pad(&mut self, port: u32, buttons: u16) {
        let vblank = self.vblank();
        let pads = &mut self.runtime.services().pads.0;
        if let Some(pad) = pads.get_mut(port as usize) {
            pad.buttons = buttons;
        }
        if let Some(log) = &mut self.recording {
            log.changes.push((vblank, port, buttons));
        }
    }

    /// Run one frame (to the next vertical blank), applying scheduled input.
    pub fn frame(&mut self) -> Result<(), Trap> {
        if let Some(reason) = &self.stopped {
            return Err(Trap::Host(reason.clone()));
        }
        let vblank = self.vblank();
        let due: Vec<_> = self.scheduled.iter().filter(|(at, _, _)| *at <= vblank).copied().collect();
        self.scheduled.retain(|(at, _, _)| *at > vblank);
        for (_, port, buttons) in due {
            self.set_pad(port, buttons);
        }
        match self.runtime.run_frame() {
            Ok(_) => Ok(()),
            Err(trap) => {
                self.stopped = Some(trap.to_string());
                Err(trap)
            }
        }
    }

    fn schema(&self) -> Result<&Schema, String> {
        self.schema.as_ref().ok_or_else(|| "no schema (build/game/schema.json) loaded".to_string())
    }

    fn inspect(&mut self, path: &str, depth: u32) -> Result<Value, String> {
        let schema = self.schema.as_ref().ok_or("no schema loaded")?;
        let memory = self.runtime.memory();
        let place = schema.locate(path, memory).map_err(|e: InspectError| e.to_string())?;
        schema.decode(place, memory, depth).map_err(|e| e.to_string())
    }

    /// Write a scalar at a path (integers, enums by name, pointers as numbers).
    fn write_path(&mut self, path: &str, value: &Value) -> Result<(), String> {
        let schema = self.schema.as_ref().ok_or("no schema loaded")?;
        let memory = self.runtime.memory();
        let place = schema.locate(path, memory).map_err(|e| e.to_string())?;
        let size = schema.size_of(place.ty) as usize;
        let mut ty = place.ty;
        while let Some(Type::Typedef { ty: inner, .. }) = ty.map(|t| &schema.types[t]) {
            ty = *inner;
        }
        let number = match (ty.map(|t| &schema.types[t]), value) {
            (Some(Type::Enum { values, .. }), Value::String(name)) => {
                *values.get(name).ok_or_else(|| format!("no enumerator {name}"))?
            }
            (Some(Type::Struct { .. }) | Some(Type::Union { .. }) | Some(Type::Array { .. }), _) => {
                return Err("write scalars, member by member".into());
            }
            (_, v) => v.as_i64().or_else(|| v.as_u64().map(|u| u as i64)).ok_or("value must be an integer")?,
        };
        let bytes = number.to_le_bytes();
        memory.write(place.address, &bytes[..size.min(8)]).map_err(|e| e.to_string())
    }

    fn condition_met(&mut self, until: &Value) -> Result<bool, String> {
        if let Some(n) = until.get("vblanks").and_then(Value::as_u64) {
            return Ok(self.vblank() >= n);
        }
        if let Some(path) = until.get("path").and_then(Value::as_str) {
            let value = self.inspect(path, 0)?;
            if let Some(expected) = until.get("equals") {
                return Ok(&value == expected);
            }
            if let Some(expected) = until.get("not_equals") {
                return Ok(&value != expected);
            }
            return Err("a path condition needs equals or not_equals".into());
        }
        Err("until needs vblanks or path".into())
    }

    /// Execute one command; errors come back as {"error": ...}.
    pub fn execute(&mut self, command: &Value) -> Value {
        match self.try_execute(command) {
            Ok(value) => value,
            Err(error) => json!({ "error": error }),
        }
    }

    fn status(&mut self) -> Value {
        let stopped = self.stopped.clone();
        let services = self.runtime.services();
        json!({
            "vblanks": services.clock.vblanks,
            "cycles": services.clock.now,
            "pads": services.pads.0.iter().map(|p| json!({"connected": p.connected, "buttons": names_of(p.buttons)})).collect::<Vec<_>>(),
            "stopped": stopped,
            "recording": self.recording.is_some(),
            "snapshots": self.snapshots.keys().cloned().collect::<Vec<_>>(),
        })
    }

    fn try_execute(&mut self, command: &Value) -> Result<Value, String> {
        let cmd = command.get("cmd").and_then(Value::as_str).ok_or("missing cmd")?;
        let arg_u64 = |name: &str| command.get(name).and_then(Value::as_u64);
        let arg_str = |name: &str| command.get(name).and_then(Value::as_str);
        match cmd {
            "status" => Ok(self.status()),
            "frames" => {
                let n = arg_u64("count").unwrap_or(1);
                for _ in 0..n {
                    self.frame().map_err(|t| t.to_string())?;
                }
                Ok(self.status())
            }
            "run_until" => {
                let max = arg_u64("max_frames").ok_or("run_until needs max_frames (it is bounded)")?;
                let until = command.get("until").ok_or("run_until needs until")?;
                for frame in 0..max {
                    if self.condition_met(until)? {
                        return Ok(json!({ "met": true, "frames": frame, "status": self.status() }));
                    }
                    self.frame().map_err(|t| t.to_string())?;
                }
                let met = self.condition_met(until)?;
                Ok(json!({ "met": met, "frames": max, "status": self.status() }))
            }
            "pad" => {
                let port = arg_u64("port").unwrap_or(0) as u32;
                let buttons = buttons_of(command.get("buttons").ok_or("pad needs buttons")?)?;
                self.set_pad(port, buttons);
                Ok(self.status())
            }
            "press" => {
                // Hold the buttons for `frames` frames from now, then release.
                let port = arg_u64("port").unwrap_or(0) as u32;
                let buttons = buttons_of(command.get("buttons").ok_or("press needs buttons")?)?;
                let frames = arg_u64("frames").unwrap_or(1);
                let now = self.vblank();
                self.set_pad(port, buttons);
                self.scheduled.push((now + frames, port, 0));
                Ok(self.status())
            }
            "connect_pad" => {
                let port = arg_u64("port").ok_or("connect_pad needs port")? as usize;
                let connected = command.get("connected").and_then(Value::as_bool).unwrap_or(true);
                let pads = &mut self.runtime.services().pads.0;
                pads.get_mut(port).ok_or("port is 0 or 1")?.connected = connected;
                Ok(self.status())
            }
            "inspect" => {
                let path = arg_str("path").ok_or("inspect needs path")?;
                let depth = arg_u64("depth").unwrap_or(2) as u32;
                self.inspect(path, depth)
            }
            "write" => {
                let path = arg_str("path").ok_or("write needs path")?;
                self.write_path(path, command.get("value").ok_or("write needs value")?)?;
                self.inspect(path, 1)
            }
            "globals" => {
                let prefix = arg_str("prefix").unwrap_or("");
                let schema = self.schema()?;
                let mut names: Vec<_> = schema.globals.keys().filter(|n| n.starts_with(prefix)).cloned().collect();
                names.sort();
                Ok(json!(names))
            }
            "read_memory" => {
                let address = arg_u64("address").ok_or("read_memory needs address")? as u32;
                let length = arg_u64("length").unwrap_or(16).min(0x10000) as usize;
                let mut data = vec![0; length];
                self.runtime.memory().read(address, &mut data).map_err(|e| e.to_string())?;
                Ok(json!(hex(&data)))
            }
            "write_memory" => {
                let address = arg_u64("address").ok_or("write_memory needs address")? as u32;
                let data = unhex(arg_str("hex").ok_or("write_memory needs hex")?)?;
                self.runtime.memory().write(address, &data).map_err(|e| e.to_string())?;
                Ok(json!({ "written": data.len() }))
            }
            "snapshot" => {
                let name = arg_str("name").unwrap_or("default").to_string();
                let data = self.runtime.snapshot().map_err(|e| e.to_string())?;
                let size = data.len();
                self.snapshots.insert(name.clone(), data);
                Ok(json!({ "name": name, "bytes": size }))
            }
            "restore" => {
                let name = arg_str("name").unwrap_or("default");
                let data = self.snapshots.get(name).ok_or_else(|| format!("no snapshot {name}"))?.clone();
                self.restore_bytes(&data)?;
                Ok(self.status())
            }
            "memory_hash" => Ok(json!(format!("{:016x}", self.memory_hash()?))),
            "record" => {
                let vblank = self.vblank();
                self.recording = Some(InputLog { start_vblank: vblank, changes: Vec::new() });
                Ok(self.status())
            }
            "stop_recording" => {
                let log = self.recording.take().ok_or("not recording")?;
                Ok(json!({
                    "start_vblank": log.start_vblank,
                    "changes": log.changes.iter().map(|(v, p, b)| json!([v, p, b])).collect::<Vec<_>>(),
                }))
            }
            "replay" => {
                // Schedule a recorded input log (from stop_recording) relative to now.
                let changes = command.get("changes").and_then(Value::as_array).ok_or("replay needs changes")?;
                let start = command.get("start_vblank").and_then(Value::as_u64).unwrap_or(0);
                let now = self.vblank();
                for change in changes {
                    let c = change.as_array().ok_or("changes are [vblank, port, buttons]")?;
                    let at = c.first().and_then(Value::as_u64).ok_or("vblank")?;
                    let port = c.get(1).and_then(Value::as_u64).ok_or("port")? as u32;
                    let buttons = c.get(2).and_then(Value::as_u64).ok_or("buttons")? as u16;
                    self.scheduled.push((now + at.saturating_sub(start), port, buttons));
                }
                Ok(self.status())
            }
            "scenario" => {
                // Typed go-anywhere setup: write values through the schema, then
                // enter a mode through the original dispatcher on an empty stack.
                for write in command.get("writes").and_then(Value::as_array).into_iter().flatten() {
                    let path = write.get("path").and_then(Value::as_str).ok_or("writes need path")?;
                    self.write_path(path, write.get("value").ok_or("writes need value")?)?;
                }
                if let Some(mode) = arg_u64("mode") {
                    self.write_path("mode_next_mode", &json!(mode))?;
                    self.runtime.restart(RESTART_DISPATCH, 0);
                    self.stopped = None;
                }
                Ok(self.status())
            }
            other => Err(format!("unknown command {other}")),
        }
    }

    /// Return to a snapshot (`Runtime::snapshot` bytes, e.g. a saved file),
    /// as `restore` does for a named one.
    pub fn restore_bytes(&mut self, data: &[u8]) -> Result<(), String> {
        self.runtime.restore(data).map_err(|e| e.to_string())?;
        self.stopped = None;
        self.scheduled.clear();
        Ok(())
    }

    /// FNV-1a over the PS1 RAM, the scratchpad and the I/O pages: the
    /// authoritative game state (comparable across hosts and runs).
    pub fn memory_hash(&mut self) -> Result<u64, String> {
        let mut h: u64 = 0xCBF2_9CE4_8422_2325;
        for (start, len) in [(0x8000_0000u32, 0x20_0000u32), (0x1F80_0000, 0x400)] {
            let mut data = vec![0; len as usize];
            self.runtime.memory().read(start, &mut data).map_err(|e| e.to_string())?;
            for b in data {
                h = (h ^ b as u64).wrapping_mul(0x100_0000_01B3);
            }
        }
        Ok(h)
    }
}

fn hex(data: &[u8]) -> String {
    data.iter().map(|b| format!("{b:02x}")).collect()
}

fn unhex(text: &str) -> Result<Vec<u8>, String> {
    if text.len() % 2 != 0 {
        return Err("odd hex length".into());
    }
    (0..text.len()).step_by(2).map(|i| u8::from_str_radix(&text[i..i + 2], 16).map_err(|e| e.to_string())).collect()
}
