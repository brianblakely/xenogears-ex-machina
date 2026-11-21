//! The field as the assisted controls see it, read from the game's memory
//! at its original addresses: the camera, the collision triangles (the
//! walkmesh the game itself walks), the event actors and the player's.
//! From these: what a screen point picks on the floor, whether the player
//! can walk there, and the route there as legal pad input (the D-pad by
//! the camera's angle, Run held for distance). Nothing is written; the
//! game moves the player by its own rules from the pad alone.
//!
//! The camera (`field_view`): `scaled_world` (+0x1E4: a 4.12 matrix and a
//! translation) takes world units to view space; the screen point is
//! `160 + x*H/z, 112 + y*H/z` (`SetGeomOffset(0xA0, 0x70)`, H the
//! projection +0x178). `field_view.angle` (+0x10C) turns the D-pad: the
//! player heads `dir - angle`, where Right is 0, Down 0x400, Left 0x800 and
//! Up 0xC00 (`field_dpad_headings`; a second table, reversed, while
//! `field_work.unk2354` is set), and moves along (cos h, -sin h) in x, z.
//!
//! The walkmesh (`FieldComponents` at 0x800AFB0C): per layer, 14-byte
//! triangles (three vertex indices, the neighbours across v0-v1, v1-v2 and
//! v2-v0 or -1, an attribute index) over SVECTOR vertices in whole world
//! units, Y down. An attribute word blocks an actor whose flags' bits 8-10
//! meet its bits 5-7, and bit 23 blocks layer 0 (above it, it is a pit the
//! leader falls through); bit 22 marks a slide (a
//! ledge) that is entered from below only by jumping, starts no jump and
//! takes the actor only down or level; the word reads 0 while the actor's
//! `layer_flags` bit (layer + 3) is set. Jumps follow the party's measured
//! arc (tools/analysis/platforming.py, `JUMP_ARC`): a slide is climbed only
//! as high as a jump rises, a gap crossed only as far as it carries.
use crate::memory::GameMemory;
use serde::Serialize;

const FIELD_VIEW: u32 = 0x800A_F880;
const VIEW_ANGLE: u32 = FIELD_VIEW + 0x10C;
const VIEW_PROJECTION: u32 = FIELD_VIEW + 0x178;
const VIEW_SCALED_WORLD: u32 = FIELD_VIEW + 0x1E4;
const COMPONENTS: u32 = 0x800A_FB0C;
const FIELD_WORK: u32 = 0x800B_2078;
const WORK_CONTROLLED: u32 = FIELD_WORK + 0x1F4;
const WORK_PARTY_PROCESSING: u32 = FIELD_WORK + 0x154;
const WORK_ALT_DPAD: u32 = FIELD_WORK + 0x2DC;
const WORK_OPEN_WINDOWS: u32 = FIELD_WORK + 0xFC;
/// The random draw's `unk2294` (frames left of the period), `unk2298` (the
/// period), `unk229C` (the count) and `unk22A0` (the steps, u16[32]).
const WORK_DRAW: u32 = FIELD_WORK + 0x21C;
const MODE_FIELD_MAP_ID: u32 = 0x8004_F34C;
const DESCRIPTOR_SIZE: u32 = 0x5C;
const ACTOR_SIZE: usize = 0x138;

const ATTR_LEDGE: u32 = 0x40_0000;
const ATTR_LAYER0_BLOCK: u32 = 0x80_0000;
/// Actor flags: jumping or in the air.
const FLAG_AIRBORNE: u32 = 0x1800;

/// The screen's centre (GTE offset) and size.
pub const SCREEN_CENTRE: (f32, f32) = (160.0, 112.0);

fn read_bytes(memory: &dyn GameMemory, address: u32, len: usize) -> Option<Vec<u8>> {
    let mut bytes = vec![0; len];
    memory.read(address, &mut bytes).ok().map(|()| bytes)
}
fn s16(b: &[u8], at: usize) -> i16 { i16::from_le_bytes([b[at], b[at + 1]]) }
fn u16_(b: &[u8], at: usize) -> u16 { u16::from_le_bytes([b[at], b[at + 1]]) }
fn s32(b: &[u8], at: usize) -> i32 { i32::from_le_bytes([b[at], b[at + 1], b[at + 2], b[at + 3]]) }
fn u32_(b: &[u8], at: usize) -> u32 { u32::from_le_bytes([b[at], b[at + 1], b[at + 2], b[at + 3]]) }
fn ram(pointer: u32) -> Option<u32> {
    (0x8000_0000..0x8020_0000).contains(&pointer).then_some(pointer)
}

#[derive(Clone, Copy, Debug, Default, PartialEq, Serialize, serde::Deserialize)]
pub struct V3 {
    pub x: f32,
    pub y: f32,
    pub z: f32,
}

impl V3 {
    pub fn new(x: f32, y: f32, z: f32) -> Self { V3 { x, y, z } }
    pub(crate) fn sub(self, o: V3) -> V3 { V3::new(self.x - o.x, self.y - o.y, self.z - o.z) }
    pub(crate) fn add(self, o: V3) -> V3 { V3::new(self.x + o.x, self.y + o.y, self.z + o.z) }
    pub(crate) fn scale(self, s: f32) -> V3 { V3::new(self.x * s, self.y * s, self.z * s) }
    pub(crate) fn dot(self, o: V3) -> f32 { self.x * o.x + self.y * o.y + self.z * o.z }
    fn cross(self, o: V3) -> V3 { V3::new(self.y * o.z - self.z * o.y, self.z * o.x - self.x * o.z, self.x * o.y - self.y * o.x) }
    /// `self - o` on the floor (y zero).
    pub fn sub_planar(self, o: V3) -> V3 { V3::new(self.x - o.x, 0.0, self.z - o.z) }
    /// Distance on the floor (x, z).
    pub fn planar(self, o: V3) -> f32 { ((self.x - o.x).powi(2) + (self.z - o.z).powi(2)).sqrt() }
}

/// The field camera.
#[derive(Clone, Copy, Debug, PartialEq, Serialize)]
pub struct Camera {
    /// `scaled_world`: rows of the 4.12 matrix as floats, and the translation.
    pub m: [[f32; 3]; 3],
    pub t: [f32; 3],
    pub h: f32,
    /// `field_view.angle`, 0x1000 a turn.
    pub angle: i32,
}

impl Camera {
    fn view(&self, p: V3) -> [f32; 3] {
        let m = &self.m;
        [0, 1, 2].map(|r| m[r][0] * p.x + m[r][1] * p.y + m[r][2] * p.z + self.t[r])
    }

    /// The screen point of a world point, when in front of the camera.
    pub fn project(&self, p: V3) -> Option<(f32, f32)> {
        let [x, y, z] = self.view(p);
        (z > 1.0).then(|| (SCREEN_CENTRE.0 + x * self.h / z, SCREEN_CENTRE.1 + y * self.h / z))
    }

    /// The part of the world triangle `corners` in front of the camera as
    /// the screen shows it: a polygon of up to four screen points (none
    /// when it lies wholly behind).
    pub fn project_clipped(&self, corners: [V3; 3]) -> Vec<(f32, f32)> {
        const NEAR: f32 = 1.0;
        let view = corners.map(|p| self.view(p));
        let mut polygon = Vec::with_capacity(4);
        for i in 0..3 {
            let (a, b) = (view[i], view[(i + 1) % 3]);
            if a[2] > NEAR {
                polygon.push(a);
            }
            if (a[2] > NEAR) != (b[2] > NEAR) {
                let t = (NEAR - a[2]) / (b[2] - a[2]);
                polygon.push([0, 1, 2].map(|k| a[k] + (b[k] - a[k]) * t));
            }
        }
        polygon.into_iter().map(|[x, y, z]| (SCREEN_CENTRE.0 + x * self.h / z.max(NEAR), SCREEN_CENTRE.1 + y * self.h / z.max(NEAR))).collect()
    }

    /// The world ray through a screen point: its origin (the eye) and
    /// direction.
    pub fn ray(&self, (sx, sy): (f32, f32)) -> Option<(V3, V3)> {
        let inverse = invert(self.m)?;
        let apply = |v: [f32; 3]| V3::new(
            inverse[0][0] * v[0] + inverse[0][1] * v[1] + inverse[0][2] * v[2],
            inverse[1][0] * v[0] + inverse[1][1] * v[1] + inverse[1][2] * v[2],
            inverse[2][0] * v[0] + inverse[2][1] * v[1] + inverse[2][2] * v[2],
        );
        let origin = apply([-self.t[0], -self.t[1], -self.t[2]]);
        let direction = apply([(sx - SCREEN_CENTRE.0) / self.h, (sy - SCREEN_CENTRE.1) / self.h, 1.0]);
        Some((origin, direction))
    }

    /// This camera as a picture shows it from another view (a presentation's
    /// pulled-back camera, a look from within the scene): a point `v` of
    /// this camera's view space shows at `centre + h · (q.x, q.y) / q.z`,
    /// `q = rows · v + offset` (screen pixels). The camera returned projects
    /// and picks as that picture shows the world (its depth sheared into x
    /// and y where the picture's centre is not the screen's); its angle, the
    /// game's, stays.
    pub fn seen(&self, rows: [[f32; 3]; 3], offset: [f32; 3], h: f32, centre: (f32, f32)) -> Camera {
        let mut m = [0, 1, 2].map(|r| [0, 1, 2].map(|c| (0..3).map(|k| rows[r][k] * self.m[k][c]).sum::<f32>()));
        let mut t = [0, 1, 2].map(|r| (0..3).map(|k| rows[r][k] * self.t[k]).sum::<f32>() + offset[r]);
        for (axis, shift) in [(0, (centre.0 - SCREEN_CENTRE.0) / h), (1, (centre.1 - SCREEN_CENTRE.1) / h)] {
            for c in 0..3 {
                m[axis][c] += shift * m[2][c];
            }
            t[axis] += shift * t[2];
        }
        Camera { m, t, h, angle: self.angle }
    }
}

