//! Seraph Glass (plan.md): the look of the bespoke widgets added for
//! headset and touch play: the gaze and pointer cursors, committed
//! destinations, the battle ring's page marks, quantity and control decks.
//! Not the settings panel or any ordinary application interface, and never
//! a coating over the game.
//!
//! Each widget is a faceted silver-white shell around a translucent inner
//! surface, with hollow halos, tapered fins, a dark recess and sparse
//! gold/copper accents, lit from above (iOS Liquid Glass-like: a bright
//! rim, a soft specular arc, a translucent body). State reads by shape and
//! contrast as well as colour: hover brightens the rim and lights the gold
//! accents, pressed sinks the body, disabled dims it and strikes it
//! through, committed gilds the halo. Symbols and labels are solid, with a
//! dark outline, at any transparency; the reduced-transparency option
//! makes bodies opaque.
//!
//! Widgets are drawn on the processor into small RGBA images (straight
//! alpha, sRGB-encoded), four samples a pixel, and kept as textures: the
//! hosts place them as screen quads (touch) or room quads (a headset).
use std::collections::HashMap;

#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub enum State {
    Normal,
    Hover,
    Pressed,
    Disabled,
    /// Committed or active (a destination, a held control, the current page).
    Active,
}

#[derive(Clone, Debug, PartialEq, Eq, Hash)]
pub enum Icon {
    None,
    Arrow(Dir),
    Cross,
    Circle,
    Triangle,
    Square,
    Plus,
    Minus,
    /// A short uppercase label (A-Z, 0-9, space).
    Text(String),
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub enum Dir {
    Up,
    Down,
    Left,
    Right,
}

/// Where a floor point is reachable from (the field cursor's shapes).
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub enum Reach {
    Walk,
    Jump,
    Invalid,
}

#[derive(Clone, Debug, PartialEq, Eq, Hash)]
pub enum Widget {
    /// The 2D gaze or pointer cursor (menus, dialogue, battle).
    Cursor,
    /// The field's surface cursor (drawn flat on the floor by the host).
    Floor(Reach),
    /// A committed destination on the floor.
    Destination,
    /// A round control with a symbol.
    Button(Icon, State),
    /// A wide control with a label (quantity buttons, deck keys).
    Key(Icon, State),
    /// The battle ring's page mark: `count` pages, `current` lit.
    Pages { count: u8, current: u8 },
}

/// Drawing options (the settings' reduced transparency and motion).
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Hash)]
pub struct Style {
    pub reduced_transparency: bool,
}

#[derive(Clone, Copy)]
struct Rgba([f32; 4]);

const SILVER: [f32; 3] = [0.93, 0.94, 0.96];
const SILVER_DARK: [f32; 3] = [0.55, 0.58, 0.63];
const RECESS: [f32; 3] = [0.05, 0.06, 0.08];
const GOLD: [f32; 3] = [0.86, 0.68, 0.33];
const COPPER: [f32; 3] = [0.72, 0.42, 0.26];
const INNER: [f32; 3] = [0.82, 0.84, 0.86];
const INVALID: [f32; 3] = [0.62, 0.18, 0.16];

/// Draws over `dst` with straight-alpha `src`.
fn over(dst: &mut Rgba, src: [f32; 3], alpha: f32) {
    let a = alpha.clamp(0.0, 1.0);
    let out_a = a + dst.0[3] * (1.0 - a);
    if out_a <= 1e-6 {
        return;
    }
    for i in 0..3 {
        dst.0[i] = (src[i] * a + dst.0[i] * dst.0[3] * (1.0 - a)) / out_a;
    }
    dst.0[3] = out_a;
}

/// Coverage of a signed distance (negative inside) at `aa` pixels softness.
fn cover(d: f32, aa: f32) -> f32 {
    (0.5 - d / aa).clamp(0.0, 1.0)
}

