//! RetroAchievements for the native hosts (docs/runtime.md,
//! RetroAchievements): rcheevos' own client (vendored C, rcheevos/README.md)
//! signs the player in, loads the achievements RetroAchievements keeps for
//! the disc in the drive (by rcheevos' hash of it, [`disc_hash`]) and
//! evaluates them over the game's memory after every frame the game
//! completes ([`Evaluate`], the session's observer). The game's memory is
//! the PlayStation's address space (docs/runtime.md, The game module), so
//! the achievements written for the console read the same RAM.
//!
//! Hardcore mode is RetroAchievements': the host keeps it on in the session
//! (`Session::set_hardcore`) while the player is signed in with the
//! setting on ([`Achievements::follow`]). A hardcore session must play from
//! the console's reset with those restrictions throughout, so the client
//! asks the host to reset the console ([`Achievements::take_reset`]) when
//! the setting turns on during play (rcheevos' own request) or when the
//! achievements load over play that had them off.
//!
//! The account (the player's name and the token the server gave at
//! sign-in, never the password) is kept in [`ACCOUNT_FILE`] beside the
//! settings, outside the settings and their backups.

mod ffi;
mod http;

use std::cell::RefCell;
use std::ffi::{CStr, CString, c_char, c_int, c_void};
use std::path::{Path, PathBuf};
use std::rc::Rc;
use std::sync::mpsc::{Receiver, Sender};
use std::time::{Duration, Instant};

use serde::{Deserialize, Serialize};
use serde_json::{Value, json};
use xem_core::control::Observer;
use xem_core::memory::{GameMemory, RAM_BASE, RAM_SIZE, SCRATCHPAD_BASE, SCRATCHPAD_SIZE};
use xem_disc::{SECTOR_SIZE, Sector, SectorSource};

/// The account's file, in the directory the host gives (beside the settings).
pub const ACCOUNT_FILE: &str = "retroachievements.json";

/// An environment variable naming another server (`http://127.0.0.1:8080`,
/// a test's stand-in), for every native host.
pub const HOST_VARIABLE: &str = "XEM_RETROACHIEVEMENTS_HOST";

/// How long after a sign-in with the kept token failed for want of an
/// answer it is tried again.
const SIGN_IN_RETRY: Duration = Duration::from_secs(60);

/// What the account's file holds.
#[derive(Clone, Debug, PartialEq, Eq, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
struct Kept {
    username: String,
    token: String,
}

/// Whether the player is signed in.
#[derive(Clone, Debug, PartialEq, Eq, Serialize)]
pub enum Account {
    SignedOut,
    SigningIn,
    SignedIn {
        /// The name the server shows.
        user: String,
        /// Hardcore and softcore points.
        score: u32,
        softcore_score: u32,
    },
}

/// What the client shows the player (the panel's RetroAchievements card).
#[derive(Clone, Debug, PartialEq, Eq, Serialize)]
pub struct Status {
    pub account: Account,
    /// What the running game's achievements are doing.
    pub game: String,
    /// Why the last sign-in or load failed, until one succeeds.
    pub error: Option<String>,
    /// Hardcore mode is in effect: signed in with it on.
    pub hardcore: bool,
}

/// What a notice over the game says (the HUD's).
#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize)]
pub enum NoticeKind {
    /// An achievement unlocked.
    Unlocked,
    /// Every achievement of the game or of a set unlocked.
    Completed,
    /// The game's achievements loaded, a leaderboard, a sign-in.
    Info,
    /// A request the server refused, the server unreachable.
    Error,
}

#[derive(Clone, Debug, PartialEq, Eq, Serialize)]
pub struct Notice {
    pub kind: NoticeKind,
    pub title: String,
    pub detail: String,
}

/// An asynchronous call's kind, which its generation tells apart from
/// earlier calls of that kind.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
enum Call {
    SignIn,
    Load,
    ChangeDisc,
}

impl Call {
    fn userdata(self, generation: u64) -> *mut c_void {
        ((generation << 2) | self as u64) as usize as *mut c_void
    }

    fn of(userdata: *mut c_void) -> (Call, u64) {
        let value = userdata as usize as u64;
        let call = match value & 3 {
            0 => Call::SignIn,
            1 => Call::Load,
            _ => Call::ChangeDisc,
        };
        (call, value >> 2)
    }
}