fn invert(m: [[f32; 3]; 3]) -> Option<[[f32; 3]; 3]> {
    let det = m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1]) - m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0])
        + m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
    if det.abs() < 1e-9 {
        return None;
    }
    let c = |r0: usize, c0: usize, r1: usize, c1: usize| m[r0][c0] * m[r1][c1] - m[r0][c1] * m[r1][c0];
    Some([
        [c(1, 1, 2, 2) / det, -c(0, 1, 2, 2) / det, c(0, 1, 1, 2) / det],
        [-c(1, 0, 2, 2) / det, c(0, 0, 2, 2) / det, -c(0, 0, 1, 2) / det],
        [c(1, 0, 2, 1) / det, -c(0, 0, 2, 1) / det, c(0, 0, 1, 1) / det],
    ])
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct Triangle {
    pub v: [usize; 3],
    /// Neighbours across v0-v1, v1-v2, v2-v0 (-1: the mesh's edge).
    pub n: [i32; 3],
    pub attribute: u32,
}

#[derive(Clone, Debug, Default, PartialEq)]
pub struct Layer {
    pub vertices: Vec<V3>,
    pub triangles: Vec<Triangle>,
    /// Where the game keeps its triangles (0 for none read from the game):
    /// with their count, which walkmesh this is.
    pub at: u32,
}

impl Layer {
    pub fn corners(&self, t: usize) -> [V3; 3] {
        self.triangles[t].v.map(|i| self.vertices[i])
    }

    pub fn centroid(&self, t: usize) -> V3 {
        let [a, b, c] = self.corners(t);
        a.add(b).add(c).scale(1.0 / 3.0)
    }

    /// Whether (x, z) lies in triangle `t` (either winding).
    pub fn contains(&self, t: usize, x: f32, z: f32) -> bool {
        let [a, b, c] = self.corners(t);
        let side = |p: V3, q: V3| (q.x - p.x) * (z - p.z) - (q.z - p.z) * (x - p.x);
        let (d1, d2, d3) = (side(a, b), side(b, c), side(c, a));
        !((d1 < 0.0 || d2 < 0.0 || d3 < 0.0) && (d1 > 0.0 || d2 > 0.0 || d3 > 0.0))
    }

    /// The triangle under (x, z).
    pub fn locate(&self, x: f32, z: f32) -> Option<usize> {
        (0..self.triangles.len()).find(|t| self.contains(*t, x, z))
    }

    /// The nearest floor point to (x, z): its triangle and the point.
    pub fn nearest(&self, x: f32, z: f32) -> Option<(usize, V3)> {
        if let Some(t) = self.locate(x, z) {
            return Some((t, V3::new(x, self.height(t, x, z), z)));
        }
        let p = V3::new(x, 0.0, z);
        (0..self.triangles.len()).map(|t| {
            let [a, b, c] = self.corners(t);
            let q = [(a, b), (b, c), (c, a)].into_iter().map(|(u, v)| {
                let (d, along) = segment_planar(p, u, v);
                (d, u.add(v.sub(u).scale(along)))
            }).min_by(|x, y| x.0.total_cmp(&y.0)).expect("three edges");
            (q.0, t, q.1)
        }).min_by(|x, y| x.0.total_cmp(&y.0)).map(|(_, t, q)| {
            // Just inside the triangle, toward its centre.
            let inside = q.add(self.centroid(t).sub(q).scale(0.02));
            (t, V3::new(inside.x, self.height(t, inside.x, inside.z), inside.z))
        })
    }

    /// The floor's height (y, down) of triangle `t` at (x, z).
    pub fn height(&self, t: usize, x: f32, z: f32) -> f32 {
        let [a, b, c] = self.corners(t);
        let n = b.sub(a).cross(c.sub(a));
        if n.y.abs() < 1e-6 { return a.y; }
        a.y - (n.x * (x - a.x) + n.z * (z - a.z)) / n.y
    }

    /// The edge `e` of `t` (its two corners).
    pub fn edge(&self, t: usize, e: usize) -> (V3, V3) {
        let corners = self.corners(t);
        (corners[e], corners[(e + 1) % 3])
    }
}

/// An event actor.
#[derive(Clone, Copy, Debug, PartialEq, Serialize)]
pub struct Actor {
    pub index: usize,
    /// Whole world units (y down).
    pub position: V3,
    pub layer: usize,
    #[serde(skip)]
    pub triangle: [i16; 4],
    pub flags: u32,
    pub layer_flags: u32,
    /// The talk and touch reach (+0x1E) and height (+0x1A).
    pub radius: i16,
    pub height: i16,
    pub heading: i16,
    pub heading_goal: i16,
}

impl Actor {
    pub fn airborne(&self) -> bool {
        self.flags & FLAG_AIRBORNE != 0
    }
}

/// The field's random draw (`field_encounter_count_down`) as the map's
/// scripts armed it (event f7): each `period` counted field frames (a
/// direction held under player control) start `count` battles, one as each
/// of `steps` comes down to 0; `window` frames of the period are left. A
/// period of 0 is a draw the map has not armed. `map` is the field map
/// (the low 12 bits of `mode_field_map_id`).
#[derive(Clone, Debug, PartialEq, Eq, Serialize)]
pub struct RandomDraw {
    pub map: u16,
    pub period: i32,
    pub count: i32,
    pub window: i32,
    /// The frames left before each of the period's battles (65535 once
    /// spent).
    pub steps: Vec<u16>,
}

impl RandomDraw {
    /// Reads the draw. Only meaningful while the field mode runs.
    pub fn read(memory: &dyn GameMemory) -> Option<RandomDraw> {
        let work = read_bytes(memory, WORK_DRAW, 12 + 64)?;
        let map = s32(&read_bytes(memory, MODE_FIELD_MAP_ID, 4)?, 0);
        let count = s32(&work, 8);
        Some(RandomDraw {
            map: (map & 0xFFF) as u16,
            period: s32(&work, 4),
            count,
            window: s32(&work, 0),
            steps: (0..count.clamp(0, 32) as usize).map(|i| u16_(&work, 12 + i * 2)).collect(),
        })
    }
}

/// The field at one moment.
#[derive(Clone, Debug)]
pub struct Field {
    pub camera: Camera,
    pub layers: Vec<Layer>,
    pub attributes: Vec<u32>,
    pub actors: Vec<Option<Actor>>,
    pub player: Option<usize>,
    pub alt_dpad: bool,
    pub party_processing: bool,
    pub dialogue_open: bool,
}

impl Field {
    /// Reads the field. Only meaningful while the field mode runs.
    pub fn read(memory: &dyn GameMemory) -> Option<Field> {
        let view = read_bytes(memory, VIEW_SCALED_WORLD, 0x20)?;
        let m = [0, 1, 2].map(|r| [0, 1, 2].map(|c| f32::from(s16(&view, (r * 3 + c) * 2)) / 4096.0));
        let t = [0, 1, 2].map(|i| s32(&view, 0x14 + i * 4) as f32);
        let h = s32(&read_bytes(memory, VIEW_PROJECTION, 4)?, 0) as f32;
        let angle = i32::from(s16(&read_bytes(memory, VIEW_ANGLE, 2)?, 0));
        let camera = Camera { m, t, h: if h > 0.0 { h } else { 256.0 }, angle };
        let comps = read_bytes(memory, COMPONENTS, 0x4A)?;
        let descriptor_count = s32(&comps, 0).clamp(0, 256) as usize;
        let descriptors = ram(u32_(&comps, 4));
        let attributes_at = ram(u32_(&comps, 0x14));
        let layer_count = s16(&comps, 0x48).clamp(0, 4) as usize;
        let mut layers = Vec::new();
        let mut max_attribute = 0;
        for layer in 0..layer_count {
            let triangles_at = ram(u32_(&comps, 0x18 + layer * 4));
            let vertices_at = ram(u32_(&comps, 0x28 + layer * 4));
            let count = s32(&comps, 0x38 + layer * 4).clamp(0, 8192) as usize;
            let (Some(triangles_at), Some(vertices_at)) = (triangles_at, vertices_at) else {
                layers.push(Layer::default());
                continue;
            };
            let raw = read_bytes(memory, triangles_at, count * 14)?;
            let mut triangles = Vec::with_capacity(count);
            let mut max_vertex = 0;
            for i in 0..count {
                let b = &raw[i * 14..i * 14 + 14];
                let v = [0, 1, 2].map(|k| u16_(b, k * 2) as usize);
                let n = [0, 1, 2].map(|k| i32::from(s16(b, 6 + k * 2)));
                max_vertex = max_vertex.max(v[0]).max(v[1]).max(v[2]);
                max_attribute = max_attribute.max(u32::from(b[12]));
                triangles.push(Triangle { v, n, attribute: u32::from(b[12]) });
            }
            let raw = read_bytes(memory, vertices_at, (max_vertex + 1) * 8)?;
            let vertices = (0..=max_vertex).map(|i| V3::new(f32::from(s16(&raw, i * 8)), f32::from(s16(&raw, i * 8 + 2)), f32::from(s16(&raw, i * 8 + 4)))).collect();
            triangles.retain(|t| t.n.iter().all(|n| *n < count as i32));
            layers.push(Layer { vertices, triangles, at: triangles_at });
        }
        let attributes = match attributes_at {
            Some(at) => {
                let raw = read_bytes(memory, at, (max_attribute as usize + 1) * 4)?;
                (0..=max_attribute as usize).map(|i| u32_(&raw, i * 4)).collect()
            }
            None => vec![],
        };
        let mut actors = Vec::with_capacity(descriptor_count);
        if let Some(descriptors) = descriptors {
            let raw = read_bytes(memory, descriptors, descriptor_count * DESCRIPTOR_SIZE as usize)?;
            for i in 0..descriptor_count {
                let d = &raw[i * DESCRIPTOR_SIZE as usize..][..DESCRIPTOR_SIZE as usize];
                let actor = ram(u32_(d, 0x4C)).and_then(|at| read_bytes(memory, at, ACTOR_SIZE)).map(|a| Actor {
                    index: i,
                    position: V3::new(s32(d, 0x20) as f32, s32(d, 0x24) as f32, s32(d, 0x28) as f32),
                    layer: s16(&a, 0x10).clamp(0, 3) as usize,
                    triangle: [0, 1, 2, 3].map(|k| s16(&a, 8 + k * 2)),
                    flags: u32_(&a, 0),
                    layer_flags: u32_(&a, 4),
                    radius: s16(&a, 0x1E),
                    height: s16(&a, 0x1A),
                    heading: s16(&a, 0x104),
                    heading_goal: s16(&a, 0x106),
                });
                actors.push(actor);
            }
        }
        let work = read_bytes(memory, FIELD_WORK, 0x2E0)?;
        let player = usize::try_from(s32(&work, (WORK_CONTROLLED - FIELD_WORK) as usize)).ok().filter(|p| actors.get(*p).is_some_and(Option::is_some));
        Some(Field {
            camera,
            layers,
            attributes,
            actors,
            player,
            alt_dpad: s16(&work, (WORK_ALT_DPAD - FIELD_WORK) as usize) != 0,
            party_processing: work[(WORK_PARTY_PROCESSING - FIELD_WORK) as usize] != 0,
            dialogue_open: u16_(&work, (WORK_OPEN_WINDOWS - FIELD_WORK) as usize) != 0,
        })
    }

