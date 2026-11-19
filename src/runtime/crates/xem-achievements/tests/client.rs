//! The client against a stand-in RetroAchievements server (plain HTTP on
//! this machine, answering as rcheevos' own tests have the server answer):
//! sign-in by password and by the kept token, the game's achievements
//! loaded by the disc's hash, unlocks from the game's memory (RAM and the
//! scratchpad) sent as softcore, their notices with the achievements'
//! badges, and sign-out.
//!
//!     cargo test -p xem-achievements --test client

use std::time::{Duration, Instant};

use xem_achievements::{Account, Achievements, NoticeKind};
use xem_core::memory::{GameMemory, OutOfBounds};

mod stand_in;
use stand_in::requests;

const HASH: &str = "1bde22c83a069c43cb32b8521bfdf7ae";

/// The stand-in's game: one achievement on a byte of RAM (RetroAchievements'
/// address 0x1000, the console's 0x80001000), one on a byte of the
/// scratchpad (0x200010, the console's 0x1F800010).
fn server() -> (String, stand_in::Log) {
    stand_in::server(vec![(HASH, stand_in::game(11248, "Xenogears", &[
        (101, "Seven", "RAM holds seven", 5, "0xH001000=7"),
        (102, "Scratchpad", "The scratchpad holds nine", 10, "0xH200010=9"),
    ]))])
}

/// The game's memory as the console's: RAM and the scratchpad.
struct Memory {
    ram: Vec<u8>,
    scratchpad: Vec<u8>,
}

impl Memory {
    fn new() -> Self {
        Memory { ram: vec![0; 0x20_0000], scratchpad: vec![0; 0x400] }
    }
    fn region(&self, address: u32, len: usize) -> Option<(&[u8], usize)> {
        let (memory, base) = if address >= 0x8000_0000 { (&self.ram, 0x8000_0000) } else { (&self.scratchpad, 0x1F80_0000) };
        let at = address.checked_sub(base)? as usize;
        (at + len <= memory.len()).then_some((memory.as_slice(), at))
    }
}

impl GameMemory for Memory {
    fn read(&self, address: u32, out: &mut [u8]) -> Result<(), OutOfBounds> {
        let (memory, at) = self.region(address, out.len()).ok_or(OutOfBounds { address, len: out.len() })?;
        out.copy_from_slice(&memory[at..at + out.len()]);
        Ok(())
    }
    fn write(&mut self, address: u32, _: &[u8]) -> Result<(), OutOfBounds> {
        Err(OutOfBounds { address, len: 0 })
    }
}

/// Notices until one of `kind` came (an unlock's waits for its badge).
fn notices_until(client: &mut Achievements, memory: &Memory, kind: NoticeKind) -> Vec<xem_achievements::Notice> {
    let mut notices = Vec::new();
    pump(client, memory, |c| {
        notices.extend(c.take_notices());
        notices.iter().any(|n| n.kind == kind)
    });
    notices
}

/// Updates until `done` holds (the server answers on its own thread).
fn pump(client: &mut Achievements, memory: &Memory, mut done: impl FnMut(&mut Achievements) -> bool) {
    let deadline = Instant::now() + Duration::from_secs(10);
    while !done(client) {
        assert!(Instant::now() < deadline, "timed out: {:?}", client.status());
        client.update(Some(memory), false);
        std::thread::sleep(Duration::from_millis(5));
    }
}


fn directory(name: &str) -> std::path::PathBuf {
    let directory = std::env::temp_dir().join(format!("xem-achievements-{name}-{}", std::process::id()));
    let _ = std::fs::remove_dir_all(&directory);
    std::fs::create_dir_all(&directory).unwrap();
    directory
}

