//! The part of rcheevos' client API (rcheevos/include/rc_client.h, v12.5.0)
//! the client uses, and src/glue.c's. Structures are mirrored up to the
//! last field read; their layouts are checked against the C compiler's
//! (`tests`).
#![allow(non_camel_case_types)]

use std::ffi::{c_char, c_int, c_void};

/// `time_t`: 64 bits on every target the hosts build (64-bit Linux,
/// Android, Windows and macOS).
pub type time_t = i64;

#[repr(C)]
pub struct rc_client_t {
    _opaque: [u8; 0],
}

#[repr(C)]
pub struct rc_client_async_handle_t {
    _opaque: [u8; 0],
}

/// A request to send (its storage follows these fields).
#[repr(C)]
pub struct rc_api_request_t {
    pub url: *const c_char,
    pub post_data: *const c_char,
    pub content_type: *const c_char,
}

#[repr(C)]
pub struct rc_api_server_response_t {
    pub body: *const c_char,
    pub body_length: usize,
    pub http_status_code: c_int,
}

/// A response that never arrived (no connection, a timeout): rcheevos
/// tries again later.
pub const RC_API_SERVER_RESPONSE_RETRYABLE_CLIENT_ERROR: c_int = -2;

#[repr(C)]
pub struct rc_client_user_t {
    pub display_name: *const c_char,
    pub username: *const c_char,
    pub token: *const c_char,
    pub score: u32,
    pub score_softcore: u32,
}

#[repr(C)]
#[derive(Default)]
pub struct rc_client_user_game_summary_t {
    pub num_core_achievements: u32,
    pub num_unofficial_achievements: u32,
    pub num_unlocked_achievements: u32,
    pub num_unsupported_achievements: u32,
    pub points_core: u32,
    pub points_unlocked: u32,
    pub beaten_time: time_t,
    pub completed_time: time_t,
}

#[repr(C)]
pub struct rc_client_game_t {
    pub id: u32,
    pub console_id: u32,
    pub title: *const c_char,
    pub hash: *const c_char,
}

#[repr(C)]
pub struct rc_client_achievement_t {
    pub title: *const c_char,
    pub description: *const c_char,
    pub badge_name: [c_char; 8],
    pub measured_progress: [c_char; 24],
    pub measured_percent: f32,
    pub id: u32,
    pub points: u32,
    pub unlock_time: time_t,
    pub state: u8,
    pub category: u8,
    pub bucket: u8,
    pub unlocked: u8,
    pub rarity: f32,
    pub rarity_hardcore: f32,
    pub kind: u8,
    pub badge_url: *const c_char,
}

#[repr(C)]
pub struct rc_client_leaderboard_t {
    pub title: *const c_char,
    pub description: *const c_char,
    pub tracker_value: *const c_char,
}

#[repr(C)]
pub struct rc_client_server_error_t {
    pub error_message: *const c_char,
    pub api: *const c_char,
    pub result: c_int,
}

#[repr(C)]
pub struct rc_client_subset_t {
    pub id: u32,
    pub title: *const c_char,
}

#[repr(C)]
pub struct rc_client_event_t {
    pub kind: u32,
    pub achievement: *const rc_client_achievement_t,
    pub leaderboard: *const rc_client_leaderboard_t,
    pub leaderboard_tracker: *const c_void,
    pub leaderboard_scoreboard: *const c_void,
    pub server_error: *const rc_client_server_error_t,
    pub subset: *const rc_client_subset_t,
}

pub const RC_OK: c_int = 0;
pub const RC_NO_GAME_LOADED: c_int = -29;
pub const RC_ABORTED: c_int = -31;
pub const RC_INVALID_CREDENTIALS: c_int = -34;
pub const RC_EXPIRED_TOKEN: c_int = -35;

pub const RC_CLIENT_LOG_LEVEL_INFO: c_int = 3;

/// An achievement's state, as its image shows it (unlocked: in colour).
pub const RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED: c_int = 2;