/// What rcheevos' callbacks reach: the game's memory while a frame is
/// evaluated, the requests' thread, and what they leave for
/// [`Achievements::update`].
struct Inner {
    memory: Option<*const (dyn GameMemory + 'static)>,
    requests: Sender<http::Request>,
    notices: Vec<Notice>,
    reset: bool,
    outcomes: Vec<(Call, u64, c_int, String)>,
}

impl Inner {
    /// The host is to reset the console ([`Achievements::take_reset`]), and
    /// the player is told why.
    fn ask_reset(&mut self) {
        if !self.reset {
            self.reset = true;
            self.notices.push(Notice {
                kind: NoticeKind::Info,
                title: "Hardcore mode: the game restarts".into(),
                detail: "RetroAchievements' hardcore play begins at the console's start.".into(),
            });
        }
    }
}

/// Where the running game's achievements are.
#[derive(Clone, Debug, PartialEq, Eq)]
enum Load {
    /// No game runs.
    None,
    /// The game runs; its achievements load once the player is signed in.
    Waiting,
    Loading,
    Loaded,
    /// The server knows no game by the disc's hash.
    Unknown,
    Failed(String),
}

/// RetroAchievements' client.
pub struct Achievements {
    client: *mut ffi::rc_client_t,
    inner: *mut Inner,
    responses: Receiver<http::Response>,
    /// The account's file, if the host keeps one.
    file: Option<PathBuf>,
    kept: Option<Kept>,
    signing_in: bool,
    /// The kept token's sign-in failed unanswered: tried again then.
    retry: Option<Instant>,
    /// Each kind of call's latest ([`Call`]): an outcome of an earlier
    /// one is stale.
    generations: [u64; 3],
    /// The hash of the disc in the drive, while a game runs.
    hash: Option<String>,
    load: Load,
    error: Option<String>,
    /// The hardcore setting rcheevos has, and whether hardcore is in effect.
    hardcore: bool,
    active: bool,
    /// Hardcore has been in effect for every frame since the console's last
    /// reset (or boot).
    clean: bool,
}

impl Achievements {
    /// The client, signing in with the account kept in `directory`
    /// ([`ACCOUNT_FILE`]; none kept without one). `client` names the app
    /// to the server: its name and version (`XenogearsExMachina/1.6`),
    /// to which the system and rcheevos' version are added.
    pub fn new(directory: Option<&Path>, client: &str) -> Self {
        // SAFETY: rcheevos keeps nothing it is given here but the
        // callbacks; it calls them only once a sign-in begins, after the
        // userdata is set below.
        let handle = unsafe { ffi::rc_client_create(read_memory, server_call) };
        let mut clause = [0 as c_char; 64];
        // SAFETY: a buffer of its size; rcheevos ends it with a zero.
        unsafe { ffi::rc_client_get_user_agent_clause(handle, clause.as_mut_ptr(), clause.len()) };
        // The requests name the app as the server's list of clients expects:
        // `Name/version (system) rcheevos/version`.
        let (requests, responses) = http::spawn(format!("{client} ({}) {}", system(), text(clause.as_ptr())));
        let inner = Box::into_raw(Box::new(Inner { memory: None, requests, notices: Vec::new(), reset: false, outcomes: Vec::new() }));
        // SAFETY: `inner` lives until the client is destroyed (Drop).
        unsafe {
            ffi::rc_client_set_userdata(handle, inner.cast());
            ffi::rc_client_set_event_handler(handle, event);
            ffi::rc_client_enable_logging(handle, ffi::RC_CLIENT_LOG_LEVEL_INFO, log);
            // rcheevos starts in hardcore: the setting decides (`follow`).
            ffi::rc_client_set_hardcore_enabled(handle, 0);
            if let Some(host) = std::env::var(HOST_VARIABLE).ok().filter(|host| !host.is_empty()).and_then(|host| CString::new(host).ok()) {
                ffi::rc_client_set_host(handle, host.as_ptr());
            }
        }
        let file = directory.map(|directory| directory.join(ACCOUNT_FILE));
        let kept = file.as_deref().and_then(read_account);
        Achievements {
            client: handle,
            inner,
            responses,
            file,
            // A kept account signs in at the first update.
            retry: kept.as_ref().map(|_| Instant::now()),
            kept,
            signing_in: false,
            generations: [0; 3],
            hash: None,
            load: Load::None,
            error: None,
            hardcore: false,
            active: false,
            clean: false,
        }
    }

    /// Another server than retroachievements.org (`http://127.0.0.1:8080`,
    /// a test's stand-in), before the first update.
    pub fn set_host(&mut self, host: &str) {
        if let Ok(host) = CString::new(host) {
            // SAFETY: rcheevos copies the name.
            unsafe { ffi::rc_client_set_host(self.client, host.as_ptr()) };
        }
    }

