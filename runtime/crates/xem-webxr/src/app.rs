//! The WebXR test application: the flat canvas loop, immersive session entry
//! and exit, the session-scheduled frame loop and the state the test page and
//! its automation read.

use std::cell::RefCell;
use std::collections::BTreeMap;
use std::rc::{Rc, Weak};

use serde::Serialize;
use wasm_bindgen::JsCast;
use wasm_bindgen::prelude::*;
use wasm_bindgen_futures::{JsFuture, spawn_local};
use web_sys::{
    Event, HtmlCanvasElement, XrFrame, XrReferenceSpace, XrReferenceSpaceType, XrRenderStateInit,
    XrSession, XrSessionInit, XrSessionMode, XrSystem, XrView, XrWebGlLayer, XrWebGlLayerInit,
};
use xem_render::glam::{Mat4, Vec4};

use crate::graphics::{EyeView, XrGraphics, js_error};
use crate::input::{self, InputSource, Pose};

/// Features an immersive session must have and may have. Optional ones are
/// reported as granted only when the session says so.
const REQUIRED_FEATURES: [&str; 1] = ["local-floor"];
const OPTIONAL_FEATURES: [&str; 2] = ["bounded-floor", "hand-tracking"];
/// Session events counted as raw state.
const COUNTED_EVENTS: [&str; 7] = [
    "select",
    "selectstart",
    "selectend",
    "squeeze",
    "squeezestart",
    "squeezeend",
    "inputsourceschange",
];

/// Maps WebXR's OpenGL clip depth (-1..1) to wgpu's (0..1).
const GL_TO_WGPU_DEPTH: Mat4 = Mat4::from_cols(
    Vec4::X,
    Vec4::Y,
    Vec4::new(0.0, 0.0, 0.5, 0.0),
    Vec4::new(0.0, 0.0, 0.5, 1.0),
);

#[derive(Clone, Copy, Debug, PartialEq, Serialize)]
#[serde(rename_all = "kebab-case")]
enum Status {
    Flat,
    Requesting,
    Immersive,
    Ending,
}

/// What this browser offers, detected once at start.
#[derive(Clone, Debug, Serialize)]
#[serde(rename_all = "camelCase")]
struct Support {
    secure_context: bool,
    navigator_xr: bool,
    /// `None` when `navigator.xr` is missing or the query failed.
    immersive_vr: Option<bool>,
    immersive_vr_error: Option<String>,
    webgpu: bool,
    xr_gpu_binding: bool,
    xr_webgl_binding: bool,
    /// The submission path this build uses.
    submission: &'static str,
    adapter: String,
}

#[derive(Clone, Debug, Serialize)]
#[serde(rename_all = "camelCase")]
struct ViewInfo {
    eye: String,
    /// The layer viewport: x, y (from the bottom), width, height.
    viewport: [i32; 4],
    /// World to eye, column major.
    view: [f32; 16],
    /// The projection the renderer used (wgpu clip space), column major.
    proj: [f32; 16],
    /// The projection WebXR reported (OpenGL clip space), column major.
    xr_projection: [f32; 16],
    eye_position: [f32; 3],
}

#[derive(Clone, Debug, Serialize)]
#[serde(rename_all = "camelCase")]
struct FrameInfo {
    count: u64,
    xr_time: f64,
    scene_time: f64,
    paused: bool,
    tracked: bool,
    emulated_position: bool,
    /// "opaque" for the session's XR framebuffer, "default" for the canvas's.
    framebuffer: &'static str,
    layer_size: [u32; 2],
    viewer: Option<Pose>,
    views: Vec<ViewInfo>,
    render_error: Option<String>,
}

#[derive(Clone, Debug, Serialize)]
#[serde(rename_all = "camelCase")]
struct Readback {
    frame: u64,
    framebuffer: &'static str,
    points: Vec<[i32; 2]>,
    pixels: Vec<[u8; 4]>,
}