pub const RC_CLIENT_EVENT_ACHIEVEMENT_TRIGGERED: u32 = 1;
pub const RC_CLIENT_EVENT_LEADERBOARD_STARTED: u32 = 2;
pub const RC_CLIENT_EVENT_LEADERBOARD_FAILED: u32 = 3;
pub const RC_CLIENT_EVENT_LEADERBOARD_SUBMITTED: u32 = 4;
pub const RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_SHOW: u32 = 7;
pub const RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_UPDATE: u32 = 9;
pub const RC_CLIENT_EVENT_GAME_COMPLETED: u32 = 15;
pub const RC_CLIENT_EVENT_SERVER_ERROR: u32 = 16;
pub const RC_CLIENT_EVENT_DISCONNECTED: u32 = 17;
pub const RC_CLIENT_EVENT_RECONNECTED: u32 = 18;
pub const RC_CLIENT_EVENT_SUBSET_COMPLETED: u32 = 19;

pub type rc_client_read_memory_func_t = unsafe extern "C" fn(address: u32, buffer: *mut u8, num_bytes: u32, client: *mut rc_client_t) -> u32;
pub type rc_client_server_callback_t = unsafe extern "C" fn(server_response: *const rc_api_server_response_t, callback_data: *mut c_void);
pub type rc_client_server_call_t = unsafe extern "C" fn(request: *const rc_api_request_t, callback: rc_client_server_callback_t, callback_data: *mut c_void, client: *mut rc_client_t);
pub type rc_client_callback_t = unsafe extern "C" fn(result: c_int, error_message: *const c_char, client: *mut rc_client_t, userdata: *mut c_void);
pub type rc_client_message_callback_t = unsafe extern "C" fn(message: *const c_char, client: *const rc_client_t);
pub type rc_client_event_handler_t = unsafe extern "C" fn(event: *const rc_client_event_t, client: *mut rc_client_t);

/// src/glue.c's sector reader.
pub type xem_read_sector_t = unsafe extern "C" fn(source: *mut c_void, sector: u32, buffer: *mut c_void, bytes: usize) -> usize;