fn lerp3(a: [f32; 3], b: [f32; 3], t: f32) -> [f32; 3] {
    [0, 1, 2].map(|i| a[i] + (b[i] - a[i]) * t)
}

/// A regular octagon's distance (the faceted shell), radius `r`.
fn octagon(x: f32, y: f32, r: f32) -> f32 {
    let (mut x, mut y) = (x.abs(), y.abs());
    let k = [-0.923_879_5f32, 0.382_683_43, 0.414_213_57];
    let d = 2.0 * (k[0] * x + k[1] * y).min(0.0);
    x -= d * k[0];
    y -= d * k[1];
    let d = 2.0 * (-k[0] * x + k[1] * y).min(0.0);
    x -= d * -k[0];
    y -= d * k[1];
    x -= x.clamp(-k[2] * r, k[2] * r);
    y -= r;
    (x * x + y * y).sqrt() * y.signum()
}

/// A rounded rectangle's distance, half size `hw`, `hh`, corner `c`.
fn rounded(x: f32, y: f32, hw: f32, hh: f32, c: f32) -> f32 {
    let qx = x.abs() - hw + c;
    let qy = y.abs() - hh + c;
    (qx.max(0.0).powi(2) + qy.max(0.0).powi(2)).sqrt() + qx.max(qy).min(0.0) - c
}

fn segment(px: f32, py: f32, ax: f32, ay: f32, bx: f32, by: f32) -> f32 {
    let (abx, aby) = (bx - ax, by - ay);
    let t = (((px - ax) * abx + (py - ay) * aby) / (abx * abx + aby * aby).max(1e-9)).clamp(0.0, 1.0);
    ((px - ax - abx * t).powi(2) + (py - ay - aby * t).powi(2)).sqrt()
}

/// A triangle's distance (vertices in order).
fn triangle(p: (f32, f32), a: (f32, f32), b: (f32, f32), c: (f32, f32)) -> f32 {
    let edges = [(a, b), (b, c), (c, a)];
    let d = edges.iter().map(|(u, v)| segment(p.0, p.1, u.0, u.1, v.0, v.1)).fold(f32::MAX, f32::min);
    let side = |u: (f32, f32), v: (f32, f32)| (v.0 - u.0) * (p.1 - u.1) - (v.1 - u.1) * (p.0 - u.0);
    let s = [side(a, b), side(b, c), side(c, a)];
    let inside = s.iter().all(|v| *v >= 0.0) || s.iter().all(|v| *v <= 0.0);
    if inside { -d } else { d }
}

