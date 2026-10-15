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
//! meet its bits 5-7, and bit 23 blocks layer 0; bit 22 marks a ledge that
//! is entered from below only by jumping; the word reads 0 while the
//! actor's `layer_flags` bit (layer + 3) is set.
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
    fn sub(self, o: V3) -> V3 { V3::new(self.x - o.x, self.y - o.y, self.z - o.z) }
    fn add(self, o: V3) -> V3 { V3::new(self.x + o.x, self.y + o.y, self.z + o.z) }
    fn scale(self, s: f32) -> V3 { V3::new(self.x * s, self.y * s, self.z * s) }
    fn dot(self, o: V3) -> f32 { self.x * o.x + self.y * o.y + self.z * o.z }
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
            layers.push(Layer { vertices, triangles });
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

    /// Whether `actor` may stand on triangle `t` at all.
    fn blocked(&self, layer: usize, t: usize, actor: &Actor) -> bool {
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
    /// way, or cannot reach it.
    pub fn reach(&self, actor: &Actor, goal: usize, point: V3) -> Reach {
        match self.route(actor, goal, point) {
            Some(route) if route.jump.is_some() => Reach::Jump,
            Some(_) => Reach::Walk,
            None if self.blocked(actor.layer, goal, actor) => Reach::Invalid,
            None => Reach::Invalid,
        }
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
        let layer = actor.layer;
        let mesh = self.layers.get(layer)?;
        let start = self.start(actor)?;
        if goal >= mesh.triangles.len() || self.blocked(layer, goal, actor) {
            return None;
        }
        // A* over triangles; entering a ledge higher than where one stands
        // needs a jump there.
        let count = mesh.triangles.len();
        let mut cost = vec![f32::INFINITY; count];
        let mut from: Vec<Option<(usize, bool)>> = vec![None; count];
        let mut open = std::collections::BinaryHeap::new();
        cost[start] = 0.0;
        open.push(Node(-mesh.centroid(start).planar(point), start));
        while let Some(Node(_, t)) = open.pop() {
            if t == goal {
                break;
            }
            for e in 0..3 {
                let next = mesh.triangles[t].n[e];
                if next < 0 { continue; }
                let next = next as usize;
                if self.blocked(layer, next, actor) { continue; }
                let climb = self.ledge(layer, next, actor) && !self.ledge(layer, t, actor)
                    && mesh.centroid(next).y < mesh.centroid(t).y - 8.0;
                let (a, b) = mesh.edge(t, e);
                let mid = a.add(b).scale(0.5);
                let step = mesh.centroid(t).planar(mid) + mid.planar(mesh.centroid(next)) + if climb { 200.0 } else { 0.0 };
                if cost[t] + step < cost[next] {
                    cost[next] = cost[t] + step;
                    from[next] = Some((t, climb));
                    open.push(Node(-(cost[next] + mesh.centroid(next).planar(point)), next));
                }
            }
        }
        if cost[goal].is_infinite() {
            return self.route_to_gap(actor, start, goal, point);
        }
        let mut corridor = vec![goal];
        let mut jump = None;
        let mut t = goal;
        while let Some((previous, climb)) = from[t] {
            if climb { jump = Some(previous); }
            corridor.push(previous);
            t = previous;
        }
        corridor.reverse();
        let corners = funnel(mesh, &corridor, actor.position, point, f32::from(actor.radius).clamp(4.0, 24.0));
        let jump = jump.map(|takeoff| {
            let i = corridor.iter().position(|c| *c == takeoff).expect("on the corridor");
            let (a, b) = shared_edge(mesh, takeoff, corridor[i + 1]).expect("neighbours");
            Jump { takeoff: a.add(b).scale(0.5), toward: mesh.centroid(corridor[i + 1]) }
        });
        let length = path_length(actor.position, &corners);
        Some(Route { corners, jump, goal: point, length })
    }

    /// The goal lies on a part of the mesh the actor cannot walk to: walk to
    /// the reachable boundary edge nearest it (within a jump's reach), the
    /// jump the player's own.
    fn route_to_gap(&self, actor: &Actor, start: usize, goal: usize, point: V3) -> Option<Route> {
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
        let mut seen = vec![false; mesh.triangles.len()];
        let mut stack = vec![start];
        seen[start] = true;
        let mut best: Option<(f32, usize, V3)> = None;
        while let Some(t) = stack.pop() {
            for e in 0..3 {
                let next = mesh.triangles[t].n[e];
                let (a, b) = mesh.edge(t, e);
                if next < 0 {
                    let mid = a.add(b).scale(0.5);
                    let d = mid.planar(point);
                    if best.is_none_or(|(bd, _, _)| d < bd) { best = Some((d, t, mid)); }
                } else if !seen[next as usize] && !self.blocked(actor.layer, next as usize, actor) {
                    seen[next as usize] = true;
                    stack.push(next as usize);
                }
            }
        }
        let (distance, takeoff_triangle, takeoff) = best?;
        if distance > JUMP_REACH || seen[goal] {
            return None;
        }
        let mut route = self.route(actor, takeoff_triangle, takeoff)?;
        route.jump = Some(Jump { takeoff, toward: point });
        route.goal = point;
        route.corners.push(point);
        route.corners.dedup_by(|a, b| a.planar(*b) < 0.5);
        route.length = path_length(actor.position, &route.corners);
        Some(route)
    }
}

/// The length of a path from `from` through `corners` on the floor.
pub fn path_length(from: V3, corners: &[V3]) -> f32 {
    corners.iter().fold((from, 0.0), |(at, sum), c| (*c, sum + at.planar(*c))).1
}

/// The farthest a gap to the goal may be for a takeoff to be offered.
const JUMP_REACH: f32 = 300.0;

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
    /// Where to take off, and the point to head for through the jump.
    pub takeoff: V3,
    pub toward: V3,
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
        Layer { vertices, triangles }
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
    fn a_higher_ledge_needs_a_jump_and_a_gap_offers_a_takeoff() {
        let mut layer = strip(3);
        // The last square is a ledge 40 units up (y is down).
        for v in [6, 7] { layer.vertices[v].y = -40.0; }
        layer.triangles[4].attribute = 1;
        layer.triangles[5].attribute = 1;
        let f = field(layer, vec![0, ATTR_LEDGE]);
        let player = *f.player().unwrap();
        let goal = f.layers[0].locate(250.0, 50.0).unwrap();
        let route = f.route(&player, goal, V3::new(250.0, 0.0, 50.0)).unwrap();
        let jump = route.jump.expect("a jump onto the ledge");
        assert!((jump.takeoff.x - 200.0).abs() < 1.0, "{jump:?}");
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