#[derive(Clone, Debug, Serialize)]
#[serde(rename_all = "camelCase")]
struct SessionInfo {
    mode: &'static str,
    granted_features: Option<Vec<String>>,
    reference_space: Option<&'static str>,
    visibility_state: String,
    environment_blend_mode: String,
    frame_rate: Option<f32>,
    reference_space_resets: u32,
    events: BTreeMap<&'static str, u32>,
}

#[derive(Serialize)]
#[serde(rename_all = "camelCase")]
struct State<'a> {
    status: Status,
    support: &'a Support,
    last_error: &'a Option<String>,
    paused: bool,
    scene_time: f64,
    flat_frames: u64,
    flat_running: bool,
    sessions_started: u32,
    sessions_ended: u32,
    session: Option<&'a SessionInfo>,
    frame: &'a Option<FrameInfo>,
    input: &'a [InputSource],
    readback: &'a Option<Readback>,
}

type FrameCallback = Closure<dyn FnMut(f64, XrFrame)>;
type EventCallback = Closure<dyn FnMut(Event)>;

struct Immersive {
    session: XrSession,
    layer: Option<XrWebGlLayer>,
    space: Option<XrReferenceSpace>,
    info: SessionInfo,
    frame_callback: Option<FrameCallback>,
    listeners: Vec<EventCallback>,
}

struct App {
    this: Weak<RefCell<App>>,
    graphics: XrGraphics,
    support: Support,
    status: Status,
    last_error: Option<String>,
    paused: bool,
    scene_time: f64,
    last_timestamp: Option<f64>,
    flat_frames: u64,
    flat_running: bool,
    flat_callback: Option<Closure<dyn FnMut(f64)>>,
    immersive: Option<Immersive>,
    /// An ended session's callbacks, dropped on the next flat frame rather
    /// than inside the `end` event that retires them.
    retired: Option<Immersive>,
    sessions_started: u32,
    sessions_ended: u32,
    frame: Option<FrameInfo>,
    input: Vec<InputSource>,
    readback_request: Option<Vec<[i32; 2]>>,
    readback: Option<Readback>,
}

/// The test application, exported to the page.
#[wasm_bindgen]
pub struct XemXr {
    app: Rc<RefCell<App>>,
}

#[wasm_bindgen]
impl XemXr {
    /// Detects WebXR support, creates the graphics device on `canvas` and
    /// starts the flat loop.
    pub async fn start(canvas: HtmlCanvasElement) -> Result<XemXr, JsValue> {
        std::panic::set_hook(Box::new(|info| {
            web_sys::console::error_1(&format!("xem-webxr: {info}").into());
        }));
        let window = web_sys::window().ok_or("no window")?;
        let global: &JsValue = window.as_ref();
        let has = |name: &str| {
            js_sys::Reflect::get(global, &name.into()).is_ok_and(|value| !value.is_undefined())
        };
        let navigator = window.navigator();
        let navigator: &JsValue = navigator.as_ref();
        let xr = js_sys::Reflect::get(navigator, &"xr".into())
            .ok()
            .filter(|value| !value.is_undefined() && !value.is_null());
        let (immersive_vr, immersive_vr_error) = match &xr {
            None => (None, None),
            Some(xr) => {
                let xr: &XrSystem = xr.unchecked_ref();
                match JsFuture::from(xr.is_session_supported(XrSessionMode::ImmersiveVr)).await {
                    Ok(value) => (value.as_bool(), None),
                    Err(error) => (None, Some(js_error(error))),
                }
            }
        };
        let webgpu = js_sys::Reflect::get(navigator, &"gpu".into())
            .is_ok_and(|value| !value.is_undefined() && !value.is_null());
        let graphics = XrGraphics::new(canvas, xr.is_some())
            .await
            .map_err(|error| JsValue::from_str(&error))?;
        let adapter = graphics.adapter_info();
        let support = Support {
            secure_context: window.is_secure_context(),
            navigator_xr: xr.is_some(),
            immersive_vr,
            immersive_vr_error,
            webgpu,
            xr_gpu_binding: has("XRGPUBinding"),
            xr_webgl_binding: has("XRWebGLBinding"),
            submission: "webgl2",
            adapter: format!("{} ({:?})", adapter.name, adapter.backend),
        };
        let app = Rc::new_cyclic(|this| {
            RefCell::new(App {
                this: this.clone(),
                graphics,
                support,
                status: Status::Flat,
                last_error: None,
                paused: false,
                scene_time: 0.0,
                last_timestamp: None,
                flat_frames: 0,
                flat_running: false,
                flat_callback: None,
                immersive: None,
                retired: None,
                sessions_started: 0,
                sessions_ended: 0,
                frame: None,
                input: Vec::new(),
                readback_request: None,
                readback: None,
            })
        });
        let weak = Rc::downgrade(&app);
        app.borrow_mut().flat_callback = Some(Closure::new(move |time: f64| {
            if let Some(app) = weak.upgrade() {
                App::flat_frame(&app, time);
            }
        }));
        app.borrow_mut().schedule_flat();
        Ok(XemXr { app })
    }