/// Stroke segments of the label font on a 4x6 grid (y down).
fn glyph(c: char) -> &'static [(f32, f32, f32, f32)] {
    match c {
        'A' => &[(0.0, 6.0, 2.0, 0.0), (2.0, 0.0, 4.0, 6.0), (1.0, 3.5, 3.0, 3.5)],
        'B' => &[(0.0, 0.0, 0.0, 6.0), (0.0, 0.0, 3.0, 0.0), (3.0, 0.0, 3.5, 1.5), (3.5, 1.5, 3.0, 3.0), (0.0, 3.0, 3.0, 3.0), (3.0, 3.0, 4.0, 4.5), (4.0, 4.5, 3.0, 6.0), (3.0, 6.0, 0.0, 6.0)],
        'C' => &[(4.0, 0.5, 3.0, 0.0), (3.0, 0.0, 1.0, 0.0), (1.0, 0.0, 0.0, 1.5), (0.0, 1.5, 0.0, 4.5), (0.0, 4.5, 1.0, 6.0), (1.0, 6.0, 3.0, 6.0), (3.0, 6.0, 4.0, 5.5)],
        'D' => &[(0.0, 0.0, 0.0, 6.0), (0.0, 0.0, 2.5, 0.0), (2.5, 0.0, 4.0, 1.5), (4.0, 1.5, 4.0, 4.5), (4.0, 4.5, 2.5, 6.0), (2.5, 6.0, 0.0, 6.0)],
        'E' => &[(4.0, 0.0, 0.0, 0.0), (0.0, 0.0, 0.0, 6.0), (0.0, 6.0, 4.0, 6.0), (0.0, 3.0, 3.0, 3.0)],
        'F' => &[(4.0, 0.0, 0.0, 0.0), (0.0, 0.0, 0.0, 6.0), (0.0, 3.0, 3.0, 3.0)],
        'G' => &[(4.0, 0.5, 3.0, 0.0), (3.0, 0.0, 1.0, 0.0), (1.0, 0.0, 0.0, 1.5), (0.0, 1.5, 0.0, 4.5), (0.0, 4.5, 1.0, 6.0), (1.0, 6.0, 3.0, 6.0), (3.0, 6.0, 4.0, 5.0), (4.0, 5.0, 4.0, 3.5), (4.0, 3.5, 2.5, 3.5)],
        'H' => &[(0.0, 0.0, 0.0, 6.0), (4.0, 0.0, 4.0, 6.0), (0.0, 3.0, 4.0, 3.0)],
        'I' => &[(1.0, 0.0, 3.0, 0.0), (2.0, 0.0, 2.0, 6.0), (1.0, 6.0, 3.0, 6.0)],
        'J' => &[(4.0, 0.0, 4.0, 4.5), (4.0, 4.5, 3.0, 6.0), (3.0, 6.0, 1.0, 6.0), (1.0, 6.0, 0.0, 4.5)],
        'K' => &[(0.0, 0.0, 0.0, 6.0), (4.0, 0.0, 0.0, 3.5), (1.5, 2.5, 4.0, 6.0)],
        'L' => &[(0.0, 0.0, 0.0, 6.0), (0.0, 6.0, 4.0, 6.0)],
        'M' => &[(0.0, 6.0, 0.0, 0.0), (0.0, 0.0, 2.0, 3.5), (2.0, 3.5, 4.0, 0.0), (4.0, 0.0, 4.0, 6.0)],
        'N' => &[(0.0, 6.0, 0.0, 0.0), (0.0, 0.0, 4.0, 6.0), (4.0, 6.0, 4.0, 0.0)],
        'O' => &[(1.0, 0.0, 3.0, 0.0), (3.0, 0.0, 4.0, 1.5), (4.0, 1.5, 4.0, 4.5), (4.0, 4.5, 3.0, 6.0), (3.0, 6.0, 1.0, 6.0), (1.0, 6.0, 0.0, 4.5), (0.0, 4.5, 0.0, 1.5), (0.0, 1.5, 1.0, 0.0)],
        'P' => &[(0.0, 6.0, 0.0, 0.0), (0.0, 0.0, 3.0, 0.0), (3.0, 0.0, 4.0, 1.0), (4.0, 1.0, 4.0, 2.5), (4.0, 2.5, 3.0, 3.5), (3.0, 3.5, 0.0, 3.5)],
        'Q' => &[(1.0, 0.0, 3.0, 0.0), (3.0, 0.0, 4.0, 1.5), (4.0, 1.5, 4.0, 4.5), (4.0, 4.5, 3.0, 6.0), (3.0, 6.0, 1.0, 6.0), (1.0, 6.0, 0.0, 4.5), (0.0, 4.5, 0.0, 1.5), (0.0, 1.5, 1.0, 0.0), (2.5, 4.5, 4.0, 6.0)],
        'R' => &[(0.0, 6.0, 0.0, 0.0), (0.0, 0.0, 3.0, 0.0), (3.0, 0.0, 4.0, 1.0), (4.0, 1.0, 4.0, 2.5), (4.0, 2.5, 3.0, 3.5), (3.0, 3.5, 0.0, 3.5), (2.0, 3.5, 4.0, 6.0)],
        'S' => &[(4.0, 0.5, 3.0, 0.0), (3.0, 0.0, 1.0, 0.0), (1.0, 0.0, 0.0, 1.0), (0.0, 1.0, 0.0, 2.0), (0.0, 2.0, 1.0, 3.0), (1.0, 3.0, 3.0, 3.0), (3.0, 3.0, 4.0, 4.0), (4.0, 4.0, 4.0, 5.0), (4.0, 5.0, 3.0, 6.0), (3.0, 6.0, 1.0, 6.0), (1.0, 6.0, 0.0, 5.5)],
        'T' => &[(0.0, 0.0, 4.0, 0.0), (2.0, 0.0, 2.0, 6.0)],
        'U' => &[(0.0, 0.0, 0.0, 4.5), (0.0, 4.5, 1.0, 6.0), (1.0, 6.0, 3.0, 6.0), (3.0, 6.0, 4.0, 4.5), (4.0, 4.5, 4.0, 0.0)],
        'V' => &[(0.0, 0.0, 2.0, 6.0), (2.0, 6.0, 4.0, 0.0)],
        'W' => &[(0.0, 0.0, 1.0, 6.0), (1.0, 6.0, 2.0, 2.5), (2.0, 2.5, 3.0, 6.0), (3.0, 6.0, 4.0, 0.0)],
        'X' => &[(0.0, 0.0, 4.0, 6.0), (4.0, 0.0, 0.0, 6.0)],
        'Y' => &[(0.0, 0.0, 2.0, 3.0), (4.0, 0.0, 2.0, 3.0), (2.0, 3.0, 2.0, 6.0)],
        'Z' => &[(0.0, 0.0, 4.0, 0.0), (4.0, 0.0, 0.0, 6.0), (0.0, 6.0, 4.0, 6.0)],
        '0' => &[(1.0, 0.0, 3.0, 0.0), (3.0, 0.0, 4.0, 1.5), (4.0, 1.5, 4.0, 4.5), (4.0, 4.5, 3.0, 6.0), (3.0, 6.0, 1.0, 6.0), (1.0, 6.0, 0.0, 4.5), (0.0, 4.5, 0.0, 1.5), (0.0, 1.5, 1.0, 0.0), (3.5, 0.5, 0.5, 5.5)],
        '1' => &[(1.0, 1.0, 2.0, 0.0), (2.0, 0.0, 2.0, 6.0), (1.0, 6.0, 3.0, 6.0)],
        '2' => &[(0.0, 1.0, 1.0, 0.0), (1.0, 0.0, 3.0, 0.0), (3.0, 0.0, 4.0, 1.0), (4.0, 1.0, 4.0, 2.0), (4.0, 2.0, 0.0, 6.0), (0.0, 6.0, 4.0, 6.0)],
        '+' => &[(0.0, 3.0, 4.0, 3.0), (2.0, 1.0, 2.0, 5.0)],
        '-' => &[(0.5, 3.0, 3.5, 3.0)],
        _ => &[],
    }
}