unsafe extern "C" {
    pub fn rc_client_create(read_memory: rc_client_read_memory_func_t, server_call: rc_client_server_call_t) -> *mut rc_client_t;
    pub fn rc_client_destroy(client: *mut rc_client_t);
    pub fn rc_client_set_hardcore_enabled(client: *mut rc_client_t, enabled: c_int);
    pub fn rc_client_set_userdata(client: *mut rc_client_t, userdata: *mut c_void);
    pub fn rc_client_get_userdata(client: *const rc_client_t) -> *mut c_void;
    pub fn rc_client_set_host(client: *mut rc_client_t, hostname: *const c_char);
    pub fn rc_client_get_user_agent_clause(client: *mut rc_client_t, buffer: *mut c_char, buffer_size: usize) -> usize;
    pub fn rc_client_enable_logging(client: *mut rc_client_t, level: c_int, callback: rc_client_message_callback_t);
    pub fn rc_client_set_event_handler(client: *mut rc_client_t, handler: rc_client_event_handler_t);
    pub fn rc_client_begin_login_with_password(client: *mut rc_client_t, username: *const c_char, password: *const c_char,
        callback: rc_client_callback_t, userdata: *mut c_void) -> *mut rc_client_async_handle_t;
    pub fn rc_client_begin_login_with_token(client: *mut rc_client_t, username: *const c_char, token: *const c_char,
        callback: rc_client_callback_t, userdata: *mut c_void) -> *mut rc_client_async_handle_t;
    pub fn rc_client_logout(client: *mut rc_client_t);
    pub fn rc_client_get_user_info(client: *const rc_client_t) -> *const rc_client_user_t;
    pub fn rc_client_get_user_game_summary(client: *const rc_client_t, summary: *mut rc_client_user_game_summary_t);
    pub fn rc_client_begin_load_game(client: *mut rc_client_t, hash: *const c_char,
        callback: rc_client_callback_t, userdata: *mut c_void) -> *mut rc_client_async_handle_t;
    pub fn rc_client_begin_change_media(client: *mut rc_client_t, hash: *const c_char,
        callback: rc_client_callback_t, userdata: *mut c_void) -> *mut rc_client_async_handle_t;
    pub fn rc_client_unload_game(client: *mut rc_client_t);
    pub fn rc_client_get_game_info(client: *const rc_client_t) -> *const rc_client_game_t;
    pub fn rc_client_achievement_get_image_url(achievement: *const rc_client_achievement_t, state: c_int, buffer: *mut c_char,
        buffer_size: usize) -> c_int;
    pub fn rc_client_do_frame(client: *mut rc_client_t);
    pub fn rc_client_idle(client: *mut rc_client_t);
    pub fn rc_client_reset(client: *mut rc_client_t);
    pub fn rc_client_deserialize_progress(client: *mut rc_client_t, serialized: *const u8) -> c_int;

    pub fn xem_rc_disc_hash(hash: *mut c_char, source: *mut c_void, read: xem_read_sector_t, error: *mut c_char) -> c_int;
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::mem::{offset_of, size_of};

    unsafe extern "C" {
        /// src/glue.c's: the C compiler's offsets and sizes, in its order.
        fn xem_rc_layout(out: *mut usize, capacity: usize) -> usize;
    }

    #[test]
    fn the_mirrored_structures_lie_where_the_c_compiler_lays_them() {
        // Sizes only where a structure is mirrored whole.
        let rust = [
            None, Some(offset_of!(rc_api_request_t, url)), Some(offset_of!(rc_api_request_t, post_data)),
            Some(offset_of!(rc_api_request_t, content_type)),
            Some(size_of::<rc_api_server_response_t>()), Some(offset_of!(rc_api_server_response_t, body)),
            Some(offset_of!(rc_api_server_response_t, body_length)), Some(offset_of!(rc_api_server_response_t, http_status_code)),
            None, Some(offset_of!(rc_client_user_t, display_name)), Some(offset_of!(rc_client_user_t, username)),
            Some(offset_of!(rc_client_user_t, token)), Some(offset_of!(rc_client_user_t, score)), Some(offset_of!(rc_client_user_t, score_softcore)),
            Some(size_of::<rc_client_user_game_summary_t>()), Some(offset_of!(rc_client_user_game_summary_t, num_core_achievements)),
            Some(offset_of!(rc_client_user_game_summary_t, num_unlocked_achievements)), Some(offset_of!(rc_client_user_game_summary_t, points_core)),
            Some(offset_of!(rc_client_user_game_summary_t, points_unlocked)),
            None, Some(offset_of!(rc_client_game_t, id)), Some(offset_of!(rc_client_game_t, title)),
            Some(offset_of!(rc_client_game_t, hash)),
            None, Some(offset_of!(rc_client_achievement_t, title)),
            Some(offset_of!(rc_client_achievement_t, description)), Some(offset_of!(rc_client_achievement_t, measured_progress)),
            Some(offset_of!(rc_client_achievement_t, id)), Some(offset_of!(rc_client_achievement_t, points)),
            Some(offset_of!(rc_client_achievement_t, unlocked)), Some(offset_of!(rc_client_achievement_t, badge_url)),
            None, Some(offset_of!(rc_client_leaderboard_t, title)),
            Some(offset_of!(rc_client_leaderboard_t, tracker_value)),
            None, Some(offset_of!(rc_client_server_error_t, error_message)),
            Some(offset_of!(rc_client_server_error_t, api)), Some(offset_of!(rc_client_server_error_t, result)),
            None, Some(offset_of!(rc_client_subset_t, title)),
            Some(size_of::<rc_client_event_t>()), Some(offset_of!(rc_client_event_t, achievement)), Some(offset_of!(rc_client_event_t, leaderboard)),
            Some(offset_of!(rc_client_event_t, server_error)), Some(offset_of!(rc_client_event_t, subset)),
        ];
        let mut c = [0usize; 64];
        let count = unsafe { xem_rc_layout(c.as_mut_ptr(), c.len()) };
        assert_eq!(count, rust.len(), "glue.c and this test list the same fields");
        for (index, (rust, c)) in rust.iter().zip(c).enumerate() {
            if let Some(rust) = rust {
                assert_eq!(*rust, c, "layout entry {index}");
            }
        }
    }
}