    /// Whether an immersive session can be requested now.
    #[wasm_bindgen(js_name = canEnterVr)]
    pub fn can_enter_vr(&self) -> bool {
        let app = self.app.borrow();
        app.status == Status::Flat && app.support.immersive_vr == Some(true)
    }

    /// Requests an immersive-vr session. Call it from a user gesture: the
    /// request is made before this returns, inside the gesture's activation.
    #[wasm_bindgen(js_name = enterVr)]
    pub fn enter_vr(&self) -> Result<(), JsValue> {
        let xr: XrSystem = {
            let app = self.app.borrow();
            if app.status != Status::Flat {
                return Err(format!("cannot enter VR while {:?}", app.status).into());
            }
            if app.support.immersive_vr != Some(true) {
                return Err("immersive-vr is not supported here".into());
            }
            js_sys::Reflect::get(
                web_sys::window().unwrap().navigator().as_ref(),
                &"xr".into(),
            )?
            .unchecked_into()
        };
        let init = XrSessionInit::new();
        init.set_required_features(&REQUIRED_FEATURES.map(JsValue::from));
        init.set_optional_features(&OPTIONAL_FEATURES.map(JsValue::from));
        let request = xr.request_session_with_options(XrSessionMode::ImmersiveVr, &init);
        {
            let mut app = self.app.borrow_mut();
            app.status = Status::Requesting;
            app.last_error = None;
        }
        let app = self.app.clone();
        spawn_local(async move {
            let result = match JsFuture::from(request).await {
                Ok(session) => App::start_session(&app, session.unchecked_into()).await,
                Err(error) => Err(format!("requestSession: {}", js_error(error))),
            };
            if let Err(error) = result {
                App::fail_session(&app, error);
            }
        });
        Ok(())
    }

    /// Ends the immersive session; the `end` event returns to the flat canvas.
    #[wasm_bindgen(js_name = exitVr)]
    pub fn exit_vr(&self) -> Result<(), JsValue> {
        let session = {
            let mut app = self.app.borrow_mut();
            let Some(immersive) = &app.immersive else {
                return Err("no immersive session".into());
            };
            let session = immersive.session.clone();
            app.status = Status::Ending;
            session
        };
        // Outside the borrow: a runtime may dispatch `end` synchronously.
        let ended = session.end();
        let app = self.app.clone();
        spawn_local(async move {
            if let Err(error) = JsFuture::from(ended).await {
                app.borrow_mut().last_error = Some(format!("session.end: {}", js_error(error)));
            }
        });
        Ok(())
    }

    /// Freezes or resumes scene time. Head tracking and rendering continue.
    #[wasm_bindgen(js_name = setPaused)]
    pub fn set_paused(&self, paused: bool) {
        self.app.borrow_mut().paused = paused;
    }

    /// Asks the next immersive frame to read back the layer framebuffer at
    /// `points` (x, y pairs; origin bottom-left) after submitting.
    #[wasm_bindgen(js_name = requestReadback)]
    pub fn request_readback(&self, points: Vec<i32>) {
        let points = points.chunks_exact(2).map(|p| [p[0], p[1]]).collect();
        let mut app = self.app.borrow_mut();
        app.readback_request = Some(points);
        app.readback = None;
    }

