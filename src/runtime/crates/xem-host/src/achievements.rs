//! RetroAchievements in the native hosts (docs/runtime.md,
//! RetroAchievements): the client (`xem_achievements`) beside the settings
//! panel's Achievements section, the HUD's notices and the game, shared by
//! the window (`View`, desktop and Android) and a headset's loop
//! (`xr_play`).

use std::cell::RefCell;
use std::path::Path;
use std::rc::Rc;
use std::time::Duration;

use xem_achievements::{Account, Achievements};
use xem_ui::{AchievementsAccount, AchievementsRequest, AchievementsView, FrameRateUi, NoticeKind, SettingsUi};

use crate::Game;

/// How the app names itself to the server (its list of clients): the
/// app and its version.
pub fn client_name() -> String {
    format!("XenogearsExMachina/{}", xem_ui::VERSION.trim_start_matches('v'))
}

/// The client, as a host follows it.
pub struct HostAchievements {
    client: Rc<RefCell<Achievements>>,
    /// A game ran at the last update: its end unloads its achievements.
    had_game: bool,
    /// The panel's sign-ins and sign-outs, for the next update.
    requests: Rc<RefCell<Vec<AchievementsRequest>>>,
}

impl HostAchievements {
    /// A client keeping the account in `directory` (beside the settings).
    pub fn new(directory: Option<&Path>) -> Self {
        Self::shared(Rc::new(RefCell::new(Achievements::new(directory, &client_name()))))
    }

    /// Another host's client (Android's window and its headset loop share
    /// one: one sign-in, one game).
    pub fn shared(client: Rc<RefCell<Achievements>>) -> Self {
        Self { client, had_game: false, requests: Rc::default() }
    }

    pub fn client(&self) -> Rc<RefCell<Achievements>> {
        self.client.clone()
    }

    /// The panel offers RetroAchievements; its Sign in and Sign out come here.
    pub fn attach(&self, ui: &SettingsUi) {
        ui.set_achievements_offered(true);
        let requests = self.requests.clone();
        ui.on_achievements_request(move |request| requests.borrow_mut().push(request));
    }

    /// Between the host's frames: the panel's requests, the game followed
    /// (`Game::follow_achievements`: its frames evaluated), what happened
    /// over the game on the HUD and the client's state in the panel. `ran`
    /// says whether the game ran frames at the last update.
    pub fn update(&mut self, game: Option<&mut Game>, ran: bool, ui: &SettingsUi, hud: &mut FrameRateUi, now: Duration) {
        for request in std::mem::take(&mut *self.requests.borrow_mut()) {
            let mut client = self.client.borrow_mut();
            match request {
                AchievementsRequest::SignIn { username, password } => {
                    if let Err(error) = client.sign_in(&username, &password) {
                        ui.set_operation_error(&error);
                    }
                }
                AchievementsRequest::SignOut => client.sign_out(),
            }
        }
        match game {
            Some(game) => {
                game.follow_achievements(&self.client, ran);
                self.had_game = true;
            }
            None => {
                let mut client = self.client.borrow_mut();
                if std::mem::take(&mut self.had_game) {
                    client.end();
                }
                client.update(None, false);
            }
        }
        let (notices, status) = {
            let mut client = self.client.borrow_mut();
            (client.take_notices(), client.status())
        };
        for notice in notices {
            let (kind, seconds) = match notice.kind {
                xem_achievements::NoticeKind::Unlocked => (NoticeKind::Achievement, 6),
                xem_achievements::NoticeKind::Completed => (NoticeKind::Achievement, 8),
                xem_achievements::NoticeKind::Info => (NoticeKind::Information, 4),
                xem_achievements::NoticeKind::Error => (NoticeKind::Failure, 8),
            };
            hud.notify(kind, &notice.title, &notice.detail, notice.badge.as_deref(), seconds, now);
        }
        ui.set_achievements(&view(&status));
    }

    /// Agents' commands (`--control`): `achievements`,
    /// `achievements_sign_in {username, password}`, `achievements_sign_out`.
    pub fn command(&mut self, command: &serde_json::Value) -> Option<serde_json::Value> {
        self.client.borrow_mut().command(command)
    }
}

/// What the panel shows of the client's status.
fn view(status: &xem_achievements::Status) -> AchievementsView {
    let (account, user) = match &status.account {
        Account::SignedOut => (AchievementsAccount::SignedOut, String::new()),
        Account::SigningIn => (AchievementsAccount::SigningIn, String::new()),
        Account::SignedIn { user, score, softcore_score } => (
            AchievementsAccount::SignedIn,
            format!("Signed in as {user} · {score} points{}", if *softcore_score > 0 { format!(" ({softcore_score} softcore)") } else { String::new() }),
        ),
    };
    AchievementsView { account, user, game: status.game.clone(), error: status.error.clone().unwrap_or_default() }
}
