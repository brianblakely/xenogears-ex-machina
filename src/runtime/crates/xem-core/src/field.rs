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
/// A floor that lifts an actor standing on it to the next floor up
/// (field_actor_update_position).
const ATTR_LIFT: u32 = 0x4;
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
    pub attribute: u16,
    /// How far below its floor the triangle reaches, solid (its byte 13
    /// times 4, field_collision_find_actor_floor's range; none at 0 or
    /// less): another layer's actor whose body meets it may not move there.
    pub bump: i16,
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
                triangles.push(Triangle { v, n, attribute: u16::from(b[12]), bump: i16::from(b[13] as i8) * 4 });
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
        self.others(actor, 0x01)
    }

    /// [`Field::obstacles`] a walk goes round: all but the party's
    /// followers (flags bit 24: field_event_request_player_control marks the
    /// party members it does not move, and field_actor_update_position
    /// leaves them to the leader's path), which follow it.
    pub fn in_the_way(&self, actor: &Actor) -> Vec<(V3, f32)> {
        self.others(actor, 0x0100_0001)
    }

    /// [`Field::obstacles`] that stop the actor's body, where a jump may not
    /// take off or land: field_actor_resolve_contacts passes a pair by when
    /// either passes through others (flags bit 7: a door's trigger box, the
    /// party following the leader), so those are none.
    pub fn bodies(&self, actor: &Actor) -> Vec<(V3, f32)> {
        if actor.flags & 0x80 != 0 {
            return Vec::new();
        }
        self.others(actor, 0x81)
    }

    fn others(&self, actor: &Actor, skipped: u32) -> Vec<(V3, f32)> {
        self.actors.iter().flatten()
            .filter(|o| o.index != actor.index && o.flags & skipped == 0 && o.layer == actor.layer && o.position.planar(actor.position) > 2.0)
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

    /// Whether `actor` may not start a jump on triangle `t` of `layer`, nor
    /// move over it in the air: its attribute's bits 3-4 against the
    /// actor's flags' bits 9-10 (field_actor_is_jump_blocked_by_floor; bit
    /// 4 on foot, bit 3 in a gear, as tools/analysis/platforming.py reads
    /// them).
    fn grounded(&self, layer: usize, t: usize, actor: &Actor) -> bool {
        (actor.flags >> 9) & 3 & (self.attribute(layer, t, actor) >> 3) != 0
    }

    /// The layers actors stand on: all but the top one, the camera's floor
    /// (field_collision_find_actor_floor's), or the only one.
    pub fn actor_layers(&self) -> usize {
        if self.layers.len() > 1 { self.layers.len() - 1 } else { self.layers.len() }
    }

    /// The floor point a screen point picks: the nearest walkmesh triangle
    /// the ray meets on the layers actors stand on.
    pub fn pick(&self, screen: (f32, f32)) -> Option<Hit> {
        let (origin, direction) = self.camera.ray(screen)?;
        self.pick_ray(origin, direction, f32::INFINITY)
    }

    /// The floor point a ray in the world picks (a ray through a diorama of
    /// the field): the nearest walkmesh triangle it meets from `origin` to
    /// `within` lengths of `direction` along it, on any layer actors stand
    /// on (through a pit, a hole in an upper layer, to what lies below; as
    /// near on the player's layer and another, the player's).
    pub fn pick_ray(&self, origin: V3, direction: V3, within: f32) -> Option<Hit> {
        let player = self.player()?;
        // The nearest (as near on the player's layer and another, the
        // player's; then the first layer's, the first triangle's), among the
        // triangles under the ray's stretch within each layer's heights,
        // cell by cell along it ([`Grid::along`]) until no nearer can lie
        // on.
        let mut best: Option<(f32, usize, usize)> = None;
        let mut look = |layer: usize, grid: &Grid| {
            let mesh = &self.layers[layer];
            let mine = if layer == player.layer { 1.0 } else { 0.0 };
            let (a, b) = if direction.y.abs() > 1e-9 {
                let (a, b) = ((grid.y0 - origin.y) / direction.y, (grid.y1 - origin.y) / direction.y);
                (a.min(b), a.max(b))
            } else if (grid.y0..=grid.y1).contains(&origin.y) {
                (0.0, within)
            } else {
                return;
            };
            grid.along(origin, direction, a.max(0.0), b.min(within), |entered, triangles| {
                if best.is_some_and(|(d, ..)| entered - mine >= d) {
                    return false;
                }
                for t in triangles.iter().map(|t| *t as usize) {
                    let [a, b, c] = mesh.corners(t);
                    if let Some(distance) = intersect(origin, direction, a, b, c).filter(|d| *d <= within)
                        && best.is_none_or(|(d, l, u)| (distance - mine, layer, t) < (d, l, u))
                        && !(layer > 0 && self.attribute(layer, t, player) & ATTR_LAYER0_BLOCK != 0)
                    {
                        best = Some((distance - mine, layer, t));
                    }
                }
                true
            });
        };
        let layers = (0..self.actor_layers()).filter(|l| *l >= 3 || player.layer_flags & (1 << l) == 0);
        // (Each walkmesh read from the game keeps its grids while the map
        // lasts; one built for the asking has none.)
        thread_local! {
            static GRIDS: std::cell::RefCell<Vec<((u32, usize), Grid)>> = Default::default();
        }
        let meshes = &self.layers[..self.actor_layers()];
        if meshes.iter().all(|mesh| mesh.at != 0) {
            GRIDS.with_borrow_mut(|grids| {
                let same = grids.len() == meshes.len() && grids.iter().zip(meshes).all(|((key, _), mesh)| *key == (mesh.at, mesh.triangles.len()));
                if !same {
                    *grids = meshes.iter().map(|mesh| ((mesh.at, mesh.triangles.len()), Grid::of(mesh))).collect();
                }
                for layer in layers {
                    look(layer, &grids[layer].1);
                }
            });
        } else {
            for layer in layers {
                look(layer, &Grid::of(&self.layers[layer]));
            }
        }
        let (_, layer, triangle) = best?;
        let [a, b, c] = self.layers[layer].corners(triangle);
        let point = origin.add(direction.scale(intersect(origin, direction, a, b, c)?));
        Some(Hit { point, triangle, layer, reach: self.reach(player, layer, triangle, point) })
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

    /// Whether the player can walk to `point` on triangle `goal` of `layer`,
    /// must jump on the way, or cannot reach it. A pointer resting on the
    /// floor asks it every frame: the answers for a game walkmesh are kept a
    /// while (by the actor's layer's place and count, its triangle and flags,
    /// the goal and the collision attributes, which scripts may rewrite:
    /// field_collision_set_attribute_byte; the walkmesh holds still within a
    /// map).
    pub fn reach(&self, actor: &Actor, layer: usize, goal: usize, point: V3) -> Reach {
        let mesh = self.layers.get(actor.layer);
        let attributes = self.attributes.iter().fold(0u64, |h, word| (h.rotate_left(7) ^ u64::from(*word)).wrapping_mul(0x9E37_79B9_7F4A_7C15));
        let key = mesh.filter(|l| l.at != 0).zip(self.start(actor))
            .map(|(l, start)| (l.at, l.triangles.len(), start, layer, goal, actor.flags, actor.layer_flags, attributes));
        thread_local! {
            static KNOWN: std::cell::RefCell<std::collections::VecDeque<(ReachKey, Reach)>> = Default::default();
        }
        if let Some(reach) = key.and_then(|key| KNOWN.with_borrow(|known| known.iter().find(|(k, _)| *k == key).map(|(_, r)| *r))) {
            return reach;
        }
        let reach = match self.route(actor, layer, goal, point) {
            Some(route) if route.jumps => Reach::Jump,
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

    /// Whether the actor stands on a slide (where the game moves it down
    /// the slope and the D-pad gives it no direction).
    pub fn sliding(&self, actor: &Actor) -> bool {
        self.start(actor).is_some_and(|t| self.ledge(actor.layer, t, actor))
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

    /// The route from the actor to `point` on triangle `goal` of `layer`:
    /// the triangles' corridor on the actor's layer, string-pulled to
    /// corners, up to where the way leaves it (a jump needed: a ledge
    /// entered from below, or a gap to the goal's part of the mesh; a floor
    /// that lifts the actor to the layer above, a pit it drops through).
    pub fn route(&self, actor: &Actor, layer: usize, goal: usize, point: V3) -> Option<Route> {
        LEAPS.with_borrow_mut(|leaps| self.route_with(actor, layer, goal, point, leaps))
    }

    /// [`Field::route`], with the jumps and the ways between layers found
    /// kept in `leaps` for the next search of this walkmesh (the assist
    /// plans its route again each frame; the walkmesh does not change under
    /// it).
    pub fn route_with(&self, actor: &Actor, layer: usize, goal: usize, point: V3, leaps: &mut Leaps) -> Option<Route> {
        let start = self.start(actor)?;
        let mesh = self.layers.get(layer).filter(|_| layer < self.actor_layers())?;
        if goal >= mesh.triangles.len() || self.blocked(layer, goal, actor) || (layer > 0 && self.attribute(layer, goal, actor) & ATTR_LAYER0_BLOCK != 0) {
            return None;
        }
        leaps.keep_for(self, actor, jump_arc(actor));
        let mut graph = leaps.graph.take().unwrap_or_else(|| Graph::of(self, actor));
        let route = self.route_on(actor, &mut graph, start, layer, goal, point, leaps);
        leaps.graph = Some(graph);
        route
    }

    /// [`Field::route_with`] on the walkmesh's `graph`.
    #[allow(clippy::too_many_arguments)]
    fn route_on(&self, actor: &Actor, graph: &mut Graph, start: usize, layer: usize, goal: usize, point: V3, leaps: &mut Leaps) -> Option<Route> {
        let nodes = graph.nodes.clone();
        let nodes = &nodes;
        let (from, to) = (nodes.index(actor.layer, start), nodes.index(layer, goal));
        let key = (layer, goal, [point.x.to_bits(), point.y.to_bits(), point.z.to_bits()]);
        // Along the path kept, from where the actor stands on it, while no
        // body stands on a jump of it.
        let bodies = self.bodies(actor);
        let clear = |way: &Way| match way {
            Way::Jump(jump) => bodies.iter().all(|(centre, keep)| centre.planar(jump.takeoff) >= *keep && centre.planar(jump.toward) >= *keep),
            _ => true,
        };
        let kept = leaps.paths.iter().find(|(known, _)| *known == key)
            .and_then(|(_, path)| path.iter().position(|(n, ..)| *n == from).map(|i| path[i..].to_vec()))
            .filter(|path| path.iter().all(|(_, way, _)| clear(way)));
        if let Some(path) = kept {
            return Some(self.route_along(actor, nodes, &path, to, point));
        }
        // A* over the layers' triangles ([`Field::search`]) walking (to a
        // neighbour, lifted to the floor above, dropping through a pit);
        // where no walk reaches the goal, walking and jumping (a walk wins
        // wherever there is one, and asks for no flight). A search that ran
        // to its end is kept: every other goal from its start is a path back
        // through it, and from a start it reached, what it did not reach is
        // out of reach (all it reaches, that one reached).
        let walked_here = leaps.walked.as_ref().is_some_and(|(n, _)| *n == from);
        if let Some((_, walked)) = leaps.walked.as_ref().filter(|_| walked_here)
            && walked.cost[to].is_finite()
        {
            let path = walked.back(to);
            return Some(self.route_along(actor, nodes, &path, to, point));
        }
        let no_walk = walked_here || leaps.walked.as_ref().is_some_and(|(_, walked)| walked.cost[from].is_finite() && walked.cost[to].is_infinite());
        if !no_walk {
            let mut search = leaps.spare.take().unwrap_or_default().reset(nodes.total);
            search.seed(from, 0.0, NONE, Way::Walk, graph.centre[from].planar(point));
            self.search(actor, graph, &mut search, to, point, leaps, None);
            if search.cost[to].is_finite() {
                return Some(self.keep(actor, nodes, search, key, to, point, leaps));
            }
            leaps.spare = leaps.walked.replace((from, search)).map(|(_, old)| old);
        }
        if let Some((n, _, search)) = leaps.expanded.as_ref().filter(|(_, others, _)| *others == bodies)
            && (*n == from || (search.cost[from].is_finite() && search.cost[to].is_infinite()))
        {
            if search.cost[to].is_infinite() {
                return (layer == actor.layer).then(|| self.route_to_gap(actor, graph, start, goal, point, leaps)).flatten();
            }
            let path = search.back(to);
            return Some(self.route_along(actor, nodes, &path, to, point));
        }
        let mut search = leaps.spare.take().unwrap_or_default().reset(nodes.total);
        search.seed(from, 0.0, NONE, Way::Walk, graph.centre[from].planar(point));
        self.search(actor, graph, &mut search, to, point, leaps, Some(&bodies));
        if search.cost[to].is_finite() {
            return Some(self.keep(actor, nodes, search, key, to, point, leaps));
        }
        leaps.spare = leaps.expanded.replace((from, bodies, search)).map(|(.., old)| old);
        (layer == actor.layer).then(|| self.route_to_gap(actor, graph, start, goal, point, leaps)).flatten()
    }

    /// The search run to its end from node `from` among other actors'
    /// bodies standing where `bodies` says, walking and jumping: all the
    /// actor reaches from there. Kept in `leaps` ([`Field::route_on`]).
    fn expanded<'a>(&self, actor: &Actor, graph: &mut Graph, from: usize, bodies: &[(V3, f32)], leaps: &'a mut Leaps) -> &'a Search {
        if !leaps.expanded.as_ref().is_some_and(|(n, others, _)| *n == from && others == bodies) {
            let mut search = leaps.spare.take().unwrap_or_default().reset(graph.nodes.total);
            search.seed(from, 0.0, NONE, Way::Walk, 0.0);
            self.search(actor, graph, &mut search, NONE as usize, graph.centre[from], leaps, Some(bodies));
            leaps.spare = leaps.expanded.replace((from, bodies.to_vec(), search)).map(|(.., old)| old);
        }
        &leaps.expanded.as_ref().expect("just expanded").2
    }

    /// The route back through `search` to node `to` at `point`, its path
    /// kept under `key` ([`Leaps`]) and the search's buffers lent on.
    #[allow(clippy::too_many_arguments)]
    fn keep(&self, actor: &Actor, nodes: &Nodes, search: Search, key: (usize, usize, [u32; 3]), to: usize, point: V3, leaps: &mut Leaps) -> Route {
        let path = search.back(to);
        leaps.spare = Some(search);
        let route = self.route_along(actor, nodes, &path, to, point);
        if leaps.paths.len() == PATHS {
            leaps.paths.pop_front();
        }
        leaps.paths.push_back((key, path));
        route
    }

    /// The route along `path` (each node from the actor's, the way out of
    /// it and the cost to it) to node `to` at `point`: its stretch on the
    /// actor's layer string-pulled up to where the way leaves it.
    fn route_along(&self, actor: &Actor, nodes: &Nodes, path: &[(usize, Way, f32)], to: usize, point: V3) -> Route {
        let jumps = path.iter().any(|(_, way, _)| matches!(way, Way::Jump(_)));
        let leaves = path.iter().position(|(_, way, _)| *way != Way::Walk).filter(|i| i + 1 < path.len());
        let mesh = &self.layers[actor.layer];
        let clearance = f32::from(actor.radius).clamp(4.0, 24.0);
        let mut corridor: Vec<usize> = path[..=leaves.unwrap_or(path.len() - 1)].iter().map(|(n, ..)| nodes.split(*n).1).collect();
        let Some(leaves) = leaves else {
            let corners = funnel(mesh, &corridor, actor.position, point, clearance);
            let length = path_length(actor.position, &corners);
            return Route { corners, jump: None, jumps, goal: point, length };
        };
        // What the rest of the way costs past where it leaves this layer.
        let rest = path.last().map_or(0.0, |(.., c)| *c) - path[leaves + 1].2;
        let (mut corners, jump) = match path[leaves].1 {
            // The walk up to the takeoff (square to its edge from a little
            // way in), the jump to its landing.
            Way::Jump(jump) => {
                let approach = self.approach(actor, &jump);
                let mut corners = funnel(mesh, &corridor, actor.position, approach.unwrap_or(jump.takeoff), clearance);
                corners.extend(approach.map(|_| jump.takeoff));
                corners.push(jump.toward);
                (corners, Some(jump))
            }
            // Onto the floor that lifts the actor.
            Way::Lift => (funnel(mesh, &corridor, actor.position, mesh.centroid(*corridor.last().expect("a floor")), clearance), None),
            // Into the pit it drops through.
            Way::Drop(pit) => {
                corridor.push(pit);
                (funnel(mesh, &corridor, actor.position, mesh.centroid(pit), clearance), None)
            }
            Way::Walk => unreachable!("where the way leaves the layer"),
        };
        // (A jump onto the goal's floor goes on from its landing to the goal.)
        let rest = if path[leaves + 1].0 == to { corners.last().map_or(0.0, |c| c.planar(point)) } else { rest };
        let length = path_length(actor.position, &corners) + rest;
        corners.push(point);
        corners.dedup_by(|a, b| a.planar(*b) < 0.5);
        Route { corners, jump, jumps, goal: point, length }
    }

    /// A* over `search` toward node `goal` (at `point`; `NONE` for none),
    /// from what it holds open: each node's cost and the way into it (from
    /// where, and how), on the walkmesh's `graph` (its nodes worked out as
    /// reached). Walks to a neighbour, lifts to the floor above and drops
    /// through pits count as steps; with `others` (other actors' bodies)
    /// jumps over slides too high to walk onto, clear of them, too, a jump
    /// JUMP_COST more than its length (a walk round is better). It runs
    /// until the goal, else until all it reaches is reached; a node expanded
    /// once stays so (the heuristic, the floor distance to the goal, never
    /// overestimates a step). A jump's flight is found only once the least
    /// a jump from there can cost is the least open (JUMP_COST and the
    /// floor distance to the goal: its length, out to its landing's middle,
    /// is no less than how much nearer that lies): where a walk gets there
    /// for less, no flight is found.
    #[allow(clippy::too_many_arguments)]
    fn search(&self, actor: &Actor, graph: &mut Graph, search: &mut Search, goal: usize, point: V3, leaps: &mut Leaps, others: Option<&[(V3, f32)]>) {
        while let Some(Open(_, entry)) = search.open.pop() {
            let (n, edge) = (entry >> 2, entry & 3);
            if edge != 0 {
                // A jump from node `n` over its edge `edge - 1`.
                let (layer, t) = graph.nodes.split(n);
                graph.grids(self, layer, true);
                if let Some((next, jump)) = self.leap(actor, leaps, &graph.grids, others.unwrap_or_default(), layer, t, edge - 1) {
                    let next = graph.nodes.index(layer, next);
                    let step = graph.centre[n].planar(jump.takeoff) + jump.takeoff.planar(jump.toward) + jump.toward.planar(graph.centre[next]) + JUMP_COST;
                    if !search.closed[next] {
                        search.seed(next, search.cost[n] + step, n as u32, Way::Jump(jump), graph.centre[next].planar(point));
                    }
                }
                continue;
            }
            if n == goal {
                break;
            }
            if std::mem::replace(&mut search.closed[n], true) {
                continue;
            }
            graph.node(self, n);
            let cost = search.cost[n];
            for e in 0..3 {
                let (next, step, way) = match graph.ways[n][e] {
                    (next, step, NONE) if next != NONE => (next as usize, step, Way::Walk),
                    (next, step, pit) if next != NONE => (next as usize, step, Way::Drop(pit as usize)),
                    _ if others.is_some() && graph.leaps[n] & (1 << e) != 0 => {
                        search.open.push(Open((cost + JUMP_COST + graph.centre[n].planar(point)).to_bits(), n << 2 | (e + 1)));
                        continue;
                    }
                    _ => continue,
                };
                if !search.closed[next] {
                    search.seed(next, cost + step, n as u32, way, graph.centre[next].planar(point));
                }
            }
            if let (above, step) = graph.lifts[n]
                && above != NONE
                && !search.closed[above as usize]
            {
                search.seed(above as usize, cost + step, n as u32, Way::Lift, graph.centre[above as usize].planar(point));
            }
        }
        // Left open for a search that goes on from here.
        search.open.clear();
    }

    /// A jump from floor `t`'s edge `e` of `layer` over the slide beyond it,
    /// too high to walk onto, straight out from the edge: the floor it lands
    /// on and where ([`Field::flight`]). None where the flight meets a
    /// slide, a triangle the actor may not cross or the mesh's edge first,
    /// and where another actor's body (`others`: centres and the distances
    /// kept from them) stands on the takeoff or the landing. No jump starts
    /// on a slide.
    #[allow(clippy::too_many_arguments)]
    fn leap(&self, actor: &Actor, leaps: &mut Leaps, grids: &[Option<Grid>], others: &[(V3, f32)], layer: usize, t: usize, e: usize) -> Option<(usize, Jump)> {
        // From a floor onto a slide it may cross (the edges worth a flight).
        let mesh = &self.layers[layer];
        let first = usize::try_from(mesh.triangles[t].n[e]).ok().filter(|n| *n < mesh.triangles.len())?;
        if self.ledge(layer, t, actor) || self.grounded(layer, t, actor) || !self.ledge(layer, first, actor) || self.blocked(layer, first, actor) {
            return None;
        }
        let arc = leaps.arc;
        let (next, jump) = (*leaps.found.entry((layer, t, e)).or_insert_with(|| self.flight(actor, &Arc(arc), grids, layer, t, e)))?;
        let kept = |p: V3| others.iter().all(|(centre, keep)| centre.planar(p) >= *keep);
        (kept(jump.takeoff) && kept(jump.toward)).then_some((next, jump))
    }

    /// [`Field::leap`]'s flight, whoever stands where (the walkmesh's
    /// alone), as the assist flies it: up from the edge's middle through the
    /// arc's updates, on along the line out of the floor by the update's
    /// step (in the air the D-pad alone moves the leader) only while the
    /// lowest it is through the update clears what lies under that stretch
    /// (by `CLEAR` over a slide), to a little past where the line reaches a
    /// floor (no farther than halfway across the floor it goes on over),
    /// where it comes down. None where it comes down on a slide or short of
    /// the floor, and where the line meets a triangle the actor may not
    /// cross or the mesh's edge first.
    fn flight(&self, actor: &Actor, arc: &Arc, grids: &[Option<Grid>], layer: usize, t: usize, e: usize) -> Option<(usize, Jump)> {
        let mesh = &self.layers[layer];
        let first = usize::try_from(mesh.triangles[t].n[e]).ok().filter(|n| *n < mesh.triangles.len())?;
        if self.ledge(layer, t, actor) || self.grounded(layer, t, actor) || !self.ledge(layer, first, actor) || self.blocked(layer, first, actor) {
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
        let floor = |u: usize| !self.ledge(layer, u, actor) && self.attribute(layer, u, actor) & ATTR_LAYER0_BLOCK == 0;
        // The triangles the line crosses (each from where it enters to where
        // it leaves), up to the first floor and on over it.
        let mut crossed: Vec<(usize, f32, f32)> = Vec::new();
        let mut reached: Option<usize> = None;
        let (mut current, mut entered) = (first, 0.0f32);
        for _ in 0..LEAP_TRIANGLES {
            let (left, exit) = exit(mesh, current, takeoff, out, entered)?;
            if reached.is_some_and(|k| !floor(current) || entered >= crossed[k].1 + 2.0 * LANDING_IN) {
                break;
            }
            if reached.is_none() && floor(current) {
                reached = Some(crossed.len());
            }
            crossed.push((current, entered, left));
            let Some(next) = usize::try_from(mesh.triangles[current].n[exit]).ok().filter(|n| *n < mesh.triangles.len() && !self.blocked(layer, *n, actor)) else {
                break;
            };
            (current, entered) = (next, left);
        }
        // Nowhere it may not move in the air (over the slides, onto the floor).
        if crossed.iter().any(|(u, ..)| self.grounded(layer, *u, actor)) {
            return None;
        }
        let reached = reached?;
        let onto = crossed[reached].1;
        let over = crossed[reached..].iter().take_while(|(u, ..)| floor(*u)).last().map_or(onto, |(_, _, left)| *left);
        let land = (onto + LANDING_IN).min((onto + over) / 2.0);
        // How high above the takeoff the triangle under `s` lies, and how
        // much clearance it asks of a flight over it.
        let under = |s: f32| crossed.iter().find(|(_, entered, left)| (*entered..=*left).contains(&s));
        let rise = |u: usize, s: f32| {
            let p = at(s);
            takeoff.y - mesh.height(u, p.x, p.z)
        };
        // The most a stretch asks to clear (its ends, in each triangle: the
        // floors are planes).
        let asks = |from: f32, to: f32| {
            crossed.iter()
                .filter(|(_, entered, left)| *left >= from && *entered <= to)
                .map(|(u, entered, left)| {
                    let margin = if floor(*u) { 0.0 } else { CLEAR };
                    rise(*u, from.max(*entered)).max(rise(*u, to.min(*left))) + margin
                })
                .fold(f32::MIN, f32::max)
        };
        // The floors of the other layers over the line, as solid as their
        // triangles are thick (field_collision_find_actor_floor's range):
        // whether the leader's body (its feet `h` above the takeoff, its head
        // `actor.height` higher) meets one at `s`. field_actor_update_position
        // rolls such a move back, and a rise under one stops there.
        let head = f32::from(actor.height.max(0));
        let meets = |s: f32, h: f32| {
            let p = at(s);
            (0..self.actor_layers()).filter(|l| *l != layer && (*l >= 3 || actor.layer_flags & (1 << l) == 0)).any(|l| {
                let mesh = &self.layers[l];
                grids.get(l).and_then(Option::as_ref).and_then(|grid| grid.locate(mesh, p.x, p.z)).is_some_and(|u| {
                    let (bump, top) = (mesh.triangles[u].bump as f32, takeoff.y - mesh.height(u, p.x, p.z));
                    bump > 0.0 && self.attribute(l, u, actor) & ATTR_LAYER0_BLOCK == 0 && h < top && h + head > top - bump
                })
            })
        };
        // Update by update: up by the arc's speed, 2 less each update after
        // (as its heights fall), on along the line by its step while the
        // lowest it is through the update clears what lies under that
        // stretch and its body meets no floor above there; a rise that meets
        // one stops where it is.
        let (mut v, fall) = (arc.0[0].1, arc.0[0].1 * 2.0 - arc.0.get(1).map_or(0.0, |(_, h)| *h));
        let last = arc.0.len().checked_sub(2).map_or(0.0, |i| arc.0[i + 1].0 - arc.0[i].0);
        let (mut s, mut h, mut went) = (0.0f32, 0.0f32, 0.0f32);
        for k in 0..arc.0.len() * 2 {
            let distance = arc.0.get(k).map_or(went + last, |(d, _)| *d);
            let step = (distance - went).max(0.0);
            went = distance;
            let up = h + v;
            let mut to = s;
            if s < land {
                let next = (s + step).min(land);
                if asks(s, next) <= h.min(up) && !meets(next, up) {
                    to = next;
                }
            }
            if v > 0.0 && meets(to, up) {
                (v, to) = (0.0, s);
            } else {
                (h, v) = (up, v - fall);
            }
            s = to;
            let &(u, ..) = under(s)?;
            if h <= rise(u, s) {
                // Down: onto the floor it aimed for, or short of it.
                return (s >= onto && floor(u)).then(|| {
                    let p = at(s);
                    (u, Jump { takeoff, toward: V3::new(p.x, mesh.height(u, p.x, p.z), p.z), from: t, layer })
                });
            }
        }
        None
    }

    /// Where a walk to `jump`'s takeoff comes in from, for an actor farther
    /// from it than `APPROACH`: that far back from the takeoff into the
    /// floor it leaves, square to the edge (along the flight's line), or as
    /// far back as that floor reaches. Walked at along the edge, the leader
    /// would clip the slide beyond it, and the game refuses a step onto a
    /// slide above it.
    pub fn approach(&self, actor: &Actor, jump: &Jump) -> Option<V3> {
        let mesh = self.layers.get(jump.layer).filter(|mesh| jump.from < mesh.triangles.len())?;
        if actor.position.planar(jump.takeoff) <= APPROACH {
            return None;
        }
        let out = jump.toward.sub_planar(jump.takeoff);
        let length = (out.x * out.x + out.z * out.z).sqrt();
        if length < 1e-3 {
            return None;
        }
        let mut back = APPROACH;
        while back >= 2.0 {
            let (x, z) = (jump.takeoff.x - out.x / length * back, jump.takeoff.z - out.z / length * back);
            if mesh.contains(jump.from, x, z) {
                return Some(V3::new(x, mesh.height(jump.from, x, z), z));
            }
            back /= 2.0;
        }
        None
    }

    /// How high above the takeoff the slides that the jump's line crosses
    /// between `from` and `to` along it reach, and the clearance a flight
    /// keeps over them ([`Field::flight`]): what the leader in the air must
    /// be above before going on over that stretch. 0 where it crosses none.
    pub fn rise_ahead(&self, actor: &Actor, jump: &Jump, from: f32, to: f32) -> f32 {
        // (Short of the takeoff lies its floor.)
        let from = from.max(0.0);
        let Some(mesh) = self.layers.get(jump.layer).filter(|_| to > from) else { return 0.0 };
        let toward = jump.toward.sub_planar(jump.takeoff);
        let length = (toward.x * toward.x + toward.z * toward.z).sqrt();
        if length < 1e-3 {
            return 0.0;
        }
        let out = toward.scale(1.0 / length);
        let at = |s: f32| jump.takeoff.add(out.scale(s));
        // The triangle the line leaves the takeoff's floor by.
        let mut current = mesh.locate(at(0.5).x, at(0.5).z);
        let mut entered = 0.0f32;
        let mut most = 0.0f32;
        for _ in 0..LEAP_TRIANGLES {
            let Some(u) = current else { break };
            let Some((left, exit)) = exit(mesh, u, jump.takeoff, out, entered) else { break };
            if left >= from && self.ledge(jump.layer, u, actor) {
                for s in [from.max(entered), to.min(left)] {
                    let p = at(s);
                    most = most.max(jump.takeoff.y - mesh.height(u, p.x, p.z) + CLEAR);
                }
            }
            if left >= to {
                break;
            }
            current = usize::try_from(mesh.triangles[u].n[exit]).ok().filter(|n| *n < mesh.triangles.len());
            entered = left;
        }
        most
    }

    /// The triangles of each layer the actor can reach from where it
    /// stands, walking, jumping over slides, lifted and dropping (no gap's
    /// jumps): what a search run to its end from there reaches (kept, as
    /// [`Field::route`] keeps it).
    pub fn reachable(&self, actor: &Actor) -> Vec<Vec<bool>> {
        let mut seen: Vec<Vec<bool>> = self.layers.iter().map(|mesh| vec![false; mesh.triangles.len()]).collect();
        let Some(start) = self.start(actor) else { return seen };
        let bodies = self.bodies(actor);
        LEAPS.with_borrow_mut(|leaps| {
            leaps.keep_for(self, actor, jump_arc(actor));
            let mut graph = leaps.graph.take().unwrap_or_else(|| Graph::of(self, actor));
            let from = graph.nodes.index(actor.layer, start);
            let search = self.expanded(actor, &mut graph, from, &bodies, leaps);
            for n in (0..graph.nodes.total).filter(|n| search.cost[*n].is_finite()) {
                let (layer, t) = graph.nodes.split(n);
                seen[layer][t] = true;
            }
            leaps.graph = Some(graph);
        });
        seen
    }

    /// The floor a screen point picks to walk to: the one [`Field::pick`]
    /// finds, when the player can reach it; else (a door, a wall, a house,
    /// the sky) the floor it can reach shown nearest the point, within
    /// `reach` screen pixels, and the point the screen point shows at that
    /// floor's height (beyond it: where a door or an exit stands).
    pub fn pick_walkable(&self, screen: (f32, f32), reach: f32) -> Option<(Hit, Option<V3>)> {
        let picked = self.pick(screen);
        if let Some(hit) = picked.filter(|hit| hit.reach != Reach::Invalid) {
            return Some((hit, None));
        }
        let player = self.player()?;
        let reachable = self.reachable(player);
        let (origin, direction) = self.camera.ray(screen)?;
        // The reachable triangle shown nearest the point (along the ray,
        // among those it lies on), and where on it.
        let mut best: Option<(f32, f32, usize, usize, (f32, f32))> = None;
        for (layer, mesh) in self.layers.iter().enumerate().take(self.actor_layers()) {
            for t in (0..mesh.triangles.len()).filter(|t| reachable[layer][*t]) {
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
                    best = Some((distance, along, layer, t, near));
                }
            }
        }
        let (_, _, layer, triangle, near) = best?;
        let mesh = &self.layers[layer];
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
        let hit = Hit { point, triangle, layer, reach: self.reach(player, layer, triangle, point) };
        if hit.reach == Reach::Invalid {
            return None;
        }
        // Past it from the player: the floor the point picked (one no route
        // reaches: pressed on toward, and jumped at where it is higher), else
        // where the point shows at the floor's height (a door, an exit).
        let beyond = picked.map(|hit| hit.point)
            .or_else(|| {
                (direction.y.abs() > 1e-6)
                    .then(|| (point.y - origin.y) / direction.y)
                    .filter(|along| *along > 0.0)
                    .map(|along| origin.add(direction.scale(along)))
            })
            .filter(|b| (BEYOND_MIN..BEYOND_MAX).contains(&b.planar(point)) && b.planar(player.position) > point.planar(player.position));
        Some((hit, beyond))
    }

    /// The goal lies on a part of the actor's layer it cannot walk to (an
    /// island beyond a gap): walk to the edge of where it can walk from
    /// which a jump carries it to the island (its arc passing over the
    /// island's nearest edge at that height), the one nearest the goal, and
    /// jump across.
    fn route_to_gap(&self, actor: &Actor, graph: &mut Graph, start: usize, goal: usize, point: V3, leaps: &mut Leaps) -> Option<Route> {
        let layer = actor.layer;
        let mesh = &self.layers[layer];
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
        // What the actor reaches on its layer.
        let others = self.bodies(actor);
        let base = graph.nodes.index(layer, 0);
        let reachable: Vec<bool> = self.expanded(actor, graph, base + start, &others, leaps).cost[base..base + mesh.triangles.len()].iter().map(|c| c.is_finite()).collect();
        // The island's floor edges (no slide: a jump lands on a floor).
        let mut island = vec![false; mesh.triangles.len()];
        let mut stack = vec![goal];
        island[goal] = true;
        let mut shore = Vec::new();
        while let Some(t) = stack.pop() {
            for e in 0..3 {
                let next = mesh.triangles[t].n[e];
                if next < 0 {
                    if !self.ledge(layer, t, actor) && !self.grounded(layer, t, actor) {
                        shore.push(mesh.edge(t, e));
                    }
                } else if !island[next as usize] && !self.blocked(layer, next as usize, actor) {
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
        // Takeoffs: the walkable floor's open edges (no jump starts on a
        // slide, nor where another actor stands).
        let mut best: Option<(f32, usize, V3, V3)> = None;
        for t in (0..mesh.triangles.len()).filter(|t| reachable[*t] && !self.ledge(layer, *t, actor) && !self.grounded(layer, *t, actor)) {
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
        let mut route = self.route_on(actor, graph, start, layer, takeoff_triangle, takeoff, leaps)?;
        if route.jump.is_none() {
            route.jump = Some(Jump { takeoff, toward: landing, from: takeoff_triangle, layer });
        }
        route.jumps = true;
        route.goal = point;
        route.corners.extend([landing, point]);
        route.corners.dedup_by(|a, b| a.planar(*b) < 0.5);
        route.length = path_length(actor.position, &route.corners);
        Some(route)
    }
}

thread_local! {
    /// The planner's cache ([`Leaps`]) for the walkmesh it last searched.
    static LEAPS: std::cell::RefCell<Leaps> = Default::default();
}

/// What a route search found of one walkmesh for one actor's flags and arc
/// ([`Field::route_with`]): its ways ([`Graph`]), each floor edge's flight
/// over the slide beyond it (whoever stands where), the last routes' paths,
/// kept between searches while that holds.
#[derive(Clone, Debug, Default)]
pub struct Leaps {
    walkmesh: Vec<(u32, usize)>,
    attributes: Vec<u32>,
    flags: (u32, u32, bool, i16),
    arc: &'static [(f32, f32)],
    found: std::collections::HashMap<(usize, usize, usize), Option<(usize, Jump)>>,
    graph: Option<Graph>,
    /// The last routes' goals (layer, triangle, point) and paths: each
    /// node, the way out of it and the cost to it. The rest of a shortest
    /// path from any node on it is the shortest from there, so a route
    /// asked again from a node on it goes on along it.
    paths: std::collections::VecDeque<((usize, usize, [u32; 3]), Vec<(usize, Way, f32)>)>,
    /// The last searches run to their ends from a node (where they reached
    /// no goal): walking, and walking and jumping among other actors'
    /// bodies standing where they stood. Each node's cost from there and
    /// the way into it ([`Field::route_on`]).
    walked: Option<(usize, Search)>,
    expanded: Option<(usize, Vec<(V3, f32)>, Search)>,
    /// A search's buffers, for the next.
    spare: Option<Search>,
}

impl Leaps {
    /// Kept for `field`'s walkmesh (its layers' places and counts) and
    /// collision attributes (which scripts may rewrite:
    /// field_collision_set_attribute_byte), the flags that decide what
    /// `actor` may cross and `arc`; else emptied.
    fn keep_for(&mut self, field: &Field, actor: &Actor, arc: Arc) {
        let flags = ((actor.flags >> 8) & 7, actor.layer_flags & 0x3F, field.party_processing, actor.height);
        // (A walkmesh not read from the game has no place to know it by.)
        let same = self.flags == flags && self.arc.as_ptr() == arc.0.as_ptr() && self.walkmesh.len() == field.layers.len()
            && self.walkmesh.iter().zip(&field.layers).all(|(&(at, count), layer)| at != 0 && at == layer.at && count == layer.triangles.len())
            && self.attributes == field.attributes;
        if !same {
            *self = Leaps {
                walkmesh: field.layers.iter().map(|layer| (layer.at, layer.triangles.len())).collect(),
                attributes: field.attributes.clone(),
                flags,
                arc: arc.0,
                found: Default::default(),
                graph: None,
                paths: Default::default(),
                walked: None,
                expanded: None,
                spare: self.spare.take(),
            };
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

/// Fei's running jump on foot (map 22), as tools/analysis/platforming.py's
/// FOOT measured it, and the party's in its gears: up 24.75 the first
/// update and 2 less each after (map 416's and 415's jumps, traced frame by
/// frame; the tool's GEAR, measured on map 414 under a layer's floor 164
/// above, stops rising at 131.25 where the leader's head meets it).
const FOOT_ARC: [(f32, f32); 22] = [
    (0.0, 20.75), (0.0, 39.5), (12.0, 56.25), (24.0, 71.0), (36.0, 83.75), (48.0, 94.5), (60.0, 103.25),
    (72.0, 110.0), (84.0, 114.75), (96.0, 117.5), (108.0, 118.25), (120.0, 117.0), (132.0, 113.75),
    (144.0, 108.5), (156.0, 101.25), (168.0, 92.0), (180.0, 80.75), (192.0, 67.5), (204.0, 52.25),
    (216.0, 35.0), (228.0, 15.75), (240.0, 0.0),
];
const GEAR_ARC: [(f32, f32); 26] = [
    (0.0, 24.75), (0.0, 47.5), (12.0, 68.25), (24.0, 87.0), (36.0, 103.75), (48.0, 118.5), (60.0, 131.25),
    (72.0, 142.0), (84.0, 150.75), (96.0, 157.5), (108.0, 162.25), (120.0, 165.0), (132.0, 165.75),
    (144.0, 164.5), (156.0, 161.25), (168.0, 156.0), (180.0, 148.75), (192.0, 139.5), (204.0, 128.25),
    (216.0, 115.0), (228.0, 99.75), (240.0, 82.5), (252.0, 63.25), (264.0, 42.0), (276.0, 18.75), (288.0, 0.0),
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
/// How far above a slide a jump's flight must pass (the flight is followed
/// update by update over the slides' planes: a little, for the D-pad's
/// eight directions taking the leader off its line).
const CLEAR: f32 = 1.0;
/// [`Field::reach`]'s kept answers' key: the actor's layer's place and
/// count, its triangle, the goal's layer and triangle, its flags, and a
/// digest of the collision attributes.
type ReachKey = (u32, usize, usize, usize, usize, u32, u32, u64);
/// How far past where its flight reaches a floor a jump lands.
const LANDING_IN: f32 = 24.0;
/// How far back from a jump's takeoff the walk to it comes in square to its
/// edge ([`Field::approach`]).
const APPROACH: f32 = 16.0;
/// The routes' paths [`Leaps`] keeps: the assist's, the walk to its takeoff
/// and a pointer's.
const PATHS: usize = 4;
/// What a jump costs a route beyond its length (a walk round is better).
const JUMP_COST: f32 = 200.0;
/// The most triangles a jump's flight crosses.
const LEAP_TRIANGLES: usize = 32;
/// How far the point beyond a picked door or wall may lie past the floor.
const BEYOND_MIN: f32 = 8.0;
const BEYOND_MAX: f32 = 600.0;

/// The layers' triangles numbered one after another: a route search's
/// nodes.
#[derive(Clone, Debug, Default)]
struct Nodes {
    base: Vec<usize>,
    total: usize,
}

impl Nodes {
    fn of(field: &Field) -> Nodes {
        let mut base = Vec::new();
        let mut total = 0;
        for mesh in field.layers.iter().take(field.actor_layers()) {
            base.push(total);
            total += mesh.triangles.len();
        }
        Nodes { base, total }
    }

    fn index(&self, layer: usize, t: usize) -> usize {
        self.base[layer] + t
    }

    fn split(&self, n: usize) -> (usize, usize) {
        let layer = self.base.partition_point(|b| *b <= n) - 1;
        (layer, n - self.base[layer])
    }
}

/// A walkmesh's ways for one actor's flags, kept for the map ([`Leaps`]):
/// each node's middle and, worked out the first time a search reaches the
/// node, the ways walking and dropping through a pit take across each of
/// its edges, the floor above that a floor of attribute 0x4 lifts it to,
/// and the edges a jump may start over (its flights found as asked).
#[derive(Clone, Debug, Default)]
struct Graph {
    nodes: Nodes,
    centre: Vec<V3>,
    /// Each node's attribute word for the actor ([`Field::attribute`]), and
    /// the actor's flags that meet them: what blocks it walking, what in the
    /// air, the layers whose floors it does not stand on.
    attr: Vec<u32>,
    walk_block: u32,
    air_block: u32,
    masked: u32,
    built: Vec<bool>,
    /// Across each edge: the node walking or dropping leads to, what it
    /// costs, and the pit it drops through (`NONE` for a walk); `NONE` for
    /// no way.
    ways: Vec<[(u32, f32, u32); 3]>,
    /// The floor above it lifts to, and what that costs (`NONE`: none).
    lifts: Vec<(u32, f32)>,
    /// The edges a jump may start over (a floor's onto a slide it may
    /// cross), a bit each.
    leaps: Vec<u8>,
    /// Each layer's triangles by where they lie, made the first time a pit
    /// or a lift asks what lies above or below, or a flight what floors
    /// above it lie in its way; and whether any of the layer's triangles is
    /// solid ([`Triangle::bump`]: a floor in a flight's way).
    grids: Vec<Option<Grid>>,
    solid: Vec<bool>,
    /// The actor's height: its head above its feet.
    head: f32,
}

/// No node.
const NONE: u32 = u32::MAX;

impl Graph {
    fn of(field: &Field, actor: &Actor) -> Graph {
        let nodes = Nodes::of(field);
        let (mut centre, mut attr) = (Vec::with_capacity(nodes.total), Vec::with_capacity(nodes.total));
        for (layer, mesh) in field.layers.iter().enumerate().take(field.actor_layers()) {
            centre.extend((0..mesh.triangles.len()).map(|t| mesh.centroid(t)));
            attr.extend((0..mesh.triangles.len()).map(|t| field.attribute(layer, t, actor)));
        }
        let total = nodes.total;
        let solid = field.layers.iter().take(field.actor_layers()).map(|mesh| mesh.triangles.iter().any(|t| t.bump > 0)).collect();
        Graph {
            nodes, centre, attr, built: vec![false; total], ways: vec![[(NONE, 0.0, NONE); 3]; total], lifts: vec![(NONE, 0.0); total], leaps: vec![0; total],
            walk_block: (actor.flags >> 8) & 7, air_block: (actor.flags >> 9) & 3, masked: actor.layer_flags & 7,
            grids: vec![None; field.actor_layers()], solid, head: f32::from(actor.height.max(0)),
        }
    }

    /// The grids of the layers but `layer` (those with solid triangles
    /// alone, for flights and floors overhead: `solid_only`), each made the
    /// first time asked for.
    fn grids(&mut self, field: &Field, layer: usize, solid_only: bool) {
        for l in (0..self.grids.len()).filter(|l| *l != layer && (!solid_only || self.solid[*l])) {
            if self.grids[l].is_none() {
                self.grids[l] = Some(Grid::of(&field.layers[l]));
            }
        }
    }

    /// Whether the actor may not stand on node `n` ([`Field::blocked`]).
    fn blocked(&self, layer: usize, n: usize) -> bool {
        let word = self.attr[n];
        self.walk_block & ((word >> 5) & 7) != 0 || (layer == 0 && word & ATTR_LAYER0_BLOCK != 0)
    }

    /// Whether a floor of another layer overhead leaves the actor no room
    /// on node `n` (at its middle): one above its floor, as solid as its
    /// triangle is thick ([`Triangle::bump`]), that its body (its feet on
    /// the floor, its head `head` above) meets. field_actor_update_position
    /// refuses a move there. The grids of the other layers with solid
    /// triangles are made first ([`Graph::grids`]).
    fn cramped(&self, field: &Field, n: usize) -> bool {
        let (layer, _) = self.nodes.split(n);
        let c = self.centre[n];
        (0..self.grids.len()).filter(|l| *l != layer && self.solid[*l] && (*l >= 3 || self.masked & (1 << l) == 0)).any(|l| {
            let mesh = &field.layers[l];
            self.grids[l].as_ref().and_then(|grid| grid.locate(mesh, c.x, c.z)).is_some_and(|u| {
                let (bump, top) = (f32::from(mesh.triangles[u].bump), mesh.height(u, c.x, c.z));
                bump > 0.0 && self.attr[self.nodes.index(l, u)] & ATTR_LAYER0_BLOCK == 0 && top < c.y && c.y - self.head < top + bump
            })
        })
    }

    /// The floor layer `l` has for the actor under (x, z): its node and
    /// height (none where the actor does not stand on that layer, or the
    /// triangle there is a pit or blocks it).
    fn floor_at(&self, field: &Field, l: usize, x: f32, z: f32) -> Option<(usize, f32)> {
        if l < 3 && self.masked & (1 << l) != 0 {
            return None;
        }
        let mesh = &field.layers[l];
        let t = self.grids.get(l)?.as_ref()?.locate(mesh, x, z)?;
        let n = self.nodes.index(l, t);
        (!self.blocked(l, n) && self.attr[n] & ATTR_LAYER0_BLOCK == 0).then(|| (n, mesh.height(t, x, z)))
    }

    /// Node `n`'s ways, worked out once from the attributes kept. Walking:
    /// a floor walks onto any floor beside it and onto a slide no more than
    /// `CLIMB` above it; a slide takes it only down or level (heights are the
    /// triangles' middles, as tools/analysis/platforming.py compares them);
    /// triangles that block it and the mesh's edge are no way. Into a pit
    /// (above layer 0 a triangle of bit 23 has no floor) it comes down on the
    /// floor below, the highest the other layers have under its middle below
    /// its highest corner (field_actor_update_position; the platforming
    /// tool's walk into a pit): none there, or one that blocks it, is a wall.
    /// A floor of attribute 0x4 lifts it to the floor above where another
    /// layer has one higher at its middle, the lowest of those (stairs to an
    /// upper layer's floor). A jump may start over an edge from a floor onto
    /// a slide it may cross ([`Field::leap`]).
    fn node(&mut self, field: &Field, n: usize) {
        if std::mem::replace(&mut self.built[n], true) {
            return;
        }
        let (layer, t) = self.nodes.split(n);
        let mesh = &field.layers[layer];
        let (here, word) = (self.centre[n], self.attr[n]);
        let neighbours = mesh.triangles[t].n.map(|u| usize::try_from(u).ok().filter(|u| *u < mesh.triangles.len()).map(|u| self.nodes.index(layer, u)));
        // What lies above or below asks for the other layers' grids, and
        // whether a floor overhead leaves the actor room for those with
        // solid triangles'.
        if (layer > 0 && neighbours.iter().flatten().any(|m| self.attr[*m] & ATTR_LAYER0_BLOCK != 0)) || word & ATTR_LIFT != 0 {
            self.grids(field, layer, false);
        }
        if self.solid.iter().enumerate().any(|(l, solid)| *solid && l != layer) {
            self.grids(field, layer, true);
        }
        let pit = |m: usize| layer > 0 && self.attr[m] & ATTR_LAYER0_BLOCK != 0;
        for (e, next) in neighbours.into_iter().enumerate() {
            let Some(next) = next else { continue };
            let blocked = self.blocked(layer, next);
            let ledge = |m: usize| self.attr[m] & ATTR_LEDGE != 0;
            if !blocked && !pit(next) {
                // A slide only down or level, and onto one no more than
                // CLIMB up; nowhere a floor overhead leaves no room.
                if !((ledge(n) || ledge(next)) && self.centre[next].y < here.y - CLIMB) && !self.cramped(field, next) {
                    let (a, b) = mesh.edge(t, e);
                    let mid = a.add(b).scale(0.5);
                    self.ways[n][e] = (next as u32, here.planar(mid) + mid.planar(self.centre[next]), NONE);
                }
            } else if !blocked && pit(next) {
                // Down through the pit to the highest floor below its
                // highest corner.
                let fall = self.centre[next];
                let (_, u) = self.nodes.split(next);
                let top = mesh.corners(u).iter().map(|c| c.y).fold(f32::INFINITY, f32::min);
                let below = (0..field.actor_layers()).filter(|l| *l != layer)
                    .filter_map(|l| self.floor_at(field, l, fall.x, fall.z))
                    .filter(|(_, y)| *y > top)
                    .min_by(|a, b| a.1.total_cmp(&b.1));
                if let Some((below, _)) = below
                    && !self.cramped(field, below)
                {
                    self.ways[n][e] = (below as u32, here.planar(fall) + fall.planar(self.centre[below]), u as u32);
                }
            }
            // A jump from a floor over a slide it may cross (Field::leap).
            if !ledge(n) && self.air_block & (word >> 3) == 0 && ledge(next) && !blocked {
                self.leaps[n] |= 1 << e;
            }
        }
        // Lifted to the next floor up.
        if word & ATTR_LIFT != 0 {
            let above = (0..field.actor_layers()).filter(|l| *l != layer)
                .filter_map(|l| self.floor_at(field, l, here.x, here.z))
                .filter(|(_, y)| *y < here.y)
                .max_by(|a, b| a.1.total_cmp(&b.1));
            if let Some((above, _)) = above
                && !self.cramped(field, above)
            {
                self.lifts[n] = (above as u32, here.planar(self.centre[above]));
            }
        }
    }
}

/// A layer's triangles by where they lie, in the cells of a grid over its
/// floor: the one under a point found among a cell's few.
#[derive(Clone, Debug, Default)]
struct Grid {
    x0: f32,
    z0: f32,
    cell: f32,
    width: usize,
    depth: usize,
    /// The layer's heights, least and most.
    y0: f32,
    y1: f32,
    /// Each cell's triangles: `listed[starts[c]..starts[c + 1]]`.
    starts: Vec<u32>,
    listed: Vec<u32>,
}

impl Grid {
    fn of(mesh: &Layer) -> Grid {
        let (x0, x1, z0, z1) = mesh.vertices.iter().fold((f32::MAX, f32::MIN, f32::MAX, f32::MIN), |(x0, x1, z0, z1), v| (x0.min(v.x), x1.max(v.x), z0.min(v.z), z1.max(v.z)));
        if mesh.triangles.is_empty() || x0 > x1 {
            return Grid::default();
        }
        // About a triangle a cell.
        let side = (mesh.triangles.len() as f32).sqrt().ceil().max(1.0);
        let cell = ((x1 - x0).max(z1 - z0) / side).max(16.0);
        let (width, depth) = (((x1 - x0) / cell) as usize + 1, ((z1 - z0) / cell) as usize + 1);
        let span = |t: usize| {
            let [a, b, c] = mesh.corners(t);
            let along = |p: f32, o: f32| ((p - o) / cell).max(0.0) as usize;
            (along(a.x.min(b.x).min(c.x), x0), along(a.x.max(b.x).max(c.x), x0).min(width - 1),
             along(a.z.min(b.z).min(c.z), z0), along(a.z.max(b.z).max(c.z), z0).min(depth - 1))
        };
        // Counted, then listed in triangle order (the first under a point is
        // the layer's first).
        let mut starts = vec![0u32; width * depth + 1];
        for t in 0..mesh.triangles.len() {
            let (i0, i1, j0, j1) = span(t);
            for j in j0..=j1 {
                for i in i0..=i1 {
                    starts[j * width + i + 1] += 1;
                }
            }
        }
        for c in 0..width * depth {
            starts[c + 1] += starts[c];
        }
        let mut filled = starts.clone();
        let mut listed = vec![0u32; starts[width * depth] as usize];
        for t in 0..mesh.triangles.len() {
            let (i0, i1, j0, j1) = span(t);
            for j in j0..=j1 {
                for i in i0..=i1 {
                    let c = j * width + i;
                    listed[filled[c] as usize] = t as u32;
                    filled[c] += 1;
                }
            }
        }
        let (y0, y1) = mesh.vertices.iter().fold((f32::MAX, f32::MIN), |(y0, y1), v| (y0.min(v.y), y1.max(v.y)));
        Grid { x0, z0, cell, width, depth, y0, y1, starts, listed }
    }

    /// The cells a ray from `origin` along `direction` passes over between
    /// `from` and `to` lengths along it, in order: `visit` gets each one's
    /// triangles and how far along the ray enters it, until it answers
    /// false.
    fn along(&self, origin: V3, direction: V3, from: f32, to: f32, mut visit: impl FnMut(f32, &[u32]) -> bool) {
        if self.width == 0 {
            return;
        }
        // Clipped to the grid's bounds.
        let (x1, z1) = (self.x0 + self.width as f32 * self.cell, self.z0 + self.depth as f32 * self.cell);
        let (mut from, mut to) = (from, to);
        for (o, d, lo, hi) in [(origin.x, direction.x, self.x0, x1), (origin.z, direction.z, self.z0, z1)] {
            if d.abs() < 1e-9 {
                if o < lo || o > hi {
                    return;
                }
            } else {
                let (a, b) = ((lo - o) / d, (hi - o) / d);
                (from, to) = (from.max(a.min(b)), to.min(a.max(b)));
            }
        }
        if from > to {
            return;
        }
        let start = origin.add(direction.scale(from));
        let cell = |p: f32, o: f32, n: usize| (((p - o) / self.cell).max(0.0) as usize).min(n - 1);
        let (mut i, mut j) = (cell(start.x, self.x0, self.width), cell(start.z, self.z0, self.depth));
        // How far along the ray it next crosses a cell's side, and between
        // sides, on each axis.
        let next = |d: f32, o: f32, at: usize, lo: f32| {
            if d > 0.0 {
                ((lo + (at + 1) as f32 * self.cell - o) / d, self.cell / d)
            } else if d < 0.0 {
                ((lo + at as f32 * self.cell - o) / d, -self.cell / d)
            } else {
                (f32::INFINITY, f32::INFINITY)
            }
        };
        let ((mut tx, dx), (mut tz, dz)) = (next(direction.x, origin.x, i, self.x0), next(direction.z, origin.z, j, self.z0));
        let mut entered = from;
        loop {
            let c = j * self.width + i;
            if !visit(entered, &self.listed[self.starts[c] as usize..self.starts[c + 1] as usize]) {
                return;
            }
            if tx < tz {
                (entered, tx) = (tx, tx + dx);
                if entered > to || (direction.x > 0.0 && i + 1 >= self.width) || (direction.x < 0.0 && i == 0) {
                    return;
                }
                i = if direction.x > 0.0 { i + 1 } else { i - 1 };
            } else {
                if tz.is_infinite() {
                    return;
                }
                (entered, tz) = (tz, tz + dz);
                if entered > to || (direction.z > 0.0 && j + 1 >= self.depth) || (direction.z < 0.0 && j == 0) {
                    return;
                }
                j = if direction.z > 0.0 { j + 1 } else { j - 1 };
            }
        }
    }

    /// [`Layer::locate`]: the first triangle under (x, z).
    fn locate(&self, mesh: &Layer, x: f32, z: f32) -> Option<usize> {
        if self.width == 0 || x < self.x0 || z < self.z0 {
            return None;
        }
        let (i, j) = (((x - self.x0) / self.cell) as usize, ((z - self.z0) / self.cell) as usize);
        if i >= self.width || j >= self.depth {
            return None;
        }
        let c = j * self.width + i;
        self.listed[self.starts[c] as usize..self.starts[c + 1] as usize].iter().map(|t| *t as usize).find(|t| mesh.contains(*t, x, z))
    }
}

/// How a route steps from one node to the next.
#[derive(Clone, Copy, Debug, PartialEq)]
enum Way {
    Walk,
    Jump(Jump),
    /// Lifted from a floor of attribute 0x4 to the floor above.
    Lift,
    /// Into a pit (this triangle of the layer), down to the floor below.
    Drop(usize),
}

/// A route search's state ([`Field::search`]): each node's cost so far,
/// the way into it (from which node, `NONE` for none, and how: [`Search::way`]),
/// the jumps those name, whether it was expanded, and what is open. Its
/// buffers are kept for the next search ([`Leaps`]).
#[derive(Clone, Debug, Default)]
struct Search {
    cost: Vec<f32>,
    from: Vec<(u32, u32)>,
    jumps: Vec<Jump>,
    closed: Vec<bool>,
    open: std::collections::BinaryHeap<Open>,
}

/// [`Search::from`]'s ways: a walk, a lift, a drop through a pit (its
/// triangle below), a jump (its index in [`Search::jumps`] below).
const BY_WALK: u32 = 0;
const BY_LIFT: u32 = 1;
const BY_DROP: u32 = 0x8000_0000;
const BY_JUMP: u32 = 0x4000_0000;

impl Search {
    /// Ready for `count` nodes, nothing reached.
    fn reset(mut self, count: usize) -> Search {
        self.cost.clear();
        self.cost.resize(count, f32::INFINITY);
        self.from.clear();
        self.from.resize(count, (NONE, 0));
        self.closed.clear();
        self.closed.resize(count, false);
        self.jumps.clear();
        self.open.clear();
        self
    }

    /// Node `n` reached at `cost` from node `previous` by `way`, `left` from
    /// the goal by the heuristic: opened if that is less than it had.
    fn seed(&mut self, n: usize, cost: f32, previous: u32, way: Way, left: f32) {
        if cost < self.cost[n] {
            let by = match way {
                Way::Walk => BY_WALK,
                Way::Lift => BY_LIFT,
                Way::Drop(pit) => BY_DROP | pit as u32,
                Way::Jump(jump) => {
                    self.jumps.push(jump);
                    BY_JUMP | (self.jumps.len() - 1) as u32
                }
            };
            self.cost[n] = cost;
            self.from[n] = (previous, by);
            self.open.push(Open((cost + left).max(0.0).to_bits(), n << 2));
        }
    }

    /// The way [`Search::from`] holds as `by`.
    fn way(&self, by: u32) -> Way {
        match by {
            BY_WALK => Way::Walk,
            BY_LIFT => Way::Lift,
            _ if by & BY_DROP != 0 => Way::Drop((by & !BY_DROP) as usize),
            _ => Way::Jump(self.jumps[(by & !BY_JUMP) as usize]),
        }
    }

    /// The path back from node `to`: each node from the search's start, the
    /// way out of it to the next (the goal's none) and the cost to it.
    fn back(&self, to: usize) -> Vec<(usize, Way, f32)> {
        let mut path = vec![(to, Way::Walk, self.cost[to])];
        let mut n = to;
        while let (previous, by) = self.from[n]
            && previous != NONE
        {
            path.push((previous as usize, self.way(by), self.cost[previous as usize]));
            n = previous as usize;
        }
        path.reverse();
        path
    }
}

/// An open node (its index times four) or a jump from one (and its edge
/// and one): its estimate (cost and heuristic, never negative: its bits
/// order as it does), least first.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
struct Open(u32, usize);
impl PartialOrd for Open {
    fn partial_cmp(&self, other: &Self) -> Option<std::cmp::Ordering> { Some(self.cmp(other)) }
}
impl Ord for Open {
    fn cmp(&self, other: &Self) -> std::cmp::Ordering { other.0.cmp(&self.0).then(self.1.cmp(&other.1)) }
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
    /// The triangle the takeoff stands on, and its layer.
    pub from: usize,
    pub layer: usize,
}

#[derive(Clone, Debug, PartialEq, Serialize)]
pub struct Route {
    /// The path's corners after the actor's position, the goal last.
    pub corners: Vec<V3>,
    /// The jump its first stretch ends with (on the actor's layer).
    pub jump: Option<Jump>,
    /// Whether it jumps anywhere on the way (on another layer too).
    pub jumps: bool,
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
            triangles.push(Triangle { v: [a, c, b], n: [-1, lower + 1, if i > 0 { lower - 1 } else { -1 }], attribute: 0, bump: 0 });
            triangles.push(Triangle { v: [c, d, b], n: [if i + 1 < squares { lower + 2 } else { -1 }, -1, lower], attribute: 0, bump: 0 });
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
        let route = f.route(&player, 0, goal, V3::new(450.0, 0.0, 50.0)).unwrap();
        assert_eq!(route.corners, [V3::new(450.0, 0.0, 50.0)]);
        assert!(route.jump.is_none());
        assert!((route.length - 400.0).abs() < 60.0, "{}", route.length);
    }

    #[test]
    fn a_walk_goes_under_a_floor_overhead_only_with_room_for_the_actors_body() {
        // The strip's middle square under another layer's floor, solid
        // (its triangles' thickness) from its top down: 50 above the ground
        // and 100 thick, it reaches below the ground and leaves no room;
        // 200 above, its underside is 100 up and an actor 64 high walks
        // under it.
        let overhead = |top: f32| {
            let mut floor = sheet(1, 1, 100.0, |_, _| -top);
            for v in &mut floor.vertices {
                v.x += 100.0;
            }
            for t in &mut floor.triangles {
                t.bump = 100;
            }
            let mut f = field(strip(3), vec![0]);
            f.layers.extend([floor, Layer::default()]);
            f
        };
        let goal = |f: &Field| f.layers[0].locate(250.0, 50.0).unwrap();
        let point = V3::new(250.0, 0.0, 50.0);
        let f = overhead(50.0);
        let player = *f.player().unwrap();
        assert!(f.route(&player, 0, goal(&f), point).is_none(), "no room under it");
        assert!(!f.reachable(&player)[0][goal(&f)]);
        let f = overhead(200.0);
        let route = f.route(&player, 0, goal(&f), point).expect("room under it");
        assert!(route.jump.is_none());
    }

    #[test]
    fn a_script_rewriting_a_collision_attribute_is_planned_with_at_once() {
        // A walkmesh read from the game (a place to know it by: what the
        // planner learns of it is kept), its middle square's attribute 1
        // open, then rewritten to block flags bit 8 (bit 5), as
        // field_collision_set_attribute_byte rewrites one.
        let mut layer = strip(3);
        layer.at = 0x8010_0000;
        layer.triangles[2].attribute = 1;
        layer.triangles[3].attribute = 1;
        let mut f = field(layer, vec![0, 0]);
        let mut player = *f.player().unwrap();
        player.flags = 0x100;
        f.actors[0] = Some(player);
        let goal = f.layers[0].locate(250.0, 50.0).unwrap();
        let point = V3::new(250.0, 0.0, 50.0);
        assert!(f.route(&player, 0, goal, point).is_some());
        assert_eq!(f.reach(&player, 0, goal, point), Reach::Walk);
        f.attributes[1] = 0x20;
        assert!(f.route(&player, 0, goal, point).is_none(), "the way is blocked now");
        assert_eq!(f.reach(&player, 0, goal, point), Reach::Invalid);
        assert!(!f.reachable(&player)[0][goal]);
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
        assert!(f.route(&player, 0, goal, V3::new(250.0, 0.0, 50.0)).is_none(), "the only way is blocked");
        // The gap is within a jump: the route offers a takeoff... but only
        // where the mesh's own edge faces the goal; here the blocked squares
        // are mesh, not a gap, so nothing is offered.
        assert_eq!(f.reach(&player, 0, goal, V3::new(250.0, 0.0, 50.0)), Reach::Invalid);
        // Without the flag, the same squares are open.
        player.flags = 0;
        assert!(f.route(&player, 0, goal, V3::new(250.0, 0.0, 50.0)).is_some());
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
        let route = f.route(&player, 0, goal, point).unwrap();
        let jump = route.jump.expect("a jump up over the slide");
        assert!((jump.takeoff.x - 200.0).abs() < 1.0 && jump.takeoff.z == 50.0, "from the slide's foot: {jump:?}");
        assert!(jump.toward.x > 300.0 && (jump.toward.y + 60.0).abs() < 1.0, "onto the raised floor: {jump:?}");
        assert_eq!(route.corners.last(), Some(&point));
        assert!(route.corners.iter().any(|c| c.planar(jump.takeoff) < 0.5), "the walk goes to the takeoff: {route:?}");
        assert_eq!(f.reach(&player, 0, goal, point), Reach::Jump);
        // The slide is no floor to stand on: walked onto from its foot only.
        let on_slide = f.layers[0].locate(250.0, 50.0).unwrap();
        assert!(f.reachable(&player)[0][on_slide] && f.reachable(&player)[0][goal]);
        // Higher than a jump rises (165.75 in gears): no way up.
        let f = raised(180.0);
        assert!(f.route(&player, 0, goal, V3::new(350.0, -180.0, 50.0)).is_none());
        assert!(!f.reachable(&player)[0][goal]);
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
        let route = f.route(&player, 0, goal, V3::new(400.0, 0.0, 50.0)).unwrap();
        let jump = route.jump.expect("a takeoff at the gap");
        assert!((jump.takeoff.x - 200.0).abs() < 1.0, "{jump:?}");
        assert_eq!(f.reach(&player, 0, goal, V3::new(400.0, 0.0, 50.0)), Reach::Jump);
    }

    #[test]
    fn stairs_lift_a_route_to_the_layer_above_and_a_pit_drops_it_back() {
        // Layer 0: a strip at the ground, its fourth square's triangles
        // stairs (attribute 0x4). Layer 1: a deck 80 up over squares 3 to 5,
        // its last square's triangles a pit (bit 23). Layer 2: the camera's.
        let mut ground = strip(6);
        ground.triangles[6].attribute = 1;
        ground.triangles[7].attribute = 1;
        let mut deck = strip(3);
        for v in &mut deck.vertices {
            v.x += 300.0;
            v.y = -80.0;
        }
        deck.triangles[4].attribute = 2;
        deck.triangles[5].attribute = 2;
        let mut f = field(ground, vec![0, ATTR_LIFT, ATTR_LAYER0_BLOCK]);
        f.layers.extend([deck, Layer::default()]);
        let player = *f.player().unwrap();
        // Up: to the deck's second square, by the stairs.
        let goal = f.layers[1].locate(450.0, 50.0).unwrap();
        let point = V3::new(450.0, -80.0, 50.0);
        let route = f.route(&player, 1, goal, point).expect("up the stairs");
        assert!(route.jump.is_none() && !route.jumps, "{route:?}");
        let lift = f.layers[0].centroid(6);
        assert!(route.corners.iter().any(|c| c.planar(lift) < 40.0), "onto the stairs first: {route:?}");
        assert_eq!(route.corners.last(), Some(&point));
        assert_eq!(f.reach(&player, 1, goal, point), Reach::Walk);
        assert!(f.reachable(&player)[1][goal]);
        // Down: from the deck through its pit to the ground beyond.
        let mut up = player;
        (up.layer, up.position) = (1, V3::new(350.0, -80.0, 50.0));
        up.triangle = [-1; 4];
        let below = f.layers[0].locate(550.0, 50.0).unwrap();
        let route = f.route(&up, 0, below, V3::new(550.0, 0.0, 50.0)).expect("down through the pit");
        assert!(route.corners.iter().any(|c| c.planar(f.layers[1].centroid(4)) < 40.0), "into the pit: {route:?}");
        // The ground's stairs masked for the actor (layer flags bit 3:
        // no attributes on layer 0): no way up.
        let mut masked = player;
        masked.layer_flags = 1 << 3;
        assert!(f.route(&masked, 1, goal, point).is_none());
    }

    /// A sheet of `columns` by `rows` squares `size` wide, each two
    /// triangles, its heights by `height` (neighbours unjoined).
    fn sheet(columns: usize, rows: usize, size: f32, height: impl Fn(f32, f32) -> f32) -> Layer {
        let mut vertices = Vec::new();
        for j in 0..=rows {
            for i in 0..=columns {
                let (x, z) = (i as f32 * size, j as f32 * size);
                vertices.push(V3::new(x, height(x, z), z));
            }
        }
        let at = |i: usize, j: usize| j * (columns + 1) + i;
        let mut triangles = Vec::new();
        for j in 0..rows {
            for i in 0..columns {
                triangles.push(Triangle { v: [at(i, j), at(i + 1, j), at(i, j + 1)], n: [-1; 3], attribute: 0, bump: 0 });
                triangles.push(Triangle { v: [at(i + 1, j), at(i + 1, j + 1), at(i, j + 1)], n: [-1; 3], attribute: 0, bump: 0 });
            }
        }
        Layer { vertices, triangles, at: 0 }
    }

    #[test]
    fn picking_along_the_grid_meets_what_testing_every_triangle_meets() {
        // Uneven ground; a deck over part of it, one of its triangles a pit
        // (attribute 1: bit 23); rays from above at every angle, straight
        // down and along the axes too, some cut short.
        let ground = sheet(16, 12, 50.0, |x, z| (x * 0.05).sin() * 20.0 + (z * 0.03).cos() * 15.0);
        let mut deck = sheet(4, 3, 50.0, |_, _| -80.0);
        for v in &mut deck.vertices {
            (v.x, v.z) = (v.x + 200.0, v.z + 150.0);
        }
        deck.triangles[5].attribute = 1;
        let mut f = field(ground, vec![0, ATTR_LAYER0_BLOCK]);
        f.layers.extend([deck, Layer::default()]);
        let player = *f.player().unwrap();
        let every = |origin: V3, direction: V3, within: f32| {
            let mut best: Option<(f32, usize, usize)> = None;
            for layer in 0..f.actor_layers() {
                let mesh = &f.layers[layer];
                let mine = if layer == player.layer { 1.0 } else { 0.0 };
                for t in 0..mesh.triangles.len() {
                    if layer > 0 && f.attribute(layer, t, &player) & ATTR_LAYER0_BLOCK != 0 {
                        continue;
                    }
                    let [a, b, c] = mesh.corners(t);
                    if let Some(distance) = intersect(origin, direction, a, b, c).filter(|d| *d <= within)
                        && best.is_none_or(|(d, ..)| distance - mine < d)
                    {
                        best = Some((distance - mine, layer, t));
                    }
                }
            }
            best.map(|(_, layer, t)| (layer, t))
        };
        let mut seed = 12345u32;
        let mut random = || {
            seed = seed.wrapping_mul(1_664_525).wrapping_add(1_013_904_223);
            (seed >> 8) as f32 / (1u32 << 24) as f32
        };
        let mut met = 0;
        for k in 0..2000 {
            let origin = V3::new(random() * 1000.0 - 100.0, -300.0 - random() * 400.0, random() * 800.0 - 100.0);
            let target = V3::new(random() * 900.0 - 50.0, random() * 100.0 - 60.0, random() * 700.0 - 50.0);
            let mut direction = target.sub(origin).scale(0.01);
            match k % 7 {
                0 => direction.x = 0.0,
                1 => direction.z = 0.0,
                2 => (direction.x, direction.z) = (0.0, 0.0),
                _ => {}
            }
            let within = if k % 5 == 0 { 80.0 + random() * 40.0 } else { f32::INFINITY };
            let picked = f.pick_ray(origin, direction, within).map(|hit| (hit.layer, hit.triangle));
            assert_eq!(picked, every(origin, direction, within), "ray {k}: from {origin:?} along {direction:?} within {within}");
            met += usize::from(picked.is_some());
        }
        assert!(met > 1000, "most rays meet a floor: {met}");
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
        l.triangles.push(Triangle { v: [5, 3, base], n: [3, -1, 5], attribute: 0, bump: 0 });
        l.triangles.push(Triangle { v: [5, base, base + 1], n: [4, -1, -1], attribute: 0, bump: 0 });
        let f = field(l, vec![0]);
        let player = *f.player().unwrap();
        let goal = f.layers[0].locate(150.0, 180.0).unwrap();
        let route = f.route(&player, 0, goal, V3::new(150.0, 0.0, 180.0)).unwrap();
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