    /// The application state as JSON, for the overlay and automation.
    #[wasm_bindgen(js_name = stateJson)]
    pub fn state_json(&self) -> String {
        let app = self.app.borrow();
        serde_json::to_string(&State {
            status: app.status,
            support: &app.support,
            last_error: &app.last_error,
            paused: app.paused,
            scene_time: app.scene_time,
            flat_frames: app.flat_frames,
            flat_running: app.flat_running,
            sessions_started: app.sessions_started,
            sessions_ended: app.sessions_ended,
            session: app.immersive.as_ref().map(|immersive| &immersive.info),
            frame: &app.frame,
            input: &app.input,
            readback: &app.readback,
        })
        .expect("state serializes")
    }
}

impl App {
    fn schedule_flat(&mut self) {
        if self.flat_running {
            return;
        }
        let Some(callback) = &self.flat_callback else {
            return;
        };
        let window = web_sys::window().unwrap();
        if window
            .request_animation_frame(callback.as_ref().unchecked_ref())
            .is_ok()
        {
            self.flat_running = true;
        }
    }

    /// Advances scene time to `timestamp` (ms) unless paused.
    fn advance(&mut self, timestamp: f64) {
        if let Some(last) = self.last_timestamp {
            let dt = ((timestamp - last) / 1000.0).clamp(0.0, 0.1);
            if !self.paused {
                self.scene_time += dt;
            }
        }
        self.last_timestamp = Some(timestamp);
    }

    /// A window animation frame: flat presentation while no immersive
    /// session owns frame scheduling.
    fn flat_frame(app: &Rc<RefCell<App>>, time: f64) {
        let mut app = app.borrow_mut();
        app.flat_running = false;
        app.retired = None;
        if app.immersive.is_some() {
            // The session's own animation frames drive presentation now.
            return;
        }
        app.advance(time);
        let canvas_aspect = {
            let canvas = app.graphics.gl.canvas().unwrap();
            let canvas: HtmlCanvasElement = canvas.unchecked_into();
            canvas.width().max(1) as f32 / canvas.height().max(1) as f32
        };
        let (view, proj) = xem_render::demo_camera(canvas_aspect, app.scene_time as f32);
        let scene_time = app.scene_time as f32;
        match app.graphics.render_flat(view, proj, scene_time) {
            Ok(()) => app.flat_frames += 1,
            Err(error) => app.last_error = Some(error),
        }
        app.schedule_flat();
    }