    pub fn player(&self) -> Option<&Actor> {
        self.player.and_then(|p| self.actors.get(p)?.as_ref())
    }

    /// Other actors in the way of `actor`: their centres and the distance
    /// to keep from them (both bodies). Actors the game skips (flags bit
    /// 0), and those standing where the actor stands (followers stacked
    /// on the leader), are not.
    pub fn obstacles(&self, actor: &Actor) -> Vec<(V3, f32)> {
        self.actors.iter().flatten()
            .filter(|o| o.index != actor.index && o.flags & 1 == 0 && o.layer == actor.layer && o.position.planar(actor.position) > 2.0)
            .map(|o| (o.position, f32::from(o.radius.max(0)) + f32::from(actor.radius.max(0)) + 4.0))
            .collect()
    }

    /// The other actor shown nearest a screen point, within `reach` screen
    /// pixels of its feet or its middle: actors the game skips, and those
    /// standing where the player stands (followers), are not pointed at.
    pub fn actor_at(&self, screen: (f32, f32), reach: f32) -> Option<usize> {
        let player = self.player()?;
        self.actors.iter().flatten()
            .filter(|a| a.index != player.index && a.flags & 1 == 0 && a.position.planar(player.position) > 2.0)
            .filter_map(|a| {
                let middle = V3::new(a.position.x, a.position.y - f32::from(a.height.max(0)) / 2.0, a.position.z);
                [a.position, middle].into_iter()
                    .filter_map(|p| self.camera.project(p))
                    .map(|(x, y)| ((x - screen.0).powi(2) + (y - screen.1).powi(2)).sqrt())
                    .min_by(f32::total_cmp)
                    .filter(|d| *d <= reach)
                    .map(|d| (d, a.index))
            })
            .min_by(|a, b| a.0.total_cmp(&b.0))
            .map(|(_, index)| index)
    }

    /// The attribute word triangle `t` of `layer` has for `actor`.
    fn attribute(&self, layer: usize, t: usize, actor: &Actor) -> u32 {
        if actor.layer_flags & (1 << (layer + 3)) != 0 || self.party_processing {
            return 0;
        }
        let index = self.layers[layer].triangles[t].attribute as usize;
        self.attributes.get(index).copied().unwrap_or(0)
    }

    /// Whether `actor` may not stand on triangle `t` of `layer` at all.
    pub fn blocked(&self, layer: usize, t: usize, actor: &Actor) -> bool {
        let word = self.attribute(layer, t, actor);
        ((actor.flags >> 8) & 7) & ((word >> 5) & 7) != 0 || (layer == 0 && word & ATTR_LAYER0_BLOCK != 0)
    }

    fn ledge(&self, layer: usize, t: usize, actor: &Actor) -> bool {
        self.attribute(layer, t, actor) & ATTR_LEDGE != 0
    }

    /// The floor point a screen point picks on the player's layer: the
    /// nearest walkmesh triangle the ray meets.
    pub fn pick(&self, screen: (f32, f32)) -> Option<Hit> {
        let (origin, direction) = self.camera.ray(screen)?;
        self.pick_ray(origin, direction, f32::INFINITY)
    }

    /// The floor point a ray in the world picks on the player's layer (a
    /// ray through a diorama of the field): the nearest walkmesh triangle it
    /// meets from `origin` to `within` lengths of `direction` along it.
    pub fn pick_ray(&self, origin: V3, direction: V3, within: f32) -> Option<Hit> {
        let player = self.player()?;
        let layer = player.layer;
        let mesh = self.layers.get(layer)?;
        let mut best: Option<(f32, usize)> = None;
        for t in 0..mesh.triangles.len() {
            let [a, b, c] = mesh.corners(t);
            if let Some(distance) = intersect(origin, direction, a, b, c).filter(|d| *d <= within)
                && best.is_none_or(|(d, _)| distance < d)
            {
                best = Some((distance, t));
            }
        }
        let (distance, triangle) = best?;
        let point = origin.add(direction.scale(distance));
        Some(Hit { point, triangle, layer, reach: self.reach(player, triangle, point) })
    }

    /// The other actor nearest a ray in the world (a ray through a diorama
    /// of the field), within `reach` world units of it at its feet or its
    /// middle, from `origin` to `within` lengths of `direction` along it:
    /// actors the game skips, and those standing where the player stands
    /// (followers), are not pointed at, as with [`Field::actor_at`].
    pub fn actor_near_ray(&self, origin: V3, direction: V3, within: f32, reach: f32) -> Option<usize> {
        let player = self.player()?;
        let length = direction.dot(direction);
        if length <= 0.0 {
            return None;
        }
        // How far `p` lies from the ray's part in reach.
        let away = |p: V3| {
            let along = (p.sub(origin).dot(direction) / length).clamp(0.0, within);
            let nearest = origin.add(direction.scale(along));
            let d = p.sub(nearest);
            d.dot(d).sqrt()
        };
        self.actors.iter().flatten()
            .filter(|a| a.index != player.index && a.flags & 1 == 0 && a.position.planar(player.position) > 2.0)
            .filter_map(|a| {
                let middle = V3::new(a.position.x, a.position.y - f32::from(a.height.max(0)) / 2.0, a.position.z);
                let d = away(a.position).min(away(middle));
                (d <= reach).then_some((d, a.index))
            })
            .min_by(|a, b| a.0.total_cmp(&b.0))
            .map(|(_, index)| index)
    }

    /// Whether the player can walk to `point` on `goal`, must jump on the
    /// way, or cannot reach it. A pointer resting on the floor asks it every
    /// frame: the answers for a game walkmesh are kept a while (by its
    /// triangles' place and count, the actor's triangle and flags and the
    /// goal; the walkmesh holds still within a map).
    pub fn reach(&self, actor: &Actor, goal: usize, point: V3) -> Reach {
        let layer = self.layers.get(actor.layer);
        let key = layer.filter(|l| l.at != 0).zip(self.start(actor))
            .map(|(l, start)| (l.at, l.triangles.len(), start, goal, actor.flags, actor.layer_flags));
        thread_local! {
            static KNOWN: std::cell::RefCell<std::collections::VecDeque<((u32, usize, usize, usize, u32, u32), Reach)>> = Default::default();
        }
        if let Some(reach) = key.and_then(|key| KNOWN.with_borrow(|known| known.iter().find(|(k, _)| *k == key).map(|(_, r)| *r))) {
            return reach;
        }
        let reach = match self.route(actor, goal, point) {
            Some(route) if route.jump.is_some() => Reach::Jump,
            Some(_) => Reach::Walk,
            None => Reach::Invalid,
        };
        if let Some(key) = key {
            KNOWN.with_borrow_mut(|known| {
                if known.len() == 64 {
                    known.pop_front();
                }
                known.push_back((key, reach));
            });
        }
        reach
    }

    /// The triangle the actor stands on.
    fn start(&self, actor: &Actor) -> Option<usize> {
        let mesh = self.layers.get(actor.layer)?;
        let t = actor.triangle[actor.layer];
        if t >= 0 && (t as usize) < mesh.triangles.len() && mesh.contains(t as usize, actor.position.x, actor.position.z) {
            return Some(t as usize);
        }
        mesh.locate(actor.position.x, actor.position.z).or((t >= 0 && (t as usize) < mesh.triangles.len()).then_some(t as usize))
    }

