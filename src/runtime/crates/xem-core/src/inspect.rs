//! Structured introspection: any global of the game, decoded by name and path
//! from game memory with the schema tools/game_schema.py derives from the
//! recovered C's debug information (build/game/schema.json).

use serde::Deserialize;
use serde_json::{Map, Value, json};
use std::collections::HashMap;

use crate::memory::GameMemory;

#[derive(Debug, Deserialize)]
pub struct Global {
    pub address: u32,
    pub image: Option<String>,
    #[serde(rename = "type")]
    pub ty: Option<usize>,
}

#[derive(Debug, Deserialize)]
pub struct Member {
    pub name: Option<String>,
    pub offset: u32,
    #[serde(rename = "type")]
    pub ty: Option<usize>,
    pub bit_size: Option<u32>,
    pub bit_offset: Option<u32>,
}

#[derive(Debug, Deserialize)]
#[serde(tag = "kind", rename_all = "lowercase")]
pub enum Type {
    Base { name: String, size: u32, encoding: String },
    Typedef { name: String, #[serde(rename = "type")] ty: Option<usize> },
    Pointer { to: Option<serde_json::Value> },
    Array { of: Option<usize>, count: u32 },
    Struct { name: Option<String>, size: u32, members: Vec<Member> },
    Union { name: Option<String>, size: u32, members: Vec<Member> },
    Enum { name: Option<String>, size: u32, values: HashMap<String, i64> },
    Function {},
    Opaque { name: Option<String> },
}

#[derive(Debug, Deserialize)]
pub struct Schema {
    pub globals: HashMap<String, Global>,
    pub types: Vec<Type>,
    #[serde(default)]
    pub records: HashMap<String, usize>,
}

/// A located value: where it is and its type (None: untyped raw word).
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Place {
    pub address: u32,
    pub ty: Option<usize>,
}

#[derive(Debug)]
pub enum InspectError {
    Unknown(String),
    Path(String),
    Memory(crate::memory::OutOfBounds),
}

impl std::fmt::Display for InspectError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            InspectError::Unknown(name) => write!(f, "no global named {name}"),
            InspectError::Path(why) => write!(f, "bad path: {why}"),
            InspectError::Memory(e) => write!(f, "{e}"),
        }
    }
}

impl std::error::Error for InspectError {}

impl From<crate::memory::OutOfBounds> for InspectError {
    fn from(e: crate::memory::OutOfBounds) -> Self {
        InspectError::Memory(e)
    }
}

impl Schema {
    pub fn parse(json: &str) -> Result<Self, serde_json::Error> {
        serde_json::from_str(json)
    }

    /// Strip typedefs.
    fn resolve(&self, ty: Option<usize>) -> Option<&Type> {
        let mut ty = ty?;
        loop {
            match &self.types[ty] {
                Type::Typedef { ty: Some(inner), .. } => ty = *inner,
                Type::Typedef { ty: None, .. } => return None,
                other => return Some(other),
            }
        }
    }

    pub fn size_of(&self, ty: Option<usize>) -> u32 {
        match self.resolve(ty) {
            Some(Type::Base { size, .. }) | Some(Type::Struct { size, .. }) | Some(Type::Union { size, .. }) => *size,
            Some(Type::Enum { size, .. }) => *size,
            Some(Type::Pointer { .. }) => 4,
            Some(Type::Array { of, count }) => self.size_of(*of) * count,
            _ => 4,
        }
    }

    /// Locate `name.member[3].field`, following pointers with `->`.
    pub fn locate(&self, path: &str, memory: &dyn GameMemory) -> Result<Place, InspectError> {
        let (head, mut rest) = split_head(path);
        let global = self.globals.get(head).ok_or_else(|| InspectError::Unknown(head.to_string()))?;
        let mut place = Place { address: global.address, ty: global.ty };
        while !rest.is_empty() {
            if let Some(after) = rest.strip_prefix('[') {
                let end = after.find(']').ok_or_else(|| InspectError::Path("missing ]".into()))?;
                let index: u32 = after[..end].trim().parse().map_err(|_| InspectError::Path("index".into()))?;
                let Some(Type::Array { of, count }) = self.resolve(place.ty) else {
                    return Err(InspectError::Path(format!("not an array before [{index}]")));
                };
                if *count != 0 && index >= *count {
                    return Err(InspectError::Path(format!("index {index} >= {count}")));
                }
                place = Place { address: place.address + index * self.size_of(*of), ty: *of };
                rest = &after[end + 1..];
            } else if let Some(after) = rest.strip_prefix("->") {
                let Some(Type::Pointer { to }) = self.resolve(place.ty) else {
                    return Err(InspectError::Path("-> on a non-pointer".into()));
                };
                let target = Place { address: memory.read_u32(place.address)?, ty: self.pointee(to) };
                let (field, tail) = split_head(after);
                place = self.member(target, field)?;
                rest = tail;
            } else if let Some(after) = rest.strip_prefix('.') {
                let (field, tail) = split_head(after);
                place = self.member(place, field)?;
                rest = tail;
            } else {
                return Err(InspectError::Path(format!("unexpected {rest}")));
            }
        }
        Ok(place)
    }