    async fn start_session(app_rc: &Rc<RefCell<App>>, session: XrSession) -> Result<(), String> {
        let gl = app_rc.borrow().graphics.gl.clone();
        {
            let mut app = app_rc.borrow_mut();
            app.immersive = Some(Immersive {
                session: session.clone(),
                layer: None,
                space: None,
                info: SessionInfo {
                    mode: "immersive-vr",
                    granted_features: granted_features(&session),
                    reference_space: None,
                    visibility_state: input::js_string(&session, "visibilityState"),
                    environment_blend_mode: input::js_string(&session, "environmentBlendMode"),
                    frame_rate: session.frame_rate(),
                    reference_space_resets: 0,
                    events: BTreeMap::new(),
                },
                frame_callback: None,
                listeners: Vec::new(),
            });
            app.sessions_started += 1;
            Self::listen(&mut app, &session);
        }

        // The context was created XR-compatible when navigator.xr existed;
        // makeXRCompatible confirms it for this session's device.
        JsFuture::from(gl.make_xr_compatible())
            .await
            .map_err(|error| format!("makeXRCompatible: {}", js_error(error)))?;
        let layer_init = XrWebGlLayerInit::new();
        layer_init.set_antialias(false);
        // wgpu owns the depth buffer; the layer needs none.
        layer_init.set_depth(false);
        layer_init.set_stencil(false);
        layer_init.set_alpha(false);
        let layer = XrWebGlLayer::new_with_web_gl2_rendering_context_and_layer_init(
            &session,
            &gl,
            &layer_init,
        )
        .map_err(|error| format!("XRWebGLLayer: {}", js_error(error)))?;
        let render_state = XrRenderStateInit::new();
        render_state.set_base_layer(Some(&layer));
        session.update_render_state_with_state(&render_state);
        let space: XrReferenceSpace =
            JsFuture::from(session.request_reference_space(XrReferenceSpaceType::LocalFloor))
                .await
                .map_err(|error| format!("requestReferenceSpace: {}", js_error(error)))?
                .unchecked_into();

        let mut app = app_rc.borrow_mut();
        let this = app.this.clone();
        let Some(immersive) = app.immersive.as_mut() else {
            // The session ended while it was being set up.
            return Ok(());
        };
        if immersive.session != session {
            return Ok(());
        }
        let resets = this.clone();
        let reset: EventCallback = Closure::new(move |_: Event| {
            if let Some(app) = resets.upgrade()
                && let Some(immersive) = app.borrow_mut().immersive.as_mut()
            {
                immersive.info.reference_space_resets += 1;
            }
        });
        space
            .add_event_listener_with_callback("reset", reset.as_ref().unchecked_ref())
            .map_err(js_error)?;
        immersive.listeners.push(reset);
        immersive.layer = Some(layer);
        immersive.space = Some(space);
        immersive.info.reference_space = Some("local-floor");
        let callback: FrameCallback = Closure::new(move |time: f64, frame: XrFrame| {
            if let Some(app) = this.upgrade() {
                App::xr_frame(&app, time, frame);
            }
        });
        session.request_animation_frame(callback.as_ref().unchecked_ref());
        immersive.frame_callback = Some(callback);
        app.status = Status::Immersive;
        Ok(())
    }

    fn listen(app: &mut App, session: &XrSession) {
        let this = app.this.clone();
        let immersive = app.immersive.as_mut().unwrap();
        let ended = this.clone();
        let end: EventCallback = Closure::new(move |_: Event| {
            if let Some(app) = ended.upgrade() {
                App::session_ended(&app);
            }
        });
        let visibility = this.clone();
        let visibility: EventCallback = Closure::new(move |event: Event| {
            if let Some(app) = visibility.upgrade()
                && let Some(immersive) = app.borrow_mut().immersive.as_mut()
            {
                immersive.info.visibility_state = event
                    .target()
                    .map(|session| input::js_string(&session, "visibilityState"))
                    .unwrap_or_default();
            }
        });
        let mut add = |name: &str, callback: EventCallback| {
            if session
                .add_event_listener_with_callback(name, callback.as_ref().unchecked_ref())
                .is_ok()
            {
                immersive.listeners.push(callback);
            }
        };
        add("end", end);
        add("visibilitychange", visibility);
        for name in COUNTED_EVENTS {
            let counter = this.clone();
            add(
                name,
                Closure::new(move |_: Event| {
                    if let Some(app) = counter.upgrade()
                        && let Some(immersive) = app.borrow_mut().immersive.as_mut()
                    {
                        *immersive.info.events.entry(name).or_default() += 1;
                    }
                }),
            );
        }
    }