    fn inner(&mut self) -> &mut Inner {
        // SAFETY: the box lives as long as `self`; rcheevos' callbacks reach
        // it only during calls made from `self`'s methods, none of which
        // holds this borrow across one.
        unsafe { &mut *self.inner }
    }

    /// A new call of `call`'s kind: earlier calls' outcomes are stale.
    fn next(&mut self, call: Call) -> *mut c_void {
        self.generations[call as usize] += 1;
        call.userdata(self.generations[call as usize])
    }

    /// Outcomes of the game's calls still under way are stale.
    fn forget_game_calls(&mut self) {
        self.generations[Call::Load as usize] += 1;
        self.generations[Call::ChangeDisc as usize] += 1;
    }

    fn sign_in_with_token(&mut self) {
        let Some(kept) = self.kept.clone() else { return };
        let (Ok(username), Ok(token)) = (CString::new(kept.username), CString::new(kept.token)) else { return };
        self.signing_in = true;
        self.retry = None;
        let userdata = self.next(Call::SignIn);
        // SAFETY: rcheevos copies the strings before returning.
        unsafe { ffi::rc_client_begin_login_with_token(self.client, username.as_ptr(), token.as_ptr(), completed, userdata) };
    }

    /// Sign in with the player's name and password (the panel's Sign in):
    /// the server's token is kept, never the password. The outcome shows
    /// in [`Self::status`] after the next [`Self::update`]s.
    pub fn sign_in(&mut self, username: &str, password: &str) -> Result<(), String> {
        let username = username.trim();
        if username.is_empty() || password.is_empty() {
            return Err("Enter your RetroAchievements user name and password.".into());
        }
        let (username, password) = (CString::new(username).map_err(|e| e.to_string())?, CString::new(password).map_err(|e| e.to_string())?);
        self.sign_out();
        self.signing_in = true;
        self.error = None;
        let userdata = self.next(Call::SignIn);
        // SAFETY: rcheevos copies the strings before returning.
        unsafe { ffi::rc_client_begin_login_with_password(self.client, username.as_ptr(), password.as_ptr(), completed, userdata) };
        Ok(())
    }

    /// Sign out: the kept account is forgotten and the game's achievements
    /// unload (hardcore mode no longer applies).
    pub fn sign_out(&mut self) {
        // SAFETY: the client is live.
        unsafe { ffi::rc_client_logout(self.client) };
        self.generations[Call::SignIn as usize] += 1;
        self.forget_game_calls();
        self.signing_in = false;
        self.retry = None;
        self.kept = None;
        self.error = None;
        if let Some(file) = &self.file {
            let _ = std::fs::remove_file(file);
        }
        if self.hash.is_some() {
            self.load = Load::Waiting;
        }
    }

    /// Whether the player is signed in (an account is kept, its sign-in
    /// made or under way).
    pub fn signed_in(&self) -> bool {
        self.kept.is_some()
    }

    /// Follow the hardcore setting: rcheevos takes it, and it is in effect
    /// while the player is signed in. Returns whether it is in effect, as
    /// the host keeps it in the session (`Session::set_hardcore`).
    pub fn follow(&mut self, hardcore: bool) -> bool {
        if hardcore != self.hardcore {
            self.hardcore = hardcore;
            // SAFETY: the client is live; a reset it asks for comes
            // through `event`.
            unsafe { ffi::rc_client_set_hardcore_enabled(self.client, c_int::from(hardcore)) };
        }
        self.active = hardcore && self.signed_in();
        self.active
    }

    /// A game booted from the disc whose hash is `hash` (rcheevos', of the
    /// disc in the drive): the last game's achievements unload, this one's
    /// load once the player is signed in. Hardcore play starts clean.
    pub fn boot(&mut self, hash: Result<String, String>) {
        // SAFETY: the client is live.
        unsafe { ffi::rc_client_unload_game(self.client) };
        self.forget_game_calls();
        self.clean = true;
        match hash {
            Ok(hash) => {
                self.hash = Some(hash);
                self.load = Load::Waiting;
                self.error = None;
            }
            Err(error) => {
                self.hash = None;
                self.load = Load::Failed(format!("The disc's RetroAchievements hash could not be made: {error}"));
            }
        }
    }