    /// The route from the actor to `point` on triangle `goal`: the
    /// triangles' corridor, string-pulled to corners, and where a jump is
    /// needed (a ledge entered from below, or a gap to the goal's part of
    /// the mesh).
    pub fn route(&self, actor: &Actor, goal: usize, point: V3) -> Option<Route> {
        self.route_with(actor, goal, point, &mut Leaps::default())
    }

    /// [`Field::route`], with the jumps found kept in `leaps` for the next
    /// search of this layer (the assist plans its route again each frame;
    /// the walkmesh does not change under it).
    pub fn route_with(&self, actor: &Actor, goal: usize, point: V3, leaps: &mut Leaps) -> Option<Route> {
        let layer = actor.layer;
        let mesh = self.layers.get(layer)?;
        let start = self.start(actor)?;
        if goal >= mesh.triangles.len() || self.blocked(layer, goal, actor) {
            return None;
        }
        // A* over triangles walking to neighbours; where no walk reaches the
        // goal, again with jumps from a floor's edge over a slide too high
        // to walk onto, to the floor beyond it (the cheap search first: a
        // walk wins wherever there is one).
        let mut search = Search::new(mesh.triangles.len());
        search.seed(start, 0.0, None, mesh.centroid(start).planar(point));
        self.search(actor, &mut search, goal, point, None);
        if search.cost[goal].is_infinite() {
            // Walking reached all it reaches: on from there by the jumps out
            // of it, then by walking and jumping.
            leaps.keep_for(layer, mesh.triangles.len(), jump_arc(actor));
            let others = self.obstacles(actor);
            let walked: Vec<usize> = (0..mesh.triangles.len()).filter(|t| search.closed[*t]).collect();
            for t in walked {
                for e in 0..3 {
                    if let Some((next, jump)) = self.leap(actor, leaps, &others, t, e)
                        && !search.closed[next]
                    {
                        let cost = search.cost[t] + mesh.centroid(t).planar(jump.takeoff) + jump.takeoff.planar(jump.toward) + JUMP_COST;
                        search.seed(next, cost, Some((t, Some(jump))), mesh.centroid(next).planar(point));
                    }
                }
            }
            self.search(actor, &mut search, goal, point, Some((leaps, &others)));
        }
        let Search { cost, from, .. } = search;
        if cost[goal].is_infinite() {
            // The search ran out: what it reached is all the actor reaches.
            let reachable: Vec<bool> = cost.iter().map(|c| c.is_finite()).collect();
            return self.route_to_gap(actor, start, goal, point, &reachable);
        }
        // Back from the goal: the corridor up to the first jump on the way,
        // and that jump.
        let mut corridor = vec![goal];
        let mut jump = None;
        let mut t = goal;
        while let Some((previous, leap)) = from[t] {
            if leap.is_some() {
                corridor.clear();
                jump = leap;
            }
            corridor.push(previous);
            t = previous;
        }
        corridor.reverse();
        let clearance = f32::from(actor.radius).clamp(4.0, 24.0);
        let Some(jump) = jump else {
            let corners = funnel(mesh, &corridor, actor.position, point, clearance);
            let length = path_length(actor.position, &corners);
            return Some(Route { corners, jump: None, goal: point, length });
        };
        // The walk up to the takeoff, the jump to its landing, the goal.
        let mut corners = funnel(mesh, &corridor, actor.position, jump.takeoff, clearance);
        corners.extend([jump.toward, point]);
        corners.dedup_by(|a, b| a.planar(*b) < 0.5);
        let length = path_length(actor.position, &corners);
        Some(Route { corners, jump: Some(jump), goal: point, length })
    }

    /// A* over `search` toward `goal` (at `point`), from what it holds open:
    /// each triangle's cost and the step into it (from where, and the jump
    /// when it was one). With `leaps` (the jump's arc, other actors' bodies)
    /// jumps over slides too high to walk onto count as steps. It runs until
    /// the goal, else until all it reaches is reached; a triangle expanded
    /// once stays so (the heuristic, the floor distance to the goal, never
    /// overestimates a step).
    fn search(&self, actor: &Actor, search: &mut Search, goal: usize, point: V3, mut leaps: Option<(&mut Leaps, &[(V3, f32)])>) {
        let mesh = &self.layers[actor.layer];
        while let Some(Node(_, t)) = search.open.pop() {
            if t == goal {
                break;
            }
            if std::mem::replace(&mut search.closed[t], true) {
                continue;
            }
            for e in 0..3 {
                let (next, step, jump) = match self.step(actor, t, e) {
                    Some(next) => {
                        let (a, b) = mesh.edge(t, e);
                        let mid = a.add(b).scale(0.5);
                        (next, mesh.centroid(t).planar(mid) + mid.planar(mesh.centroid(next)), None)
                    }
                    None => match leaps.as_mut().and_then(|(leaps, others)| self.leap(actor, leaps, others, t, e)) {
                        Some((next, jump)) => {
                            let step = mesh.centroid(t).planar(jump.takeoff) + jump.takeoff.planar(jump.toward) + JUMP_COST;
                            (next, step, Some(jump))
                        }
                        None => continue,
                    },
                };
                if !search.closed[next] {
                    search.seed(next, search.cost[t] + step, Some((t, jump)), mesh.centroid(next).planar(point));
                }
            }
        }
        // Left open for a search that goes on from here.
        search.open.clear();
    }

    /// Where the actor walks from triangle `t` of its layer across edge `e`:
    /// a floor walks onto any floor beside it and onto a slide no more than
    /// `CLIMB` above it; a slide takes it only down or level (heights are
    /// the triangles' middles, as tools/analysis/platforming.py compares
    /// them). Triangles that block it, and the mesh's edge, are no way.
    fn step(&self, actor: &Actor, t: usize, e: usize) -> Option<usize> {
        let layer = actor.layer;
        let mesh = &self.layers[layer];
        let next = usize::try_from(mesh.triangles[t].n[e]).ok().filter(|n| *n < mesh.triangles.len())?;
        // Above layer 0 a triangle of bit 23 has no floor: the leader falls
        // through it to another layer, which a route does not follow.
        if self.blocked(layer, next, actor) || (layer > 0 && self.attribute(layer, next, actor) & ATTR_LAYER0_BLOCK != 0) {
            return None;
        }
        let slide = self.ledge(layer, t, actor) || self.ledge(layer, next, actor);
        (!(slide && mesh.centroid(next).y < mesh.centroid(t).y - CLIMB)).then_some(next)
    }

    /// A jump from floor `t`'s edge `e` over the slide beyond it, too high
    /// to walk onto, straight out from the edge: the floor it lands on, the
    /// first its flight (`arc`) passes over above every slide it crosses,
    /// and where on it, a little past where it reaches it. None where the
    /// flight meets a slide, a triangle the actor may not cross or the
    /// mesh's edge first, and where another actor's body (`others`: centres
    /// and the distances kept from them) stands on the takeoff or the
    /// landing. No jump starts on a slide.
    fn leap(&self, actor: &Actor, leaps: &mut Leaps, others: &[(V3, f32)], t: usize, e: usize) -> Option<(usize, Jump)> {
        // From a floor onto a slide it may cross (the edges worth a flight).
        let layer = actor.layer;
        let first = usize::try_from(self.layers[layer].triangles[t].n[e]).ok().filter(|n| *n < self.layers[layer].triangles.len())?;
        if self.ledge(layer, t, actor) || !self.ledge(layer, first, actor) || self.blocked(layer, first, actor) {
            return None;
        }
        let arc = leaps.arc;
        let (next, jump) = (*leaps.found.entry((t, e)).or_insert_with(|| self.flight(actor, &Arc(arc), t, e)))?;
        let kept = |p: V3| others.iter().all(|(centre, keep)| centre.planar(p) >= *keep);
        (kept(jump.takeoff) && kept(jump.toward)).then_some((next, jump))
    }