    /// A session animation frame: poses, views and the layer render.
    fn xr_frame(app_rc: &Rc<RefCell<App>>, time: f64, frame: XrFrame) {
        let mut guard = app_rc.borrow_mut();
        let app = &mut *guard;
        let Some(immersive) = app.immersive.as_ref() else {
            return;
        };
        let session = frame.session();
        if session != immersive.session {
            return;
        }
        if let Some(callback) = &immersive.frame_callback {
            session.request_animation_frame(callback.as_ref().unchecked_ref());
        }
        let (Some(layer), Some(space)) = (immersive.layer.clone(), immersive.space.clone()) else {
            return;
        };
        app.advance(time);
        let count = app.frame.as_ref().map_or(0, |frame| frame.count) + 1;
        let mut info = FrameInfo {
            count,
            xr_time: time,
            scene_time: app.scene_time,
            paused: app.paused,
            tracked: false,
            emulated_position: false,
            framebuffer: if layer.framebuffer().is_some() {
                "opaque"
            } else {
                "default"
            },
            layer_size: [layer.framebuffer_width(), layer.framebuffer_height()],
            viewer: None,
            views: Vec::new(),
            render_error: None,
        };
        app.input = input::snapshot(&session, &frame, &space);
        if let Some(pose) = frame.get_viewer_pose(&space) {
            info.tracked = true;
            info.emulated_position = pose.emulated_position();
            info.viewer = Some(Pose::from_xr(&pose));
            let mut eyes = Vec::new();
            for view in pose.views().iter() {
                let view: XrView = view.unchecked_into();
                let Some(viewport) = layer.get_viewport(&view) else {
                    continue;
                };
                let viewport = [
                    viewport.x(),
                    viewport.y(),
                    viewport.width(),
                    viewport.height(),
                ];
                let transform = view.transform();
                let world_to_eye = Mat4::from_cols_slice(&transform.inverse().matrix());
                let xr_projection = Mat4::from_cols_slice(&view.projection_matrix());
                let proj = GL_TO_WGPU_DEPTH * xr_projection;
                let position = transform.position();
                info.views.push(ViewInfo {
                    eye: input::js_string(&view, "eye"),
                    viewport,
                    view: world_to_eye.to_cols_array(),
                    proj: proj.to_cols_array(),
                    xr_projection: xr_projection.to_cols_array(),
                    eye_position: [
                        position.x() as f32,
                        position.y() as f32,
                        position.z() as f32,
                    ],
                });
                if viewport[2] > 0 && viewport[3] > 0 {
                    eyes.push(EyeView {
                        view: world_to_eye,
                        proj,
                        viewport,
                    });
                }
            }
            let scene_time = app.scene_time as f32;
            if let Err(error) = app.graphics.render_layer(&layer, &eyes, scene_time) {
                info.render_error = Some(error);
            } else if let Some(points) = app.readback_request.take() {
                let pixels = app.graphics.read_layer_pixels(&layer, &points);
                match pixels {
                    Ok(pixels) => {
                        app.readback = Some(Readback {
                            frame: count,
                            framebuffer: info.framebuffer,
                            points,
                            pixels,
                        })
                    }
                    Err(error) => app.last_error = Some(format!("readback: {error}")),
                }
            }
        }
        if let Some(immersive) = app.immersive.as_mut() {
            immersive.info.frame_rate = session.frame_rate();
        }
        app.frame = Some(info);
    }

    fn session_ended(app_rc: &Rc<RefCell<App>>) {
        let mut app = app_rc.borrow_mut();
        app.retired = app.immersive.take();
        app.graphics.release_layer();
        app.status = Status::Flat;
        app.sessions_ended += 1;
        app.input.clear();
        app.schedule_flat();
    }

    /// A session that could not start: end it if it exists and return to flat.
    fn fail_session(app_rc: &Rc<RefCell<App>>, error: String) {
        let session = {
            let mut app = app_rc.borrow_mut();
            app.last_error = Some(error);
            if app.immersive.is_none() {
                app.status = Status::Flat;
                app.schedule_flat();
            }
            app.immersive
                .as_ref()
                .map(|immersive| immersive.session.clone())
        };
        if let Some(session) = session {
            // Its `end` event returns the page to flat presentation.
            let _ = session.end();
        }
    }
}

/// `session.enabledFeatures`, where the browser exposes it.
fn granted_features(session: &XrSession) -> Option<Vec<String>> {
    let features = js_sys::Reflect::get(session, &"enabledFeatures".into()).ok()?;
    if !js_sys::Array::is_array(&features) {
        return None;
    }
    Some(
        js_sys::Array::from(&features)
            .iter()
            .filter_map(|feature| feature.as_string())
            .collect(),
    )
}