    /// The drive took another disc of the game (the story's disc change):
    /// rcheevos checks that it is the same game's.
    pub fn change_disc(&mut self, hash: Result<String, String>) {
        let Ok(hash) = hash else { return };
        if self.hash.as_deref() == Some(hash.as_str()) {
            return;
        }
        self.hash = Some(hash.clone());
        if self.load != Load::Loaded {
            // A load under way or waiting takes the new disc's.
            if self.load == Load::Loading {
                // SAFETY: the client is live.
                unsafe { ffi::rc_client_unload_game(self.client) };
                self.load = Load::Waiting;
            }
            return;
        }
        let Ok(hash) = CString::new(hash) else { return };
        let userdata = self.next(Call::ChangeDisc);
        // SAFETY: rcheevos copies the hash before returning.
        unsafe { ffi::rc_client_begin_change_media(self.client, hash.as_ptr(), completed, userdata) };
    }

    /// The game ends (the host drops it): its achievements unload.
    pub fn end(&mut self) {
        // SAFETY: the client is live.
        unsafe { ffi::rc_client_unload_game(self.client) };
        self.forget_game_calls();
        self.hash = None;
        self.load = Load::None;
    }

    /// The game completed a frame: its achievements are evaluated over
    /// `memory`.
    pub fn frame(&mut self, memory: &dyn GameMemory) {
        if !self.active {
            self.clean = false;
        }
        self.with_memory(memory, |client| {
            // SAFETY: the client is live; the memory is reachable for the call.
            unsafe { ffi::rc_client_do_frame(client) }
        });
    }

    /// The game went to another point in time ([`Observer::timeline`]):
    /// the achievements' evaluation starts again (rcheevos' reset after the
    /// console's reset, which also ends a hardcore reset's wait).
    pub fn timeline(&mut self, reset: bool) {
        // SAFETY: the client is live.
        unsafe {
            if reset {
                ffi::rc_client_reset(self.client);
            } else {
                ffi::rc_client_deserialize_progress(self.client, std::ptr::null());
            }
        }
        self.clean = reset && self.active;
    }

