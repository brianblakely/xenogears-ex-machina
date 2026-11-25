//! The server's requests: rcheevos hands each one over (its `server_call`)
//! and is answered later, and the client fetches an unlocked achievement's
//! badge image. A thread of their own makes them one after another with
//! `ureq` (HTTPS through rustls), and the answers go back to the host's
//! thread, which gives them to rcheevos or keeps the badge
//! (`Achievements::update`): rcheevos is only ever called there.

use std::sync::mpsc::{Receiver, Sender, channel};
use std::time::Duration;

/// A request, with where its answer goes.
pub(crate) struct Request {
    pub url: String,
    /// Form data to POST, or none to GET.
    pub post: Option<(String, String)>,
    pub reply: Reply,
}

/// Where an answer goes: to rcheevos, or to the badges kept (the image's
/// URL).
pub(crate) enum Reply {
    Rcheevos(Answer),
    Badge(String),
}

/// rcheevos' callback for a request and its data, opaque to the thread.
#[derive(Clone, Copy)]
pub(crate) struct Answer {
    pub callback: crate::ffi::rc_client_server_callback_t,
    pub data: usize,
}

pub(crate) struct Response {
    pub reply: Reply,
    /// The HTTP status, or `RC_API_SERVER_RESPONSE_RETRYABLE_CLIENT_ERROR`
    /// when no whole answer came (no connection, a timeout, the connection
    /// lost during the body).
    pub status: i32,
    /// The answer's body, or why none came: rcheevos gives that as the
    /// failure's message, which the panel shows ("Sign-in failed: …").
    pub body: Vec<u8>,
}

/// How long a request may take before it is given up (rcheevos tries an
/// unlock's again later).
const TIMEOUT: Duration = Duration::from_secs(30);

/// The largest badge image taken (RetroAchievements' are 64 pixels square,
/// a few kilobytes).
const BADGE_BYTES: u64 = 1 << 20;

/// Starts the thread; it ends when the requests' sender is dropped.
pub(crate) fn spawn(user_agent: String) -> (Sender<Request>, Receiver<Response>) {
    let (requests, incoming) = channel::<Request>();
    let (outgoing, responses) = channel();
    std::thread::Builder::new()
        .name("retroachievements".into())
        .spawn(move || {
            let agent: ureq::Agent = ureq::Agent::config_builder()
                .user_agent(user_agent)
                .timeout_global(Some(TIMEOUT))
                // The server answers its errors with a status and a body
                // that says why; rcheevos reads both.
                .http_status_as_error(false)
                .build()
                .into();
            for request in incoming {
                let response = match &request.post {
                    Some((content_type, form)) => agent.post(&request.url).header("Content-Type", content_type).send(form.as_str()),
                    None => agent.get(&request.url).call(),
                };
                let answer = response.and_then(|mut response| {
                    let status = i32::from(response.status().as_u16());
                    let body = match request.reply {
                        Reply::Rcheevos(_) => response.body_mut().read_to_vec(),
                        Reply::Badge(_) => response.body_mut().with_config().limit(BADGE_BYTES).read_to_vec(),
                    }?;
                    Ok((status, body))
                });
                let (status, body) = answer.unwrap_or_else(|error| {
                    (crate::ffi::RC_API_SERVER_RESPONSE_RETRYABLE_CLIENT_ERROR,
                        format!("RetroAchievements could not be reached ({error})").into_bytes())
                });
                if outgoing.send(Response { reply: request.reply, status, body }).is_err() {
                    break;
                }
            }
        })
        .expect("the requests' thread starts");
    (requests, responses)
}