    /// [`Field::leap`]'s flight, whoever stands where (the walkmesh's alone).
    fn flight(&self, actor: &Actor, arc: &Arc, t: usize, e: usize) -> Option<(usize, Jump)> {
        let layer = actor.layer;
        let mesh = &self.layers[layer];
        let first = usize::try_from(mesh.triangles[t].n[e]).ok().filter(|n| *n < mesh.triangles.len())?;
        if self.ledge(layer, t, actor) || !self.ledge(layer, first, actor) || self.blocked(layer, first, actor) {
            return None;
        }
        let (a, b) = mesh.edge(t, e);
        let mid = a.add(b).scale(0.5);
        let takeoff = V3::new(mid.x, mesh.height(t, mid.x, mid.z), mid.z);
        // Out of `t` across the edge, on the floor.
        let along = b.sub_planar(a);
        let length = (along.x * along.x + along.z * along.z).sqrt().max(1e-3);
        let mut out = V3::new(along.z / length, 0.0, -along.x / length);
        if out.dot(mesh.centroid(t).sub_planar(takeoff)) > 0.0 {
            out = out.scale(-1.0);
        }
        let at = |s: f32| takeoff.add(out.scale(s));
        let above = |t: usize, s: f32| {
            let p = at(s);
            takeoff.y - mesh.height(t, p.x, p.z)
        };
        let (mut current, mut entered) = (first, 0.0f32);
        for _ in 0..LEAP_TRIANGLES {
            let (left, exit) = exit(mesh, current, takeoff, out, entered)?;
            if !self.ledge(layer, current, actor) {
                // A floor: land a little past where the flight reaches it,
                // and no farther than halfway across the floor the line
                // goes on over (not just this triangle: a thin one's middle
                // lies by its edges, where the leader, let go a little
                // short, comes down on the slide it cleared and slides
                // back).
                let floor = |n: usize| !self.ledge(layer, n, actor) && !self.blocked(layer, n, actor)
                    && self.attribute(layer, n, actor) & ATTR_LAYER0_BLOCK == 0;
                let mut floors = vec![(current, left)];
                let (mut through, mut edge) = (left, exit);
                while through < entered + 2.0 * LANDING_IN && floors.len() < LEAP_TRIANGLES {
                    let last = floors[floors.len() - 1].0;
                    let Some(next) = usize::try_from(mesh.triangles[last].n[edge]).ok().filter(|n| *n < mesh.triangles.len() && floor(*n)) else { break };
                    let Some((out_of, e)) = crate::field::exit(mesh, next, takeoff, out, through) else { break };
                    floors.push((next, out_of));
                    (through, edge) = (out_of, e);
                }
                let s = (entered + LANDING_IN).min((entered + through) / 2.0);
                let under = floors.iter().find(|(_, out_of)| s <= *out_of).map_or(current, |(n, _)| *n);
                if !arc.carries(s, above(under, s)) {
                    return None;
                }
                let p = at(s);
                return Some((under, Jump { takeoff, toward: V3::new(p.x, mesh.height(under, p.x, p.z), p.z), from: t }));
            }
            if !arc.carries(entered, above(current, entered) + CLEAR) || !arc.carries(left, above(current, left) + CLEAR) {
                return None;
            }
            let next = usize::try_from(mesh.triangles[current].n[exit]).ok().filter(|n| *n < mesh.triangles.len())?;
            if self.blocked(layer, next, actor) {
                return None;
            }
            (current, entered) = (next, left);
        }
        None
    }

    /// The triangles of the actor's layer it can reach from where it stands,
    /// walking and jumping over slides ([`Field::step`], [`Field::leap`]; no
    /// gap's jumps).
    pub fn reachable(&self, actor: &Actor) -> Vec<bool> {
        let Some(mesh) = self.layers.get(actor.layer) else { return vec![] };
        let mut seen = vec![false; mesh.triangles.len()];
        let Some(start) = self.start(actor) else { return seen };
        let mut leaps = Leaps::default();
        leaps.keep_for(actor.layer, mesh.triangles.len(), jump_arc(actor));
        let others = self.obstacles(actor);
        seen[start] = true;
        let mut stack = vec![start];
        while let Some(t) = stack.pop() {
            for e in 0..3 {
                let next = self.step(actor, t, e).or_else(|| self.leap(actor, &mut leaps, &others, t, e).map(|(next, _)| next));
                if let Some(next) = next
                    && !seen[next]
                {
                    seen[next] = true;
                    stack.push(next);
                }
            }
        }
        seen
    }

    /// The floor a screen point picks to walk to: the one [`Field::pick`]
    /// finds, when the player can reach it; else (a door, a wall, a house,
    /// the sky) the floor it can reach shown nearest the point, within
    /// `reach` screen pixels, and the point the screen point shows at that
    /// floor's height (beyond it: where a door or an exit stands).
    pub fn pick_walkable(&self, screen: (f32, f32), reach: f32) -> Option<(Hit, Option<V3>)> {
        if let Some(hit) = self.pick(screen).filter(|hit| hit.reach != Reach::Invalid) {
            return Some((hit, None));
        }
        let player = self.player()?;
        let mesh = self.layers.get(player.layer)?;
        let reachable = self.reachable(player);
        let (origin, direction) = self.camera.ray(screen)?;
        // The reachable triangle shown nearest the point (along the ray,
        // among those it lies on), and where on it.
        let mut best: Option<(f32, f32, usize, (f32, f32))> = None;
        for t in (0..mesh.triangles.len()).filter(|t| reachable[*t]) {
            let shown = self.camera.project_clipped(mesh.corners(t));
            if shown.len() < 3 {
                continue;
            }
            let (near, distance) = nearest_on_polygon(screen, &shown);
            let along = if distance == 0.0 {
                let [p, q, r] = mesh.corners(t);
                intersect(origin, direction, p, q, r).unwrap_or(f32::INFINITY)
            } else {
                f32::INFINITY
            };
            if distance <= reach && best.is_none_or(|(d, l, ..)| (distance, along) < (d, l)) {
                best = Some((distance, along, t, near));
            }
        }
        let (_, _, triangle, near) = best?;
        // Just inside the triangle as shown, then on its floor.
        let [a, b, c] = mesh.corners(triangle);
        let polygon = self.camera.project_clipped([a, b, c]);
        let centre = polygon.iter().fold((0.0, 0.0), |(x, y), p| (x + p.0, y + p.1));
        let (cx, cy) = (centre.0 / polygon.len() as f32 - near.0, centre.1 / polygon.len() as f32 - near.1);
        let step = 0.5 / (cx * cx + cy * cy).sqrt().max(0.5);
        let shown = (near.0 + cx * step, near.1 + cy * step);
        let (o, d) = self.camera.ray(shown)?;
        // On the triangle there (or its plane, a hair outside).
        let normal = b.sub(a).cross(c.sub(a));
        let point = match intersect(o, d, a, b, c) {
            Some(along) => o.add(d.scale(along)),
            None if normal.dot(d).abs() > 1e-9 => o.add(d.scale(normal.dot(a.sub(o)) / normal.dot(d))),
            None => mesh.centroid(triangle),
        };
        let hit = Hit { point, triangle, layer: player.layer, reach: self.reach(player, triangle, point) };
        if hit.reach == Reach::Invalid {
            return None;
        }
        // Where the point shows at the floor's height, past it from the player.
        let beyond = (direction.y.abs() > 1e-6)
            .then(|| (point.y - origin.y) / direction.y)
            .filter(|along| *along > 0.0)
            .map(|along| origin.add(direction.scale(along)))
            .filter(|b| (BEYOND_MIN..BEYOND_MAX).contains(&b.planar(point)) && b.planar(player.position) > point.planar(player.position));
        Some((hit, beyond))
    }

    /// The goal lies on a part of the mesh the actor cannot walk to (an
    /// island beyond a gap): walk to the edge of where it can walk from
    /// which a jump carries it to the island (its arc passing over the
    /// island's nearest edge at that height), the one nearest the goal, and
    /// jump across.
    fn route_to_gap(&self, actor: &Actor, start: usize, goal: usize, point: V3, reachable: &[bool]) -> Option<Route> {
        let mesh = &self.layers[actor.layer];
        // Mesh joining the two (through walls the actor may not cross) is no
        // gap to jump.
        let mut linked = vec![false; mesh.triangles.len()];
        let mut stack = vec![start];
        linked[start] = true;
        while let Some(t) = stack.pop() {
            for next in mesh.triangles[t].n {
                if next >= 0 && !linked[next as usize] {
                    linked[next as usize] = true;
                    stack.push(next as usize);
                }
            }
        }
        if linked[goal] {
            return None;
        }
        // The island's floor edges (no slide: a jump lands on a floor).
        let mut island = vec![false; mesh.triangles.len()];
        let mut stack = vec![goal];
        island[goal] = true;
        let mut shore = Vec::new();
        while let Some(t) = stack.pop() {
            for e in 0..3 {
                let next = mesh.triangles[t].n[e];
                if next < 0 {
                    if !self.ledge(actor.layer, t, actor) {
                        shore.push(mesh.edge(t, e));
                    }
                } else if !island[next as usize] && !self.blocked(actor.layer, next as usize, actor) {
                    island[next as usize] = true;
                    stack.push(next as usize);
                }
            }
        }
        let arc = jump_arc(actor);
        let far = arc.0.last().map_or(0.0, |(d, _)| *d);
        // The shore's bounds, widened by the farthest jump: no takeoff beyond.
        let (x0, x1, z0, z1) = shore.iter().flat_map(|(u, v)| [*u, *v]).fold((f32::MAX, f32::MIN, f32::MAX, f32::MIN), |(x0, x1, z0, z1), p| {
            (x0.min(p.x - far), x1.max(p.x + far), z0.min(p.z - far), z1.max(p.z + far))
        });
        let others = self.obstacles(actor);
        // Takeoffs: the walkable floor's open edges (no jump starts on a
        // slide, nor where another actor stands).
        let mut best: Option<(f32, usize, V3, V3)> = None;
        for t in (0..mesh.triangles.len()).filter(|t| reachable[*t] && !self.ledge(actor.layer, *t, actor)) {
            for e in 0..3 {
                if mesh.triangles[t].n[e] >= 0 {
                    continue;
                }
                let (a, b) = mesh.edge(t, e);
                let takeoff = a.add(b).scale(0.5);
                if !(x0..=x1).contains(&takeoff.x) || !(z0..=z1).contains(&takeoff.z)
                    || others.iter().any(|(centre, keep)| centre.planar(takeoff) < *keep)
                {
                    continue;
                }
                let Some((landing, gap)) = shore.iter()
                    .map(|&(u, v)| {
                        let (d, along) = segment_planar(takeoff, u, v);
                        (u.add(v.sub(u).scale(along)), d)
                    })
                    .min_by(|x, y| x.1.total_cmp(&y.1)) else { return None };
                // How high the landing lies above the takeoff (y is down).
                if !arc.carries(gap + LANDING_IN, takeoff.y - landing.y) {
                    continue;
                }
                let d = takeoff.planar(point);
                if best.is_none_or(|(bd, ..)| d < bd) {
                    best = Some((d, t, takeoff, landing));
                }
            }
        }
        let (_, takeoff_triangle, takeoff, shore_point) = best?;
        // On over the island's edge toward the goal.
        let toward = shore_point.sub_planar(takeoff);
        let length = (toward.x * toward.x + toward.z * toward.z).sqrt().max(1e-3);
        let landing = shore_point.add(V3::new(toward.x / length * LANDING_IN, 0.0, toward.z / length * LANDING_IN));
        let mut route = self.route(actor, takeoff_triangle, takeoff)?;
        if route.jump.is_none() {
            route.jump = Some(Jump { takeoff, toward: landing, from: takeoff_triangle });
        }
        route.goal = point;
        route.corners.extend([landing, point]);
        route.corners.dedup_by(|a, b| a.planar(*b) < 0.5);
        route.length = path_length(actor.position, &route.corners);
        Some(route)
    }
}

