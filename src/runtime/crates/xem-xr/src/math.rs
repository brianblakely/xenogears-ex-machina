//! OpenXR poses and fields of view as the renderer's matrices.

use glam::{Mat4, Quat, Vec3};
use openxr as xr;

/// A rigid transform in a reference space (metres, right-handed, y up).
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Pose {
    pub position: Vec3,
    pub orientation: Quat,
}

impl Pose {
    pub const IDENTITY: Self = Self {
        position: Vec3::ZERO,
        orientation: Quat::IDENTITY,
    };

    pub fn from_xr(pose: xr::Posef) -> Self {
        let p = pose.position;
        let o = pose.orientation;
        Self {
            position: Vec3::new(p.x, p.y, p.z),
            // Runtimes return unit quaternions; normalising guards against drift.
            orientation: Quat::from_xyzw(o.x, o.y, o.z, o.w).normalize(),
        }
    }

    pub fn to_xr(self) -> xr::Posef {
        let (p, o) = (self.position, self.orientation);
        xr::Posef {
            orientation: xr::Quaternionf {
                x: o.x,
                y: o.y,
                z: o.z,
                w: o.w,
            },
            position: xr::Vector3f {
                x: p.x,
                y: p.y,
                z: p.z,
            },
        }
    }

    /// The transform from this pose's local frame to its reference space.
    pub fn matrix(self) -> Mat4 {
        Mat4::from_rotation_translation(self.orientation, self.position)
    }
}

/// The four half-angles of an eye's field of view, in radians (left and down
/// are negative for a centred eye).
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Fov {
    pub left: f32,
    pub right: f32,
    pub up: f32,
    pub down: f32,
}

impl Fov {
    pub fn from_xr(fov: xr::Fovf) -> Self {
        Self {
            left: fov.angle_left,
            right: fov.angle_right,
            up: fov.angle_up,
            down: fov.angle_down,
        }
    }

    pub fn to_xr(self) -> xr::Fovf {
        xr::Fovf {
            angle_left: self.left,
            angle_right: self.right,
            angle_up: self.up,
            angle_down: self.down,
        }
    }

    /// The (generally asymmetric) right-handed projection of this field of
    /// view onto wgpu clip space: the view looks down -z and depth maps `near`
    /// to 0 and `far` to 1.
    pub fn projection(self, near: f32, far: f32) -> Mat4 {
        let (l, r) = (self.left.tan(), self.right.tan());
        let (d, u) = (self.down.tan(), self.up.tan());
        let depth = far / (near - far);
        Mat4::from_cols(
            [2.0 / (r - l), 0.0, 0.0, 0.0].into(),
            [0.0, 2.0 / (u - d), 0.0, 0.0].into(),
            [(r + l) / (r - l), (u + d) / (u - d), depth, -1.0].into(),
            [0.0, 0.0, depth * near, 0.0].into(),
        )
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use glam::Vec4;

    #[test]
    fn symmetric_fov_matches_glam_perspective() {
        let half = 45f32.to_radians();
        let fov = Fov {
            left: -half,
            right: half,
            up: half,
            down: -half,
        };
        let ours = fov.projection(0.1, 100.0);
        let glam = Mat4::perspective_rh(2.0 * half, 1.0, 0.1, 100.0);
        assert!(ours.abs_diff_eq(glam, 1e-5), "{ours:?} != {glam:?}");
    }

    #[test]
    fn asymmetric_fov_edges_map_to_clip_edges() {
        let fov = Fov {
            left: -0.9,
            right: 0.6,
            up: 0.7,
            down: -0.8,
        };
        let proj = fov.projection(0.05, 50.0);
        let ndc = |v: Vec3| {
            let c = proj * Vec4::new(v.x, v.y, v.z, 1.0);
            c.truncate() / c.w
        };
        let z = -2.0;
        let edge = |x: f32, y: f32| ndc(Vec3::new(x * -z, y * -z, z));
        assert!((edge(fov.left.tan(), 0.0).x + 1.0).abs() < 1e-5);
        assert!((edge(fov.right.tan(), 0.0).x - 1.0).abs() < 1e-5);
        assert!((edge(0.0, fov.up.tan()).y - 1.0).abs() < 1e-5);
        assert!((edge(0.0, fov.down.tan()).y + 1.0).abs() < 1e-5);
        assert!(ndc(Vec3::new(0.0, 0.0, -0.05)).z.abs() < 1e-5);
        assert!((ndc(Vec3::new(0.0, 0.0, -50.0)).z - 1.0).abs() < 1e-5);
    }

    #[test]
    fn pose_round_trips_through_openxr() {
        let pose = Pose {
            position: Vec3::new(0.1, 1.6, -0.3),
            orientation: Quat::from_rotation_y(0.4),
        };
        let back = Pose::from_xr(pose.to_xr());
        assert!(back.position.abs_diff_eq(pose.position, 1e-6));
        assert!(back.orientation.abs_diff_eq(pose.orientation, 1e-6));
    }
}