    fn with_memory<R>(&mut self, memory: &dyn GameMemory, call: impl FnOnce(*mut ffi::rc_client_t) -> R) -> R {
        let pointer: *const (dyn GameMemory + '_) = memory;
        // SAFETY: only the lifetime is erased; the pointer is cleared below,
        // before `memory`'s borrow ends, and read only during `call`.
        self.inner().memory = Some(unsafe { std::mem::transmute::<*const (dyn GameMemory + '_), *const (dyn GameMemory + 'static)>(pointer) });
        let result = call(self.client);
        self.inner().memory = None;
        result
    }

    /// Between the host's frames: the server's answers are taken, what they
    /// finished is followed (a sign-in kept, a game's achievements loaded),
    /// a load starts once it can, and rcheevos' periodic work runs when no
    /// game frame ran (`ran`).
    pub fn update(&mut self, memory: Option<&dyn GameMemory>, ran: bool) {
        while let Ok(response) = self.responses.try_recv() {
            let server = ffi::rc_api_server_response_t {
                body: response.body.as_ptr().cast(),
                body_length: response.body.len(),
                http_status_code: response.status,
            };
            let answer = response.answer;
            // SAFETY: rcheevos' own callback with its own data, once; the
            // body lives until it returns.
            let deliver = |_: *mut ffi::rc_client_t| unsafe { (answer.callback)(&server, answer.data as *mut c_void) };
            match memory {
                Some(memory) => self.with_memory(memory, deliver),
                None => deliver(self.client),
            }
        }
        let outcomes = std::mem::take(&mut self.inner().outcomes);
        for (call, generation, result, message) in outcomes {
            if generation == self.generations[call as usize] {
                self.finished(call, result, message);
            }
        }
        if self.retry.is_some_and(|at| Instant::now() >= at) {
            self.sign_in_with_token();
        }
        if self.load == Load::Waiting && self.signed_in() && !self.signing_in
            && let Some(hash) = self.hash.clone().and_then(|hash| CString::new(hash).ok())
        {
            self.load = Load::Loading;
            let userdata = self.next(Call::Load);
            // SAFETY: rcheevos copies the hash before returning.
            unsafe { ffi::rc_client_begin_load_game(self.client, hash.as_ptr(), completed, userdata) };
        }
        if !ran {
            match memory {
                // SAFETY (both): the client is live.
                Some(memory) => self.with_memory(memory, |client| unsafe { ffi::rc_client_idle(client) }),
                None => unsafe { ffi::rc_client_idle(self.client) },
            }
        }
    }

    fn finished(&mut self, call: Call, result: c_int, message: String) {
        match call {
            Call::SignIn => {
                self.signing_in = false;
                if result == ffi::RC_OK {
                    // SAFETY: the client is live; the user's strings are
                    // copied before the next call.
                    let user = unsafe { ffi::rc_client_get_user_info(self.client).as_ref() };
                    let Some(user) = user else { return };
                    let kept = Kept { username: text(user.username), token: text(user.token) };
                    if self.kept.as_ref() != Some(&kept) {
                        if let Some(file) = &self.file
                            && let Err(error) = write_account(file, &kept)
                        {
                            self.error = Some(format!("Signed in, but the account could not be kept for the next start: {error}"));
                        }
                        self.kept = Some(kept);
                    }
                    self.inner().notices.push(Notice {
                        kind: NoticeKind::Info,
                        title: format!("Signed in as {}", text(user.display_name)),
                        detail: format!("RetroAchievements · {} points", user.score),
                    });
                } else if result == ffi::RC_INVALID_CREDENTIALS || result == ffi::RC_EXPIRED_TOKEN || self.kept.is_none() {
                    // A password refused, or a kept token no longer valid:
                    // signed out until the player signs in again.
                    let had = self.kept.take().is_some();
                    if let Some(file) = &self.file && had {
                        let _ = std::fs::remove_file(file);
                    }
                    self.error = Some(format!("Sign-in failed: {}", reason(result, &message)));
                } else {
                    // No answer: the kept token is tried again later.
                    self.error = Some(format!("Sign-in failed: {}; trying again in a minute.", reason(result, &message)));
                    self.retry = Some(Instant::now() + SIGN_IN_RETRY);
                }
            }
            Call::Load => {
                if result == ffi::RC_OK {
                    self.load = Load::Loaded;
                    self.error = None;
                    let notice = self.loaded_notice();
                    self.inner().notices.push(notice);
                    // Hardcore play must start at the console's reset
                    // with its restrictions throughout.
                    if self.active && !self.clean {
                        self.inner().ask_reset();
                    }
                } else if result == ffi::RC_NO_GAME_LOADED {
                    self.load = Load::Unknown;
                } else if result == ffi::RC_ABORTED {
                    // Unloaded or signed out meanwhile.
                } else {
                    let error = reason(result, &message);
                    self.load = Load::Failed(error.clone());
                    self.inner().notices.push(Notice { kind: NoticeKind::Error, title: "Achievements did not load".into(), detail: error });
                }
            }
            Call::ChangeDisc => {
                if result != ffi::RC_OK && result != ffi::RC_ABORTED {
                    self.inner().notices.push(Notice { kind: NoticeKind::Error, title: "Disc not recognized".into(), detail: reason(result, &message) });
                }
            }
        }
    }

    fn summary(&self) -> ffi::rc_client_user_game_summary_t {
        let mut summary = ffi::rc_client_user_game_summary_t::default();
        // SAFETY: a summary rcheevos fills.
        unsafe { ffi::rc_client_get_user_game_summary(self.client, &mut summary) };
        summary
    }

    fn title(&self) -> String {
        // SAFETY: the client is live; the title is copied.
        unsafe { ffi::rc_client_get_game_info(self.client).as_ref() }.map_or_default(|game| text(game.title))
    }

    fn loaded_notice(&self) -> Notice {
        let summary = self.summary();
        let mode = if self.active { " · hardcore" } else { "" };
        Notice {
            kind: NoticeKind::Info,
            title: self.title(),
            detail: if summary.num_core_achievements == 0 {
                format!("No achievements yet{mode}")
            } else {
                format!("{} of {} achievements unlocked{mode}", summary.num_unlocked_achievements, summary.num_core_achievements)
            },
        }
    }

    /// What the panel shows.
    pub fn status(&self) -> Status {
        // SAFETY: the client is live; the strings are copied.
        let user = unsafe { ffi::rc_client_get_user_info(self.client).as_ref() };
        let account = match (user, self.signing_in) {
            (_, true) => Account::SigningIn,
            (Some(user), false) => Account::SignedIn { user: text(user.display_name), score: user.score, softcore_score: user.score_softcore },
            // A kept account whose sign-in waits to be tried again.
            (None, false) if self.kept.is_some() => Account::SigningIn,
            (None, false) => Account::SignedOut,
        };
        let game = match &self.load {
            Load::None => "No game: play a disc to load its achievements.".into(),
            Load::Waiting if !self.signed_in() => "Sign in to load the game's achievements.".into(),
            Load::Waiting | Load::Loading => "Loading the game's achievements…".into(),
            Load::Loaded => {
                let summary = self.summary();
                format!("{}: {} of {} achievements unlocked, {} of {} points.", self.title(), summary.num_unlocked_achievements,
                    summary.num_core_achievements, summary.points_unlocked, summary.points_core)
            }
            Load::Unknown => "RetroAchievements knows no game by this disc.".into(),
            Load::Failed(error) => format!("The game's achievements did not load: {error}"),
        };
        Status { account, game, error: self.error.clone(), hardcore: self.active }
    }

    /// What happened over the game since the last call (the HUD's notices).
    pub fn take_notices(&mut self) -> Vec<Notice> {
        std::mem::take(&mut self.inner().notices)
    }

    /// Whether the host should reset the console now (hardcore turned on
    /// during play, or achievements loaded over play that had it off); the
    /// reset's timeline (`Observer::timeline`) starts the evaluation again.
    pub fn take_reset(&mut self) -> bool {
        std::mem::take(&mut self.inner().reset)
    }

    /// Agents' commands (`xem-desktop --control`): `achievements` (the
    /// status), `achievements_sign_in {username, password}` and
    /// `achievements_sign_out`, as the panel's. Another command is not the
    /// client's (`None`).
    pub fn command(&mut self, command: &Value) -> Option<Value> {
        let field = |name: &str| command.get(name).and_then(Value::as_str).unwrap_or_default();
        Some(match command.get("cmd").and_then(Value::as_str)? {
            "achievements" => serde_json::to_value(self.status()).expect("status"),
            "achievements_sign_in" => match self.sign_in(field("username"), field("password")) {
                Ok(()) => json!({ "ok": true, "message": "Signing in to RetroAchievements…" }),
                Err(error) => json!({ "error": error }),
            },
            "achievements_sign_out" => {
                self.sign_out();
                json!({ "ok": true, "message": "Signed out of RetroAchievements." })
            }
            _ => return None,
        })
    }
}

impl std::fmt::Debug for Achievements {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("Achievements").field("status", &self.status()).finish_non_exhaustive()
    }
}

impl Drop for Achievements {
    fn drop(&mut self) {
        // SAFETY: made in `new`, destroyed once; rcheevos calls nothing
        // after it is destroyed, so the box goes after it.
        unsafe {
            ffi::rc_client_destroy(self.client);
            drop(Box::from_raw(self.inner));
        }
    }
}

/// The session's [`Observer`] for a client the host shares: every frame
/// evaluated, every change of timeline followed.
pub struct Evaluate(pub Rc<RefCell<Achievements>>);

impl Observer for Evaluate {
    fn frame(&mut self, memory: &dyn GameMemory) {
        self.0.borrow_mut().frame(memory);
    }