#[test]
fn a_player_signs_in_loads_the_games_achievements_and_unlocks_them_from_its_memory() {
    let (host, log) = server();
    let directory = directory("unlocks");
    let account = directory.join(xem_achievements::ACCOUNT_FILE);
    let mut client = Achievements::new(Some(&directory), "XemTest/1.0");
    client.set_host(&host);
    let mut memory = Memory::new();
    assert_eq!(client.status().account, Account::SignedOut);

    // A wrong password: refused with the server's words, nothing kept.
    client.sign_in("Player", "wrong").unwrap();
    pump(&mut client, &memory, |c| c.status().error.is_some());
    let status = client.status();
    assert_eq!(status.account, Account::SignedOut);
    assert!(status.error.as_deref().unwrap().contains("Invalid User/Password"), "{status:?}");
    assert!(!account.exists());

    // The right one: the token is kept for the next start, never the password.
    client.sign_in("Player", "secret").unwrap();
    pump(&mut client, &memory, |c| matches!(c.status().account, Account::SignedIn { .. }));
    assert_eq!(client.status().account, Account::SignedIn { user: "Player".into(), score: 1234, softcore_score: 56 });
    let kept = std::fs::read_to_string(&account).unwrap();
    assert!(kept.contains("TOKEN") && !kept.contains("secret"), "{kept}");
    #[cfg(unix)]
    {
        use std::os::unix::fs::PermissionsExt;
        assert_eq!(std::fs::metadata(&account).unwrap().permissions().mode() & 0o777, 0o600, "the player's alone");
    }
    let login = &requests(&log, "login2")[1];
    let agent = &login["user-agent"];
    assert!(agent.starts_with("XemTest/1.0 (") && agent.ends_with(") rcheevos/12.5"), "{agent}");
    assert!(client.take_notices().iter().any(|n| n.title == "Signed in as Player"));

    // The game boots: its achievements load by the disc's hash.
    client.boot(Ok(HASH.into()));
    pump(&mut client, &memory, |c| c.status().game.starts_with("Xenogears:"));
    assert_eq!(client.status().game, "Xenogears: 0 of 2 achievements unlocked, 0 of 15 points.");
    let loaded = client.take_notices();
    assert_eq!((loaded[0].title.as_str(), loaded[0].detail.as_str()), ("Xenogears", "0 of 2 achievements unlocked"));
    assert_eq!(requests(&log, "achievementsets")[0]["m"], HASH);

    // An achievement's condition, false then true, in RAM.
    for _ in 0..3 { client.frame(&memory); }
    assert!(client.take_notices().is_empty());
    memory.ram[0x1000] = 7;
    client.frame(&memory);
    // The unlock's notice waits for the achievement's badge, which the
    // client asks the server for.
    assert!(client.take_notices().is_empty(), "the badge is not here yet");
    let unlocked = notices_until(&mut client, &memory, NoticeKind::Unlocked);
    assert_eq!(unlocked.len(), 1, "{unlocked:?}");
    assert_eq!((unlocked[0].kind, unlocked[0].title.as_str(), unlocked[0].detail.as_str()), (NoticeKind::Unlocked, "Seven", "RAM holds seven · 5 points"));
    assert_eq!(unlocked[0].badge.as_deref(), Some(stand_in::badge("101").as_slice()));
    assert_eq!(requests(&log, "badge")[0]["id"], "101");
    pump(&mut client, &memory, |_| !requests(&log, "awardachievement").is_empty());
    let award = &requests(&log, "awardachievement")[0];
    assert_eq!((award["a"].as_str(), award["h"].as_str()), ("101", "0"), "a softcore unlock");

    // A state loaded starts the evaluation again: the scratchpad's
    // achievement must be seen false before it unlocks.
    memory.scratchpad[0x10] = 9;
    client.timeline(false);
    client.frame(&memory);
    assert!(client.take_notices().is_empty(), "true from the start: not yet");
    memory.scratchpad[0x10] = 0;
    client.frame(&memory);
    memory.scratchpad[0x10] = 9;
    client.frame(&memory);
    let unlocked = notices_until(&mut client, &memory, NoticeKind::Unlocked);
    assert_eq!(unlocked.iter().map(|n| n.title.as_str()).collect::<Vec<_>>(), ["Scratchpad"], "{unlocked:?}");
    assert_eq!(unlocked[0].badge.as_deref(), Some(stand_in::badge("102").as_slice()), "its own badge");
    pump(&mut client, &memory, |_| requests(&log, "awardachievement").len() == 2);
    let award = &requests(&log, "awardachievement")[1];
    assert_eq!((award["a"].as_str(), award["h"].as_str()), ("102", "0"), "a softcore unlock from the scratchpad");

    // Signing out forgets the account.
    client.sign_out();
    assert!(!account.exists());
    assert_eq!(client.status().account, Account::SignedOut);
    std::fs::remove_dir_all(directory).unwrap();
}