/// A label's stroke distance at (x, y), the text centred at (cx, cy) with
/// letters `h` pixels tall.
fn text_distance(text: &str, x: f32, y: f32, cx: f32, cy: f32, h: f32) -> f32 {
    let unit = h / 6.0;
    let advance = 5.5 * unit;
    let count = text.chars().count() as f32;
    let left = cx - (advance * count - 1.5 * unit) / 2.0;
    let top = cy - h / 2.0;
    let mut best = f32::MAX;
    for (i, c) in text.chars().enumerate() {
        let ox = left + i as f32 * advance;
        for &(ax, ay, bx, by) in glyph(c.to_ascii_uppercase()) {
            best = best.min(segment(x, y, ox + ax * unit, top + ay * unit, ox + bx * unit, top + by * unit));
        }
    }
    best
}

/// A symbol's distance (negative inside), centred, `r` its half size.
fn icon_distance(icon: &Icon, x: f32, y: f32, r: f32, stroke: f32) -> f32 {
    match icon {
        Icon::None => f32::MAX,
        Icon::Arrow(dir) => {
            let (fx, fy) = match dir {
                Dir::Up => (x, y),
                Dir::Down => (x, -y),
                Dir::Left => (y, x),
                Dir::Right => (y, -x),
            };
            triangle((fx, fy), (0.0, -r * 0.85), (r * 0.8, r * 0.55), (-r * 0.8, r * 0.55))
        }
        Icon::Cross => segment(x, y, -r * 0.7, -r * 0.7, r * 0.7, r * 0.7).min(segment(x, y, -r * 0.7, r * 0.7, r * 0.7, -r * 0.7)) - stroke,
        Icon::Circle => ((x * x + y * y).sqrt() - r * 0.72).abs() - stroke,
        Icon::Triangle => {
            let t = triangle((x, y), (0.0, -r * 0.8), (r * 0.8, r * 0.6), (-r * 0.8, r * 0.6));
            t.abs() - stroke
        }
        Icon::Square => rounded(x, y, r * 0.66, r * 0.66, 1.0).abs() - stroke,
        Icon::Plus => segment(x, y, -r * 0.75, 0.0, r * 0.75, 0.0).min(segment(x, y, 0.0, -r * 0.75, 0.0, r * 0.75)) - stroke,
        Icon::Minus => segment(x, y, -r * 0.75, 0.0, r * 0.75, 0.0) - stroke,
        Icon::Text(text) => text_distance(text, x, y, 0.0, 0.0, r * 1.1) - stroke * 0.8,
    }
}