    fn timeline(&mut self, reset: bool) {
        self.0.borrow_mut().timeline(reset);
    }
}

/// RetroAchievements' hash of the PlayStation disc `source` holds
/// (rcheevos' own: the boot program SYSTEM.CNF names, its name and
/// contents), by which the server knows the game.
pub fn disc_hash(source: &mut dyn SectorSource) -> Result<String, String> {
    let mut hash = [0 as c_char; 33];
    let mut error = [0 as c_char; 160];
    let mut source: &mut dyn SectorSource = source;
    let reader: *mut &mut dyn SectorSource = &mut source;
    // SAFETY: the reader outlives the call, which reads through it alone.
    let made = unsafe { ffi::xem_rc_disc_hash(hash.as_mut_ptr(), reader.cast(), read_sector, error.as_mut_ptr()) };
    if made != 0 {
        Ok(text(hash.as_ptr()))
    } else {
        let error = text(error.as_ptr());
        Err(if error.is_empty() { "not a PlayStation disc".into() } else { error })
    }
}

unsafe extern "C" fn read_sector(source: *mut c_void, sector: u32, buffer: *mut c_void, bytes: usize) -> usize {
    // SAFETY: `disc_hash`'s reader, for the duration of its call.
    let source = unsafe { &mut *source.cast::<&mut dyn SectorSource>() };
    let mut raw = [0u8; SECTOR_SIZE];
    if source.read_sector(sector, &mut raw).is_err() {
        return 0;
    }
    // The Form 1 window, as rcheevos' own reader gives a MODE2/2352 track.
    let data = Sector(&raw).form1_window();
    let count = bytes.min(data.len());
    // SAFETY: rcheevos' buffer holds `bytes`.
    unsafe { std::ptr::copy_nonoverlapping(data.as_ptr(), buffer.cast::<u8>(), count) };
    count
}

/// rcheevos' address `address` in the game's memory: RetroAchievements'
/// PlayStation map is RAM from 0 and the scratchpad from 0x200000.
fn locate(address: u32, bytes: u32) -> Option<(u32, u32)> {
    let (base, offset, size) = if address < RAM_SIZE {
        (RAM_BASE, address, RAM_SIZE)
    } else if (0x20_0000..0x20_0000 + SCRATCHPAD_SIZE).contains(&address) {
        (SCRATCHPAD_BASE, address - 0x20_0000, SCRATCHPAD_SIZE)
    } else {
        return None;
    };
    Some((base + offset, bytes.min(size - offset)))
}

fn inner_of<'a>(client: *const ffi::rc_client_t) -> Option<&'a mut Inner> {
    // SAFETY: the userdata `new` set; rcheevos calls back only from the
    // calls `Achievements` makes, on its thread.
    unsafe { ffi::rc_client_get_userdata(client).cast::<Inner>().as_mut() }
}

unsafe extern "C" fn read_memory(address: u32, buffer: *mut u8, num_bytes: u32, client: *mut ffi::rc_client_t) -> u32 {
    let Some(memory) = inner_of(client).and_then(|inner| inner.memory) else { return 0 };
    let Some((at, count)) = locate(address, num_bytes) else { return 0 };
    // SAFETY: the memory `with_memory` gives for the call; rcheevos' buffer
    // holds `num_bytes`.
    let (memory, out) = unsafe { (&*memory, std::slice::from_raw_parts_mut(buffer, count as usize)) };
    if memory.read(at, out).is_ok() { count } else { 0 }
}

unsafe extern "C" fn server_call(request: *const ffi::rc_api_request_t, callback: ffi::rc_client_server_callback_t, callback_data: *mut c_void,
    client: *mut ffi::rc_client_t) {
    let Some(inner) = inner_of(client) else { return };
    // SAFETY: rcheevos' request, valid for the call.
    let request = unsafe { &*request };
    let post = (!request.post_data.is_null()).then(|| {
        let content_type = if request.content_type.is_null() { "application/x-www-form-urlencoded".into() } else { text(request.content_type) };
        (content_type, text(request.post_data))
    });
    let answer = http::Answer { callback, data: callback_data as usize };
    let _ = inner.requests.send(http::Request { url: text(request.url), post, answer });
}

unsafe extern "C" fn completed(result: c_int, error_message: *const c_char, client: *mut ffi::rc_client_t, userdata: *mut c_void) {
    let Some(inner) = inner_of(client) else { return };
    let (call, generation) = Call::of(userdata);
    inner.outcomes.push((call, generation, result, text(error_message)));
}

unsafe extern "C" fn event(event: *const ffi::rc_client_event_t, client: *mut ffi::rc_client_t) {
    let Some(inner) = inner_of(client) else { return };
    // SAFETY: rcheevos' event and what it points at, valid for the call.
    let event = unsafe { &*event };
    let achievement = unsafe { event.achievement.as_ref() };
    let leaderboard = unsafe { event.leaderboard.as_ref() };
    let notice = |kind, title: String, detail: String| Some(Notice { kind, title, detail });
    let game = || unsafe { ffi::rc_client_get_game_info(client).as_ref() }.map_or_default(|game| text(game.title));
    let hardcore = unsafe { ffi::rc_client_get_hardcore_enabled(client) } != 0;
    let notice = match event.kind {
        ffi::RC_CLIENT_EVENT_ACHIEVEMENT_TRIGGERED => achievement.and_then(|a| notice(NoticeKind::Unlocked, text(a.title),
            format!("{} · {} points", text(a.description), a.points))),
        ffi::RC_CLIENT_EVENT_GAME_COMPLETED => notice(NoticeKind::Completed, format!("{} {}", if hardcore { "Mastered" } else { "Completed" }, game()),
            "Every achievement unlocked".into()),
        ffi::RC_CLIENT_EVENT_SUBSET_COMPLETED => unsafe { event.subset.as_ref() }.and_then(|subset| notice(NoticeKind::Completed,
            format!("Completed {}", text(subset.title)), "Every achievement of the set unlocked".into())),
        ffi::RC_CLIENT_EVENT_LEADERBOARD_STARTED => leaderboard.and_then(|l| notice(NoticeKind::Info, text(l.title), "Leaderboard attempt started".into())),
        ffi::RC_CLIENT_EVENT_LEADERBOARD_FAILED => leaderboard.and_then(|l| notice(NoticeKind::Info, text(l.title), "Leaderboard attempt failed".into())),
        ffi::RC_CLIENT_EVENT_LEADERBOARD_SUBMITTED => leaderboard.and_then(|l| notice(NoticeKind::Info, text(l.title),
            format!("Submitted {}", text(l.tracker_value)))),
        ffi::RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_SHOW | ffi::RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_UPDATE => {
            achievement.and_then(|a| notice(NoticeKind::Info, text(a.title), format!("Progress {}", text(a.measured_progress.as_ptr()))))
        }
        ffi::RC_CLIENT_EVENT_SERVER_ERROR => unsafe { event.server_error.as_ref() }.and_then(|error| notice(NoticeKind::Error,
            "RetroAchievements refused a request".into(), format!("{}: {}", text(error.api), text(error.error_message)))),
        ffi::RC_CLIENT_EVENT_DISCONNECTED => notice(NoticeKind::Error, "RetroAchievements unreachable".into(),
            "Unlocks are kept and sent once it answers.".into()),
        ffi::RC_CLIENT_EVENT_RECONNECTED => notice(NoticeKind::Info, "RetroAchievements reached again".into(), "Kept unlocks sent.".into()),
        ffi::RC_CLIENT_EVENT_RESET => {
            inner.ask_reset();
            None
        }
        _ => None,
    };
    inner.notices.extend(notice);
}

unsafe extern "C" fn log(message: *const c_char, _client: *const ffi::rc_client_t) {
    eprintln!("xem-achievements: {}", text(message));
}

/// A C string's text (empty for none).
fn text(pointer: *const c_char) -> String {
    if pointer.is_null() {
        return String::new();
    }
    // SAFETY: rcheevos' strings end with a zero.
    unsafe { CStr::from_ptr(pointer) }.to_string_lossy().into_owned()
}

/// Why a call failed: the server's words, else rcheevos' error's name.
fn reason(result: c_int, message: &str) -> String {
    if !message.is_empty() {
        return message.to_string();
    }
    match result {
        ffi::RC_INVALID_CREDENTIALS => "the user name or password is wrong".into(),
        ffi::RC_EXPIRED_TOKEN => "the session expired; sign in again".into(),
        _ => format!("error {result}"),
    }
}

/// The system, as the server's client list shows it.
fn system() -> &'static str {
    if cfg!(target_os = "android") {
        "Android"
    } else if cfg!(target_os = "linux") {
        "Linux"
    } else if cfg!(target_os = "windows") {
        "Windows"
    } else if cfg!(target_os = "macos") {
        "macOS"
    } else {
        "Unknown"
    }
}