#[test]
fn a_kept_account_signs_in_at_start_and_loads_the_games_achievements() {
    let (host, log) = server();
    let directory = directory("kept");
    std::fs::write(directory.join(xem_achievements::ACCOUNT_FILE), r#"{"username":"Player","token":"TOKEN"}"#).unwrap();
    let mut client = Achievements::new(Some(&directory), "XemTest/1.0");
    client.set_host(&host);
    let memory = Memory::new();
    assert_eq!(client.status().account, Account::SigningIn, "a kept account signs in");
    client.boot(Ok(HASH.into()));
    client.frame(&memory);
    pump(&mut client, &memory, |c| c.status().game.starts_with("Xenogears:"));
    assert_eq!(requests(&log, "login2")[0]["t"], "TOKEN");
    // A disc RetroAchievements does not know.
    client.boot(Ok("00000000000000000000000000000000".into()));
    pump(&mut client, &memory, |c| c.status().game != "Loading the game's achievements…");
    assert_eq!(client.status().game, "RetroAchievements knows no game by this disc.");
    std::fs::remove_dir_all(directory).unwrap();
}

#[test]
fn an_unlock_whose_badge_cannot_be_fetched_shows_without_one() {
    // The badges' server is unreachable: a port nothing listens on.
    let closed = std::net::TcpListener::bind("127.0.0.1:0").unwrap().local_addr().unwrap();
    let game = stand_in::game(11248, "Xenogears", &[(101, "Seven", "RAM holds seven", 5, "0xH001000=7")])
        .replace(stand_in::BADGES, &format!("http://{closed}/Badge"));
    let (host, _log) = stand_in::server(vec![(HASH, game)]);
    let directory = directory("badgeless");
    let mut client = Achievements::new(Some(&directory), "XemTest/1.0");
    client.set_host(&host);
    let mut memory = Memory::new();
    client.sign_in("Player", "secret").unwrap();
    client.boot(Ok(HASH.into()));
    pump(&mut client, &memory, |c| c.status().game.starts_with("Xenogears:"));
    client.frame(&memory);
    memory.ram[0x1000] = 7;
    client.frame(&memory);
    let asked = Instant::now();
    let notices = notices_until(&mut client, &memory, NoticeKind::Unlocked);
    let unlocked = notices.iter().find(|n| n.kind == NoticeKind::Unlocked).unwrap();
    assert_eq!((unlocked.title.as_str(), unlocked.badge.as_deref()), ("Seven", None));
    assert!(asked.elapsed() < Duration::from_secs(2), "shown once the badge failed, not after the longest wait: {:?}", asked.elapsed());
    std::fs::remove_dir_all(directory).unwrap();
}

#[test]
fn a_kept_token_the_server_refuses_is_forgotten() {
    let (host, _log) = server();
    let directory = directory("refused");
    let account = directory.join(xem_achievements::ACCOUNT_FILE);
    std::fs::write(&account, r#"{"username":"Player","token":"STALE"}"#).unwrap();
    let mut client = Achievements::new(Some(&directory), "XemTest/1.0");
    client.set_host(&host);
    let memory = Memory::new();
    pump(&mut client, &memory, |c| c.status().account == Account::SignedOut);
    assert!(client.status().error.as_deref().unwrap().contains("Invalid User/Token"), "{:?}", client.status());
    assert!(!account.exists(), "signed out until the player signs in again");
    std::fs::remove_dir_all(directory).unwrap();
}