/// Paints a solid symbol with a dark outline (legible at any transparency).
fn paint_icon(pixel: &mut Rgba, icon: &Icon, x: f32, y: f32, r: f32, stroke: f32, aa: f32, colour: [f32; 3]) {
    let d = icon_distance(icon, x, y, r, stroke);
    over(pixel, RECESS, cover(d - stroke * 0.9, aa) * 0.85);
    over(pixel, colour, cover(d, aa));
}

/// The glass body's colour at height `t` (0 top .. 1 bottom), lit from above.
fn body(t: f32) -> [f32; 3] {
    lerp3([0.97, 0.98, 0.99], INNER, t)
}

fn shade(widget: &Widget, style: Style, x: f32, y: f32, size: f32) -> Rgba {
    let mut p = Rgba([0.0; 4]);
    let aa = 1.2;
    let c = size / 2.0;
    let (x, y) = (x - c, y - c);
    let glass_alpha = if style.reduced_transparency { 0.92 } else { 0.38 };
    match widget {
        Widget::Cursor => {
            // A hollow halo with four short fins; the centre stays open.
            let r = c * 0.5;
            let ring = ((x * x + y * y).sqrt() - r).abs() - c * 0.07;
            over(&mut p, RECESS, cover(ring - 2.0, aa * 2.0) * 0.5);
            over(&mut p, lerp3(SILVER, SILVER_DARK, (y / c + 1.0) / 2.0), cover(ring, aa));
            for (fx, fy) in [(0.0, -1.0), (1.0, 0.0), (0.0, 1.0), (-1.0, 0.0)] {
                let tip = (fx * c * 0.92, fy * c * 0.92);
                let base = (fx * c * 0.66, fy * c * 0.66);
                let side = (-fy * c * 0.08, fx * c * 0.08);
                let fin = triangle((x, y), tip, (base.0 + side.0, base.1 + side.1), (base.0 - side.0, base.1 - side.1));
                over(&mut p, RECESS, cover(fin - 1.5, aa) * 0.6);
                over(&mut p, SILVER, cover(fin, aa));
            }
            // A gold point at the stable centre.
            over(&mut p, GOLD, cover((x * x + y * y).sqrt() - c * 0.06, aa));
        }
        Widget::Floor(reach) => {
            let r = c * 0.62;
            let d = (x * x + y * y).sqrt();
            let ring = (d - r).abs() - c * 0.06;
            let (rim, accent) = match reach {
                Reach::Walk => (SILVER, SILVER),
                Reach::Jump => (SILVER, GOLD),
                Reach::Invalid => (SILVER_DARK, INVALID),
            };
            over(&mut p, RECESS, cover(d - r * 0.95, aa) * 0.22);
            over(&mut p, RECESS, cover(ring - 2.0, aa * 2.0) * 0.5);
            let broken = *reach == Reach::Invalid && (x + y).abs() < c * 0.12;
            if !broken {
                over(&mut p, rim, cover(ring, aa));
            }
            match reach {
                Reach::Walk => {
                    for (fx, fy) in [(0.0, -1.0), (1.0, 0.0), (0.0, 1.0), (-1.0, 0.0)] {
                        let tip = (fx * c * 0.98, fy * c * 0.98);
                        let base = (fx * c * 0.72, fy * c * 0.72);
                        let side = (-fy * c * 0.07, fx * c * 0.07);
                        over(&mut p, rim, cover(triangle((x, y), tip, (base.0 + side.0, base.1 + side.1), (base.0 - side.0, base.1 - side.1)), aa));
                    }
                }
                Reach::Jump => {
                    // An upward chevron in gold: a jump on the way.
                    let chevron = segment(x, y, -c * 0.28, c * 0.12, 0.0, -c * 0.18).min(segment(x, y, 0.0, -c * 0.18, c * 0.28, c * 0.12)) - c * 0.06;
                    over(&mut p, RECESS, cover(chevron - 2.0, aa) * 0.7);
                    over(&mut p, accent, cover(chevron, aa));
                }
                Reach::Invalid => {
                    let slash = segment(x, y, -r * 0.8, r * 0.8, r * 0.8, -r * 0.8) - c * 0.05;
                    over(&mut p, RECESS, cover(slash - 2.0, aa) * 0.7);
                    over(&mut p, accent, cover(slash, aa));
                }
            }
        }
        Widget::Destination => {
            // The cursor's ring gilded, with long fins and a copper centre.
            let d = (x * x + y * y).sqrt();
            let r = c * 0.58;
            let ring = (d - r).abs() - c * 0.07;
            over(&mut p, RECESS, cover(ring - 2.5, aa * 2.0) * 0.55);
            over(&mut p, GOLD, cover(ring, aa));
            over(&mut p, SILVER, cover((d - r * 0.72).abs() - c * 0.025, aa));
            for (fx, fy) in [(0.0, -1.0), (1.0, 0.0), (0.0, 1.0), (-1.0, 0.0)] {
                let tip = (fx * c, fy * c);
                let base = (fx * c * 0.62, fy * c * 0.62);
                let side = (-fy * c * 0.09, fx * c * 0.09);
                over(&mut p, SILVER, cover(triangle((x, y), tip, (base.0 + side.0, base.1 + side.1), (base.0 - side.0, base.1 - side.1)), aa));
            }
            over(&mut p, COPPER, cover(d - c * 0.12, aa));
        }
        Widget::Button(icon, state) | Widget::Key(icon, state) => {
            let wide = matches!(widget, Widget::Key(..));
            let (hw, hh) = if wide { (c * 0.96, c * 0.46) } else { (c * 0.86, c * 0.86) };
            let shell = if wide { rounded(x, y, hw, hh, hh * 0.45) } else { octagon(x, y, hw) };
            let inner = shell + c * 0.1;
            let t = ((y + hh) / (2.0 * hh)).clamp(0.0, 1.0);
            let pressed = *state == State::Pressed;
            let disabled = *state == State::Disabled;
            // A soft shadow and the dark recess behind the glass.
            over(&mut p, RECESS, cover(shell - 3.0, 6.0) * 0.45);
            over(&mut p, RECESS, cover(shell, aa) * if pressed { 0.75 } else { 0.55 });
            // The translucent body, lit from above (sunk when pressed).
            let lit = if pressed { body(0.6 + t * 0.4) } else { body(t) };
            // Opaque bodies (reduced transparency) are dark glass, so the
            // white symbols keep their contrast.
            let lit = if style.reduced_transparency { lerp3([0.24, 0.26, 0.3], [0.13, 0.14, 0.17], if pressed { 1.0 } else { t }) } else { lit };
            over(&mut p, lit, cover(inner, aa) * glass_alpha * if disabled { 0.5 } else { 1.0 });
            // The specular arc near the top.
            let arc = ((y + hh * 0.55) / (hh * 0.35)).powi(2) + (x / (hw * 0.75)).powi(2) - 1.0;
            if !pressed && !disabled {
                over(&mut p, [1.0; 3], cover(arc * c * 0.2, aa * 3.0) * cover(inner, aa) * 0.35);
            }
            // The faceted silver shell; brighter on hover, gold when active.
            let rim = (shell + c * 0.05).abs() - c * 0.05;
            let rim_colour = match state {
                State::Hover => [1.0, 1.0, 1.0],
                State::Active => GOLD,
                State::Disabled => SILVER_DARK,
                _ => lerp3(SILVER, SILVER_DARK, t * 0.6),
            };
            over(&mut p, rim_colour, cover(rim, aa));
            // Sparse gold accents: two short facets, lit on hover and when active.
            if matches!(state, State::Hover | State::Active) || !wide {
                let accent = segment(x, y, -hw * 0.35, -hh * 0.97, hw * 0.35, -hh * 0.97) - c * 0.025;
                over(&mut p, if matches!(state, State::Hover | State::Active) { GOLD } else { COPPER }, cover(accent, aa) * if disabled { 0.3 } else { 0.9 });
            }
            // The symbol, solid.
            let colour = if disabled { SILVER_DARK } else if pressed { [1.0, 0.97, 0.9] } else { [1.0; 3] };
            let r = if wide { hh * 0.62 } else { c * 0.46 };
            paint_icon(&mut p, icon, x, y, r, c * 0.06, aa, colour);
            if disabled {
                let strike = segment(x, y, -hw * 0.6, hh * 0.6, hw * 0.6, -hh * 0.6) - c * 0.035;
                over(&mut p, RECESS, cover(strike - 1.5, aa) * 0.8);
                over(&mut p, SILVER_DARK, cover(strike, aa));
            }
        }
        Widget::Pages { count, current } => {
            // Diamonds on a thin bar; the current page filled in gold.
            let count = (*count).max(1) as f32;
            let spacing = size * 0.8 / count;
            let bar = segment(x, y, -size * 0.4, 0.0, size * 0.4, 0.0) - 1.0;
            over(&mut p, SILVER_DARK, cover(bar, aa) * 0.8);
            for i in 0..count as u8 {
                let cx = -size * 0.4 + spacing * (f32::from(i) + 0.5);
                let d = ((x - cx).abs() + y.abs()) * 0.7071 - c * 0.12;
                over(&mut p, RECESS, cover(d - 2.0, aa) * 0.7);
                over(&mut p, if i == *current { GOLD } else { SILVER }, cover(if i == *current { d } else { d.abs() - 1.2 }, aa));
            }
        }
    }
    p
}