fn read_account(path: &Path) -> Option<Kept> {
    let bytes = xem_storage::read(path, 64 * 1024).ok()?;
    serde_json::from_slice(&bytes).ok()
}

/// The account's file, readable by the player alone where the system
/// says who reads files.
fn write_account(path: &Path, kept: &Kept) -> std::io::Result<()> {
    xem_storage::replace(path, serde_json::to_string_pretty(kept).expect("account").as_bytes())?;
    #[cfg(unix)]
    {
        use std::os::unix::fs::PermissionsExt;
        std::fs::set_permissions(path, std::fs::Permissions::from_mode(0o600))?;
    }
    Ok(())
}

#[cfg(test)]
mod tests {
    #[test]
    fn retroachievements_playstation_addresses_are_ram_then_the_scratchpad() {
        use super::locate;
        assert_eq!(locate(0, 4), Some((0x8000_0000, 4)));
        assert_eq!(locate(0x1F_FFFE, 4), Some((0x801F_FFFE, 2)), "clipped at RAM's end");
        assert_eq!(locate(0x20_0000, 2), Some((0x1F80_0000, 2)));
        assert_eq!(locate(0x20_03FF, 4), Some((0x1F80_03FF, 1)), "clipped at the scratchpad's end");
        assert_eq!(locate(0x20_0400, 1), None);
    }

    #[test]
    fn calls_carry_their_kind_and_generation() {
        use super::Call;
        for call in [Call::SignIn, Call::Load, Call::ChangeDisc] {
            assert_eq!(Call::of(call.userdata(12345)), (call, 12345));
        }
    }
}