    fn pointee(&self, to: &Option<serde_json::Value>) -> Option<usize> {
        match to {
            Some(serde_json::Value::Number(n)) => n.as_u64().map(|n| n as usize),
            Some(serde_json::Value::String(s)) => s.strip_prefix("record ").and_then(|name| self.records.get(name).copied()),
            _ => None,
        }
    }

    fn member(&self, place: Place, field: &str) -> Result<Place, InspectError> {
        let members = match self.resolve(place.ty) {
            Some(Type::Struct { members, .. }) | Some(Type::Union { members, .. }) => members,
            _ => return Err(InspectError::Path(format!(".{field} on a non-record"))),
        };
        let member = members
            .iter()
            .find(|m| m.name.as_deref() == Some(field))
            .ok_or_else(|| InspectError::Path(format!("no member {field}")))?;
        Ok(Place { address: place.address + member.offset, ty: member.ty })
    }

    /// Decode a place as JSON, expanding records and arrays down to `depth`
    /// levels (arrays longer than `max_elements` are cut).
    pub fn decode(&self, place: Place, memory: &dyn GameMemory, depth: u32) -> Result<Value, InspectError> {
        const MAX_ELEMENTS: u32 = 256;
        Ok(match self.resolve(place.ty) {
            None => json!(memory.read_u32(place.address)?),
            Some(Type::Base { size, encoding, .. }) => read_int(memory, place.address, *size, encoding == "signed")?,
            Some(Type::Enum { size, values, .. }) => {
                let raw = read_int(memory, place.address, *size, true)?;
                let n = raw.as_i64().unwrap_or(0);
                match values.iter().find(|(_, v)| **v == n) {
                    Some((name, _)) => json!(name),
                    None => raw,
                }
            }
            Some(Type::Pointer { .. }) => json!(format!("{:#010x}", memory.read_u32(place.address)?)),
            Some(Type::Function {}) | Some(Type::Opaque { .. }) | Some(Type::Typedef { .. }) => {
                json!(format!("@{:#010x}", place.address))
            }
            Some(Type::Array { of, count }) => {
                if depth == 0 {
                    return Ok(json!(format!("[{count}] @{:#010x}", place.address)));
                }
                let element = self.size_of(*of);
                let mut items = Vec::new();
                for i in 0..(*count).min(MAX_ELEMENTS) {
                    items.push(self.decode(Place { address: place.address + i * element, ty: *of }, memory, depth - 1)?);
                }
                Value::Array(items)
            }
            Some(Type::Struct { members, .. }) | Some(Type::Union { members, .. }) => {
                if depth == 0 {
                    return Ok(json!(format!("{{..}} @{:#010x}", place.address)));
                }
                let mut map = Map::new();
                for (i, m) in members.iter().enumerate() {
                    let name = m.name.clone().unwrap_or_else(|| format!("_{i}"));
                    let value = if let Some(bits) = m.bit_size {
                        let word = memory.read_u32(place.address + m.offset)?;
                        let shift = m.bit_offset.unwrap_or(0) % 32;
                        json!((word >> shift) & ((1u64 << bits) - 1) as u32)
                    } else {
                        self.decode(Place { address: place.address + m.offset, ty: m.ty }, memory, depth - 1)?
                    };
                    map.insert(name, value);
                }
                Value::Object(map)
            }
        })
    }
}

fn split_head(path: &str) -> (&str, &str) {
    let end = path.find(['.', '[', '-']).unwrap_or(path.len());
    (&path[..end], &path[end..])
}

fn read_int(memory: &dyn GameMemory, address: u32, size: u32, signed: bool) -> Result<Value, InspectError> {
    let mut bytes = [0u8; 8];
    let size = size.clamp(1, 8) as usize;
    memory.read(address, &mut bytes[..size])?;
    let raw = u64::from_le_bytes(bytes);
    Ok(if signed {
        let shift = 64 - size * 8;
        json!(((raw << shift) as i64) >> shift)
    } else {
        json!(raw)
    })
}