/// A widget's image: `size` x `size` straight-alpha RGBA8, sRGB-encoded.
pub fn draw(widget: &Widget, size: u32, style: Style) -> Vec<u8> {
    let mut out = Vec::with_capacity((size * size * 4) as usize);
    let s = size as f32;
    for py in 0..size {
        for px in 0..size {
            let mut acc = [0.0f32; 4];
            for (ox, oy) in [(0.25, 0.25), (0.75, 0.25), (0.25, 0.75), (0.75, 0.75)] {
                let p = shade(widget, style, px as f32 + ox, py as f32 + oy, s);
                for i in 0..3 { acc[i] += p.0[i] * p.0[3]; }
                acc[3] += p.0[3];
            }
            let a = acc[3] / 4.0;
            let rgb = if acc[3] > 0.0 { [acc[0] / acc[3], acc[1] / acc[3], acc[2] / acc[3]] } else { [0.0; 3] };
            out.extend(rgb.map(|v| (v.clamp(0.0, 1.0) * 255.0).round() as u8));
            out.push((a.clamp(0.0, 1.0) * 255.0).round() as u8);
        }
    }
    out
}

/// Widget textures on a device, drawn once each.
pub struct Atlas {
    textures: HashMap<(Widget, u32, Style), (wgpu::Texture, wgpu::TextureView)>,
}