/// The jumps found from floors' edges over slides ([`Field::route_with`]):
/// for one layer of one walkmesh and one arc, each edge's flight (whoever
/// stands where), kept between searches while that holds.
#[derive(Clone, Debug, Default)]
pub struct Leaps {
    layer: usize,
    triangles: usize,
    arc: &'static [(f32, f32)],
    found: std::collections::HashMap<(usize, usize), Option<(usize, Jump)>>,
}

impl Leaps {
    /// Kept for `layer` of `triangles` triangles and `arc`; else emptied.
    fn keep_for(&mut self, layer: usize, triangles: usize, arc: Arc) {
        if (self.layer, self.triangles, self.arc.as_ptr()) != (layer, triangles, arc.0.as_ptr()) {
            *self = Leaps { layer, triangles, arc: arc.0, found: Default::default() };
        }
    }
}

/// A jump's flight: the distance from its takeoff and the height above it
/// after each field update.
pub struct Arc(&'static [(f32, f32)]);

impl Arc {
    /// The most it rises.
    pub fn rise(&self) -> f32 {
        self.0.iter().map(|(_, h)| *h).fold(0.0, f32::max)
    }

    /// Whether it passes `distance` from its takeoff at `rise` above it or
    /// higher (in the air the leader may go slower than it runs, so any
    /// update as far or farther counts).
    pub fn carries(&self, distance: f32, rise: f32) -> bool {
        self.0.iter().any(|(d, h)| *d >= distance && *h >= rise)
    }
}

/// Fei's running jump on foot (map 22) and the party's in its gears (map
/// 414), as tools/analysis/platforming.py's FOOT and GEAR measured them.
const FOOT_ARC: [(f32, f32); 22] = [
    (0.0, 20.75), (0.0, 39.5), (12.0, 56.25), (24.0, 71.0), (36.0, 83.75), (48.0, 94.5), (60.0, 103.25),
    (72.0, 110.0), (84.0, 114.75), (96.0, 117.5), (108.0, 118.25), (120.0, 117.0), (132.0, 113.75),
    (144.0, 108.5), (156.0, 101.25), (168.0, 92.0), (180.0, 80.75), (192.0, 67.5), (204.0, 52.25),
    (216.0, 35.0), (228.0, 15.75), (240.0, 0.0),
];
const GEAR_ARC: [(f32, f32); 19] = [
    (0.0, 24.75), (0.0, 47.5), (12.0, 68.25), (24.0, 87.0), (36.0, 103.75), (48.0, 118.5), (60.0, 131.25),
    (72.0, 131.25), (84.0, 129.25), (96.0, 125.25), (108.0, 119.25), (120.0, 111.25), (132.0, 101.25),
    (144.0, 89.25), (156.0, 75.25), (168.0, 59.25), (180.0, 41.25), (192.0, 21.25), (204.0, 0.0),
];

/// The arc of the actor's jump: on foot when its flags' bits 8-10 are the
/// foot party's (attribute bit 7 blocks it), else in its gears.
pub fn jump_arc(actor: &Actor) -> Arc {
    if (actor.flags >> 8) & 7 == 4 { Arc(&FOOT_ARC) } else { Arc(&GEAR_ARC) }
}

/// The length of a path from `from` through `corners` on the floor.
pub fn path_length(from: V3, corners: &[V3]) -> f32 {
    corners.iter().fold((from, 0.0), |(at, sum), c| (*c, sum + at.planar(*c))).1
}

/// A slide more than this above a floor is not walked onto, nor a
/// triangle more than this above a slide (tools/analysis/platforming.py's
/// CLIMB, by the triangles' middles).
const CLIMB: f32 = 8.0;
/// How far above a slide a jump's flight must pass.
const CLEAR: f32 = 4.0;
/// How far past where its flight reaches a floor a jump lands.
const LANDING_IN: f32 = 24.0;
/// What a jump costs a route beyond its length (a walk round is better).
const JUMP_COST: f32 = 200.0;
/// The most triangles a jump's flight crosses.
const LEAP_TRIANGLES: usize = 32;
/// How far the point beyond a picked door or wall may lie past the floor.
const BEYOND_MIN: f32 = 8.0;
const BEYOND_MAX: f32 = 600.0;

/// A route search's state ([`Field::search`]): each triangle's cost so far,
/// the step into it (from where, and the jump when it was one), whether it
/// was expanded, and what is open.
struct Search {
    cost: Vec<f32>,
    from: Vec<Option<(usize, Option<Jump>)>>,
    closed: Vec<bool>,
    open: std::collections::BinaryHeap<Node>,
}

impl Search {
    fn new(count: usize) -> Search {
        Search { cost: vec![f32::INFINITY; count], from: vec![None; count], closed: vec![false; count], open: Default::default() }
    }

    /// Triangle `t` reached at `cost` (by `from`), `left` from the goal by
    /// the heuristic: opened if that is less than it had.
    fn seed(&mut self, t: usize, cost: f32, from: Option<(usize, Option<Jump>)>, left: f32) {
        if cost < self.cost[t] {
            self.cost[t] = cost;
            self.from[t] = from;
            self.open.push(Node(-(cost + left), t));
        }
    }
}

#[derive(PartialEq)]
struct Node(f32, usize);
impl Eq for Node {}
impl PartialOrd for Node {
    fn partial_cmp(&self, other: &Self) -> Option<std::cmp::Ordering> { Some(self.cmp(other)) }
}
impl Ord for Node {
    fn cmp(&self, other: &Self) -> std::cmp::Ordering { self.0.total_cmp(&other.0).then(self.1.cmp(&other.1)) }
}

fn shared_edge(mesh: &Layer, a: usize, b: usize) -> Option<(V3, V3)> {
    (0..3).find(|e| mesh.triangles[a].n[*e] == b as i32).map(|e| mesh.edge(a, e))
}

/// String-pulls a corridor of triangles to the corners of the shortest
/// path within it (the simple stupid funnel, on x and z).
fn funnel(mesh: &Layer, corridor: &[usize], start: V3, goal: V3, clearance: f32) -> Vec<V3> {
    let mut portals = vec![(start, start)];
    for pair in corridor.windows(2) {
        let (a, b) = shared_edge(mesh, pair[0], pair[1]).expect("corridor neighbours");
        // Kept `clearance` from the portal's ends (the actor's body), or its
        // middle when narrower.
        let length = a.planar(b);
        let keep = (clearance / length.max(1e-3)).min(0.5);
        let (a, b) = (a.add(b.sub(a).scale(keep)), b.add(a.sub(b).scale(keep)));
        // Left and right as seen walking from pair[0] into pair[1].
        let c = mesh.centroid(pair[0]);
        let cross = (a.x - c.x) * (b.z - c.z) - (a.z - c.z) * (b.x - c.x);
        portals.push(if cross > 0.0 { (a, b) } else { (b, a) });
    }
    portals.push((goal, goal));
    let area = |a: V3, b: V3, c: V3| (b.x - a.x) * (c.z - a.z) - (c.x - a.x) * (b.z - a.z);
    let same = |a: V3, b: V3| a.planar(b) < 1e-3;
    let mut corners = Vec::new();
    let (mut apex, mut left, mut right) = (start, start, start);
    let (mut left_i, mut right_i) = (0, 0);
    let mut i = 1;
    while i < portals.len() {
        let (pl, pr) = portals[i];
        if area(apex, right, pr) <= 0.0 {
            if same(apex, right) || area(apex, left, pr) > 0.0 {
                right = pr;
                right_i = i;
            } else {
                corners.push(left);
                apex = left;
                right = apex;
                right_i = left_i;
                i = left_i + 1;
                continue;
            }
        }
        if area(apex, left, pl) >= 0.0 {
            if same(apex, left) || area(apex, right, pl) < 0.0 {
                left = pl;
                left_i = i;
            } else {
                corners.push(right);
                apex = right;
                left = apex;
                left_i = right_i;
                i = right_i + 1;
                continue;
            }
        }
        i += 1;
    }
    corners.push(goal);
    corners.dedup_by(|a, b| a.planar(*b) < 0.5);
    corners
}

/// The point of the convex screen polygon `shown` nearest `p`, and how far
/// from `p` it is (0 inside it).
fn nearest_on_polygon(p: (f32, f32), shown: &[(f32, f32)]) -> ((f32, f32), f32) {
    let side = |u: (f32, f32), v: (f32, f32)| (v.0 - u.0) * (p.1 - u.1) - (v.1 - u.1) * (p.0 - u.0);
    let edges = || (0..shown.len()).map(|i| (shown[i], shown[(i + 1) % shown.len()]));
    let sides: Vec<f32> = edges().map(|(u, v)| side(u, v)).collect();
    if sides.iter().all(|d| *d >= 0.0) || sides.iter().all(|d| *d <= 0.0) {
        return (p, 0.0);
    }
    edges().map(|(u, v)| {
        let (ex, ey) = (v.0 - u.0, v.1 - u.1);
        let length = ex * ex + ey * ey;
        let t = if length < 1e-9 { 0.0 } else { (((p.0 - u.0) * ex + (p.1 - u.1) * ey) / length).clamp(0.0, 1.0) };
        let q = (u.0 + ex * t, u.1 + ey * t);
        (q, ((q.0 - p.0).powi(2) + (q.1 - p.1).powi(2)).sqrt())
    }).min_by(|x, y| x.1.total_cmp(&y.1)).expect("three edges")
}

/// Where the floor ray from `origin` along `direction` (unit, y 0) leaves
/// triangle `t` after `entered` along it: the distance, and the edge.
fn exit(mesh: &Layer, t: usize, origin: V3, direction: V3, entered: f32) -> Option<(f32, usize)> {
    (0..3).filter_map(|e| {
        let (a, b) = mesh.edge(t, e);
        let (ex, ez) = (b.x - a.x, b.z - a.z);
        let det = direction.x * ez - direction.z * ex;
        if det.abs() < 1e-9 {
            return None;
        }
        let (wx, wz) = (a.x - origin.x, a.z - origin.z);
        let s = (wx * ez - wz * ex) / det;
        let u = (wx * direction.z - wz * direction.x) / det;
        ((-1e-4..=1.0 + 1e-4).contains(&u) && s > entered + 1e-3).then_some((s, e))
    }).min_by(|x, y| x.0.total_cmp(&y.0))
}

/// The floor distance from `p` to the segment `a`-`b`, and where along it.
fn segment_planar(p: V3, a: V3, b: V3) -> (f32, f32) {
    let (abx, abz) = (b.x - a.x, b.z - a.z);
    let length = abx * abx + abz * abz;
    let t = if length < 1e-9 { 0.0 } else { (((p.x - a.x) * abx + (p.z - a.z) * abz) / length).clamp(0.0, 1.0) };
    (((a.x + abx * t - p.x).powi(2) + (a.z + abz * t - p.z).powi(2)).sqrt(), t)
}

/// Ray-triangle intersection (either side): the ray's parameter.
fn intersect(origin: V3, direction: V3, a: V3, b: V3, c: V3) -> Option<f32> {
    let (e1, e2) = (b.sub(a), c.sub(a));
    let p = direction.cross(e2);
    let det = e1.dot(p);
    if det.abs() < 1e-9 { return None; }
    let s = origin.sub(a);
    let u = s.dot(p) / det;
    if !(0.0..=1.0).contains(&u) { return None; }
    let q = s.cross(e1);
    let v = direction.dot(q) / det;
    if v < 0.0 || u + v > 1.0 { return None; }
    let t = e2.dot(q) / det;
    (t > 0.0).then_some(t)
}

/// Whether a picked floor point is walkable, needs a jump, or neither.
#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize)]
#[serde(rename_all = "snake_case")]
pub enum Reach {
    Walk,
    Jump,
    Invalid,
}

#[derive(Clone, Copy, Debug, PartialEq, Serialize)]
pub struct Hit {
    pub point: V3,
    pub triangle: usize,
    pub layer: usize,
    pub reach: Reach,
}

#[derive(Clone, Copy, Debug, PartialEq, Serialize)]
pub struct Jump {
    /// Where to take off, and where to land: the point to head for through
    /// the jump, letting go of the direction over it.
    pub takeoff: V3,
    pub toward: V3,
    /// The triangle the takeoff stands on.
    pub from: usize,
}

#[derive(Clone, Debug, PartialEq, Serialize)]
pub struct Route {
    /// The path's corners after the actor's position, the goal last.
    pub corners: Vec<V3>,
    pub jump: Option<Jump>,
    pub goal: V3,
    /// Its length on the floor (world units).
    pub length: f32,
}

/// The game's heading (0x1000 a turn) of a step `dx`, `dz`: the player
/// moves along (cos h, -sin h).
pub fn heading(dx: f32, dz: f32) -> i32 {
    let radians = (-dz).atan2(dx);
    ((radians / std::f32::consts::TAU * 4096.0).round() as i32).rem_euclid(4096)
}

/// The D-pad directions (a bit each: up, down, left, right) of the screen
/// direction `dir` (Right 0, Down 0x400, Left 0x800, Up 0xC00), in eighths.
pub fn dpad_of_sector(sector: i32) -> [bool; 4] {
    match sector.rem_euclid(8) {
        0 => [false, false, false, true],
        1 => [false, true, false, true],
        2 => [false, true, false, false],
        3 => [false, true, true, false],
        4 => [false, false, true, false],
        5 => [true, false, true, false],
        6 => [true, false, false, false],
        _ => [true, false, false, true],
    }
}

/// The screen direction (D-pad heading) that moves the player along world
/// heading `h` under the camera's `angle`.
pub fn screen_direction(h: i32, angle: i32, alt: bool) -> i32 {
    (h + angle + if alt { 0x800 } else { 0 }).rem_euclid(4096)
}

#[cfg(test)]
mod tests {
    use super::*;

    /// A strip of squares along x, each two triangles, 100 units a side.
    fn strip(squares: usize) -> Layer {
        let mut vertices = Vec::new();
        for i in 0..=squares {
            vertices.push(V3::new(i as f32 * 100.0, 0.0, 0.0));
            vertices.push(V3::new(i as f32 * 100.0, 0.0, 100.0));
        }
        let mut triangles = Vec::new();
        for i in 0..squares {
            let (a, b, c, d) = (i * 2, i * 2 + 1, i * 2 + 2, i * 2 + 3);
            let lower = (i * 2) as i32;
            // Lower: a, c, b (edges a-c, c-b, b-a); upper: c, d, b.
            triangles.push(Triangle { v: [a, c, b], n: [-1, lower + 1, if i > 0 { lower - 1 } else { -1 }], attribute: 0 });
            triangles.push(Triangle { v: [c, d, b], n: [if i + 1 < squares { lower + 2 } else { -1 }, -1, lower], attribute: 0 });
        }
        Layer { vertices, triangles, at: 0 }
    }

    fn actor(at: V3) -> Actor {
        Actor { index: 0, position: at, layer: 0, triangle: [-1; 4], flags: 0, layer_flags: 0, radius: 16, height: 64, heading: 0, heading_goal: 0 }
    }

    fn field(layer: Layer, attributes: Vec<u32>) -> Field {
        Field {
            camera: Camera { m: [[1.0, 0.0, 0.0], [0.0, 1.0, 0.0], [0.0, 0.0, 1.0]], t: [0.0, 0.0, 1000.0], h: 256.0, angle: 0 },
            layers: vec![layer],
            attributes,
            actors: vec![Some(actor(V3::new(50.0, 0.0, 50.0)))],
            player: Some(0),
            alt_dpad: false,
            party_processing: false,
            dialogue_open: false,
        }
    }

    #[test]
    fn a_straight_strip_routes_straight_to_the_goal() {
        let f = field(strip(5), vec![0]);
        let player = *f.player().unwrap();
        let goal = f.layers[0].locate(450.0, 50.0).unwrap();
        let route = f.route(&player, goal, V3::new(450.0, 0.0, 50.0)).unwrap();
        assert_eq!(route.corners, [V3::new(450.0, 0.0, 50.0)]);
        assert!(route.jump.is_none());
        assert!((route.length - 400.0).abs() < 60.0, "{}", route.length);
    }

    #[test]
    fn blocked_triangles_are_walked_around_or_refused() {
        let mut layer = strip(3);
        // An L: a second row above the strip's first square only; block
        // the strip's middle square for flags bit 8 (attribute 1: bit 5).
        layer.triangles[2].attribute = 1;
        layer.triangles[3].attribute = 1;
        let mut f = field(layer, vec![0, 0x20]);
        let mut player = *f.player().unwrap();
        player.flags = 0x100;
        f.actors[0] = Some(player);
        let goal = f.layers[0].locate(250.0, 50.0).unwrap();
        assert!(f.route(&player, goal, V3::new(250.0, 0.0, 50.0)).is_none(), "the only way is blocked");
        // The gap is within a jump: the route offers a takeoff... but only
        // where the mesh's own edge faces the goal; here the blocked squares
        // are mesh, not a gap, so nothing is offered.
        assert_eq!(f.reach(&player, goal, V3::new(250.0, 0.0, 50.0)), Reach::Invalid);
        // Without the flag, the same squares are open.
        player.flags = 0;
        assert!(f.route(&player, goal, V3::new(250.0, 0.0, 50.0)).is_some());
    }

    #[test]
    fn a_floor_beyond_a_slide_is_jumped_up_to_as_high_as_a_jump_rises_and_a_gap_as_far_as_it_carries() {
        // Squares 0 and 1 a floor, square 2 a slide rising `height` (y is
        // down) to square 3, a floor that high.
        let raised = |height: f32| {
            let mut layer = strip(4);
            for v in [6, 7, 8, 9] { layer.vertices[v].y = -height; }
            layer.triangles[4].attribute = 1;
            layer.triangles[5].attribute = 1;
            field(layer, vec![0, ATTR_LEDGE])
        };
        let f = raised(60.0);
        let player = *f.player().unwrap();
        let goal = f.layers[0].locate(350.0, 50.0).unwrap();
        let point = V3::new(350.0, -60.0, 50.0);
        let route = f.route(&player, goal, point).unwrap();
        let jump = route.jump.expect("a jump up over the slide");
        assert!((jump.takeoff.x - 200.0).abs() < 1.0 && jump.takeoff.z == 50.0, "from the slide's foot: {jump:?}");
        assert!(jump.toward.x > 300.0 && (jump.toward.y + 60.0).abs() < 1.0, "onto the raised floor: {jump:?}");
        assert_eq!(route.corners.last(), Some(&point));
        assert!(route.corners.iter().any(|c| c.planar(jump.takeoff) < 0.5), "the walk goes to the takeoff: {route:?}");
        assert_eq!(f.reach(&player, goal, point), Reach::Jump);
        // The slide is no floor to stand on: walked onto from its foot only.
        let on_slide = f.layers[0].locate(250.0, 50.0).unwrap();
        assert!(f.reachable(&player)[on_slide] && f.reachable(&player)[goal]);
        // Higher than a jump rises (118.25): no way up.
        let f = raised(150.0);
        assert!(f.route(&player, goal, V3::new(350.0, -150.0, 50.0)).is_none());
        assert!(!f.reachable(&player)[goal]);
        // Two strips with a gap between them.
        let mut layer = strip(2);
        let mut far = strip(2);
        let offset = layer.vertices.len();
        for v in &mut far.vertices { v.x += 300.0; }
        for t in &mut far.triangles {
            t.v = t.v.map(|v| v + offset);
            t.n = t.n.map(|n| if n < 0 { n } else { n + 4 });
        }
        layer.vertices.extend(far.vertices);
        layer.triangles.extend(far.triangles);
        let f = field(layer, vec![0]);
        let goal = f.layers[0].locate(400.0, 50.0).unwrap();
        let route = f.route(&player, goal, V3::new(400.0, 0.0, 50.0)).unwrap();
        let jump = route.jump.expect("a takeoff at the gap");
        assert!((jump.takeoff.x - 200.0).abs() < 1.0, "{jump:?}");
        assert_eq!(f.reach(&player, goal, V3::new(400.0, 0.0, 50.0)), Reach::Jump);
    }

    #[test]
    fn the_funnel_cuts_corners_inside_an_l() {
        // Squares (0,0), (1,0), (1,1): from the first to the third the path
        // bends once, at the shared corner (200, 100).
        let mut l = strip(2);
        let base = l.vertices.len();
        l.vertices.push(V3::new(100.0, 0.0, 200.0));
        l.vertices.push(V3::new(200.0, 0.0, 200.0));
        // Square (1,1) over the strip's second square's top edge (3-5).
        l.triangles[3].n[1] = 4;
        l.triangles.push(Triangle { v: [5, 3, base], n: [3, -1, 5], attribute: 0 });
        l.triangles.push(Triangle { v: [5, base, base + 1], n: [4, -1, -1], attribute: 0 });
        let f = field(l, vec![0]);
        let player = *f.player().unwrap();
        let goal = f.layers[0].locate(150.0, 180.0).unwrap();
        let route = f.route(&player, goal, V3::new(150.0, 0.0, 180.0)).unwrap();
        // One bend near the inner corner (100, 100), kept a body's width
        // (16) off it.
        assert_eq!(route.corners.len(), 2, "{route:?}");
        let bend = route.corners[0];
        let off = bend.planar(V3::new(100.0, 0.0, 100.0));
        assert!((8.0..=24.0).contains(&off) && bend.x > 100.0 && bend.z < 100.0, "{route:?}");
    }

    #[test]
    fn rays_pick_the_floor_and_projection_inverts() {
        let f = field(strip(5), vec![0]);
        let camera = Camera { m: [[1.0, 0.0, 0.0], [0.0, 0.0, -1.0], [0.0, 1.0, 0.0]], t: [-250.0, 0.0, 600.0], h: 256.0, angle: 0 };
        // y (down) maps to view z: the camera looks down at the floor.
        let f = Field { camera, ..f };
        let point = V3::new(300.0, 0.0, 70.0);
        let screen = f.camera.project(point).unwrap();
        let hit = f.pick(screen).unwrap();
        assert!(hit.point.planar(point) < 0.5, "{hit:?}");
        assert_eq!(hit.reach, Reach::Walk);
    }

    #[test]
    fn world_rays_pick_the_floor_within_their_reach_and_the_actor_beside_them() {
        let mut f = field(strip(5), vec![0]);
        // From above the strip and ahead of it, down at (300, 0, 70) (y down).
        let goal = V3::new(300.0, 0.0, 70.0);
        let origin = V3::new(100.0, -400.0, 70.0);
        let direction = goal.sub(origin).scale(0.5);
        let hit = f.pick_ray(origin, direction, 4.0).unwrap();
        assert!(hit.point.planar(goal) < 0.5 && hit.reach == Reach::Walk, "{hit:?}");
        assert!(f.pick_ray(origin, direction, 1.9).is_none(), "the floor lies beyond the ray's part in reach");
        assert!(f.pick_ray(origin, direction.scale(-1.0), 10.0).is_none(), "nothing behind it");
        // An actor standing beside the ray's way down, another far off.
        let other = |index: usize, at: V3| Actor { index, ..actor(at) };
        f.actors.push(Some(other(1, V3::new(310.0, 0.0, 85.0))));
        f.actors.push(Some(other(2, V3::new(480.0, 0.0, 20.0))));
        assert_eq!(f.actor_near_ray(origin, direction, 4.0, 20.0), Some(1));
        assert_eq!(f.actor_near_ray(origin, direction, 4.0, 5.0), None, "beyond its reach");
        assert_eq!(f.actor_near_ray(origin, direction, 1.0, 20.0), None, "its part of the ray stops short of them");
        // The player is not pointed at, nor an actor the game skips.
        f.actors[1] = Some(Actor { flags: 1, ..f.actors[1].unwrap() });
        assert_eq!(f.actor_near_ray(V3::new(50.0, -400.0, 50.0), V3::new(0.0, 100.0, 0.0), 10.0, 20.0), None);
    }

    #[test]
    fn a_camera_seen_from_another_view_projects_and_picks_as_its_picture_shows() {
        let f = field(strip(5), vec![0]);
        let identity = [[1.0, 0.0, 0.0], [0.0, 1.0, 0.0], [0.0, 0.0, 1.0]];
        let camera = Camera { m: [[1.0, 0.0, 0.0], [0.0, 0.0, -1.0], [0.0, 1.0, 0.0]], t: [-250.0, 0.0, 600.0], h: 256.0, angle: 0x300 };
        let point = V3::new(300.0, 0.0, 70.0);
        let view = camera.view(point);
        let near = |a: (f32, f32), b: (f32, f32)| (a.0 - b.0).abs() < 1e-3 && (a.1 - b.1).abs() < 1e-3;
        // Pulled back 900 units along its axis, about the picture's own
        // centre (8 lines below the screen's).
        let pulled = camera.seen(identity, [0.0, 0.0, 900.0], 256.0, (160.0, 120.0));
        let shown = pulled.project(point).unwrap();
        let z = view[2] + 900.0;
        assert!(near(shown, (160.0 + 256.0 * view[0] / z, 120.0 + 256.0 * view[1] / z)), "{shown:?}");
        assert_eq!(pulled.angle, 0x300, "the game's own angle");
        let f = Field { camera: pulled, ..f };
        let hit = f.pick(shown).unwrap();
        assert!(hit.point.planar(point) < 0.5, "the pulled-back picture's point picks the floor shown there: {hit:?}");
        // Looking from within the scene a quarter turn to the right (its
        // forward the camera's right), from 500 units ahead.
        let rows = [[0.0, 0.0, -1.0], [0.0, 1.0, 0.0], [1.0, 0.0, 0.0]];
        let eye = [0.0, 0.0, 500.0];
        let offset = [0, 1, 2].map(|r| -(0..3).map(|k| rows[r][k] * eye[k]).sum::<f32>());
        let looking = camera.seen(rows, offset, 200.0, (160.0, 112.0));
        let q = [0, 1, 2].map(|r| (0..3).map(|k| rows[r][k] * (view[k] - eye[k])).sum::<f32>());
        let shown = looking.project(point).unwrap();
        assert!(near(shown, (160.0 + 200.0 * q[0] / q[2], 112.0 + 200.0 * q[1] / q[2])), "{shown:?} for {q:?}");
        let (origin, direction) = looking.ray(shown).unwrap();
        let along = point.sub(origin).dot(direction) / direction.dot(direction);
        let off = point.sub(origin.add(direction.scale(along)));
        assert!(off.dot(off).sqrt() < 1e-2, "the look's ray through its picture's point meets the point: {off:?}");
    }

    #[test]
    fn headings_and_dpad_directions() {
        assert_eq!(heading(1.0, 0.0), 0);
        assert_eq!(heading(0.0, -1.0), 0x400);
        assert_eq!(heading(-1.0, 0.0), 0x800);
        assert_eq!(dpad_of_sector(0), [false, false, false, true]);
        assert_eq!(dpad_of_sector(6), [true, false, false, false]);
        assert_eq!(screen_direction(0x400, 0x100, false), 0x500);
        assert_eq!(screen_direction(0x400, 0, true), 0xC00);
    }
}