impl Default for Atlas {
    fn default() -> Self {
        Self { textures: HashMap::new() }
    }
}

impl Atlas {
    /// The widget's texture (an sRGB texture with straight alpha).
    pub fn view(&mut self, device: &wgpu::Device, queue: &wgpu::Queue, widget: &Widget, size: u32, style: Style) -> &wgpu::TextureView {
        let key = (widget.clone(), size, style);
        &self.textures.entry(key).or_insert_with(|| {
            let texture = device.create_texture(&wgpu::TextureDescriptor {
                label: Some("seraph glass"),
                size: wgpu::Extent3d { width: size, height: size, depth_or_array_layers: 1 },
                mip_level_count: 1,
                sample_count: 1,
                dimension: wgpu::TextureDimension::D2,
                format: wgpu::TextureFormat::Rgba8UnormSrgb,
                usage: wgpu::TextureUsages::TEXTURE_BINDING | wgpu::TextureUsages::COPY_DST,
                view_formats: &[],
            });
            queue.write_texture(
                texture.as_image_copy(),
                &draw(widget, size, style),
                wgpu::TexelCopyBufferLayout { offset: 0, bytes_per_row: Some(size * 4), rows_per_image: Some(size) },
                wgpu::Extent3d { width: size, height: size, depth_or_array_layers: 1 },
            );
            let view = texture.create_view(&Default::default());
            (texture, view)
        }).1
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn alpha(image: &[u8], size: u32, x: u32, y: u32) -> u8 {
        image[((y * size + x) * 4 + 3) as usize]
    }

    #[test]
    fn cursors_keep_an_open_stable_centre_and_buttons_a_solid_symbol() {
        let cursor = draw(&Widget::Cursor, 64, Style::default());
        assert!(alpha(&cursor, 64, 32, 32) > 200, "the gold centre point");
        assert!(alpha(&cursor, 64, 32, 24) < 40, "open between centre and halo");
        assert!(alpha(&cursor, 64, 32, 16) > 200, "the halo");
        let button = draw(&Widget::Button(Icon::Plus, State::Normal), 64, Style::default());
        let centre = &button[((32 * 64 + 32) * 4) as usize..][..4];
        assert!(centre[0] > 230 && centre[3] > 240, "the symbol is solid white: {centre:?}");
        let opaque = draw(&Widget::Button(Icon::None, State::Normal), 64, Style { reduced_transparency: true });
        let clear = draw(&Widget::Button(Icon::None, State::Normal), 64, Style::default());
        assert!(alpha(&opaque, 64, 32, 40) > alpha(&clear, 64, 32, 40), "reduced transparency fills the body");
        let body = &opaque[((40 * 64 + 32) * 4) as usize..][..3];
        assert!(body.iter().all(|v| *v < 120), "dark, for white symbols: {body:?}");
    }

    #[test]
    fn labels_draw_every_letter_they_use() {
        for word in ["ALL", "NONE", "START", "SELECT", "JUMP", "MENU", "CANCEL", "OK", "BACK", "DASH", "CHARGE"] {
            for c in word.chars() {
                assert!(!glyph(c).is_empty(), "{c}");
            }
        }
        let key = draw(&Widget::Key(Icon::Text("ALL".into()), State::Hover), 96, Style::default());
        assert!(key.chunks(4).filter(|p| p[0] > 240 && p[1] > 240 && p[3] > 240).count() > 50, "a white label");
    }
}
