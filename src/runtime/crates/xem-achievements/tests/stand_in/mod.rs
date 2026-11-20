//! A stand-in RetroAchievements server for tests (plain HTTP on this
//! machine), answering as rcheevos' own tests have the server answer: the
//! account `Player` with the password `secret` (its token `TOKEN`), the
//! games it is given by their discs' hashes, sessions and unlocks, and the
//! achievements' badges (`GET /Badge/<id>.png`, answered with [`badge`]).
//! Every request's form is logged (a badge's as `r` `badge` and its `id`).
//! xem-host's tests take it too (by path).
#![allow(dead_code)]

use std::collections::BTreeMap;
use std::io::{BufRead, BufReader, Read, Write};
use std::net::TcpListener;
use std::sync::{Arc, Mutex};

/// The requests the server took: each one's form by field, with its
/// `user-agent`.
pub type Log = Arc<Mutex<Vec<BTreeMap<String, String>>>>;

/// Where the achievements' badges are, in a [`game`] answer: the server
/// puts its own address there (a test may put another first).
pub const BADGES: &str = "{badges}";

/// The image the server gives as achievement `id`'s badge (not a PNG: the
/// client keeps what it is given).
pub fn badge(id: &str) -> Vec<u8> {
    format!("badge {id}").into_bytes()
}

/// A game's `achievementsets` answer: `achievements` as
/// `(id, title, description, points, condition)`, each with its badge.
pub fn game(id: u32, title: &str, achievements: &[(u32, &str, &str, u32, &str)]) -> String {
    let list: Vec<String> = achievements.iter().map(|(id, title, description, points, condition)| format!(
        r#"{{"ID":{id},"Title":"{title}","Description":"{description}","Flags":3,"Points":{points},"MemAddr":"{condition}","Author":"Tester","BadgeName":"{id:05}","BadgeURL":"{BADGES}/{id}.png","Created":1700000000,"Modified":1700000000}}"#
    )).collect();
    format!(r#"{{"Success":true,"GameId":{id},"Title":"{title}","ConsoleId":12,"ImageIconUrl":"http://server/Images/000001.png",
        "RichPresenceGameId":{id},"RichPresencePatch":"","Sets":[{{"AchievementSetId":1,"GameId":{id},"Title":null,"Type":"core",
        "ImageIconUrl":"http://server/Images/000001.png","Achievements":[{}],"Leaderboards":[]}}]}}"#, list.join(","))
}

fn decode(text: &str) -> String {
    let bytes = text.as_bytes();
    let mut out = Vec::new();
    let mut at = 0;
    while at < bytes.len() {
        match bytes[at] {
            b'+' => { out.push(b' '); at += 1; }
            b'%' if at + 2 < bytes.len() => {
                out.push(u8::from_str_radix(&text[at + 1..at + 3], 16).unwrap());
                at += 3;
            }
            byte => { out.push(byte); at += 1; }
        }
    }
    String::from_utf8(out).unwrap()
}

/// The server at `http://127.0.0.1:<port>`, knowing `games` (a disc's hash
/// and its game's [`game`] answer).
pub fn server(games: Vec<(&'static str, String)>) -> (String, Log) {
    let listener = TcpListener::bind("127.0.0.1:0").unwrap();
    let address = format!("http://{}", listener.local_addr().unwrap());
    let log: Log = Arc::default();
    let requests = log.clone();
    let badges = format!("{address}/Badge");
    std::thread::spawn(move || {
        for stream in listener.incoming() {
            let Ok(stream) = stream else { continue };
            let mut reader = BufReader::new(stream);
            let mut length = 0;
            let mut user_agent = String::new();
            let mut request_line = String::new();
            if reader.read_line(&mut request_line).unwrap_or(0) == 0 {
                continue;
            }
            loop {
                let mut line = String::new();
                if reader.read_line(&mut line).unwrap_or(0) == 0 || line == "\r\n" {
                    break;
                }
                let lower = line.to_ascii_lowercase();
                if let Some(value) = lower.strip_prefix("content-length:") {
                    length = value.trim().parse().unwrap_or(0);
                }
                if lower.starts_with("user-agent:") {
                    user_agent = line["user-agent:".len()..].trim().to_string();
                }
            }
            if let Some(id) = request_line.strip_prefix("GET /Badge/").and_then(|rest| rest.split_once(".png ")).map(|(id, _)| id.to_string()) {
                let image = badge(&id);
                requests.lock().unwrap().push(BTreeMap::from([("r".into(), "badge".into()), ("id".into(), id), ("user-agent".into(), user_agent)]));
                let mut stream = reader.into_inner();
                let _ = write!(stream, "HTTP/1.1 200 X\r\nContent-Type: image/png\r\nContent-Length: {}\r\nConnection: close\r\n\r\n", image.len());
                let _ = stream.write_all(&image);
                continue;
            }
            let mut body = vec![0; length];
            if reader.read_exact(&mut body).is_err() {
                continue;
            }
            let mut form: BTreeMap<String, String> = String::from_utf8(body).unwrap().split('&').filter_map(|pair| pair.split_once('='))
                .map(|(key, value)| (key.to_string(), decode(value))).collect();
            form.insert("user-agent".into(), user_agent);
            let (status, answer) = match form.get("r").map(String::as_str) {
                Some("login2") => match (form.get("p").map(String::as_str), form.get("t").map(String::as_str)) {
                    (Some("secret"), _) | (_, Some("TOKEN")) => (200, r#"{"Success":true,"User":"Player","Token":"TOKEN","Score":1234,"SoftcoreScore":56,"Messages":0,"Permissions":1,"AccountType":"Registered"}"#.to_string()),
                    (Some(_), _) => (401, r#"{"Success":false,"Error":"Invalid User/Password combination. Please try again","Status":401,"Code":"invalid_credentials"}"#.to_string()),
                    _ => (401, r#"{"Success":false,"Error":"Invalid User/Token combination.","Status":401,"Code":"invalid_credentials"}"#.to_string()),
                },
                Some("achievementsets") => match games.iter().find(|(hash, _)| form.get("m").map(String::as_str) == Some(*hash)) {
                    Some((_, answer)) => (200, answer.replace(BADGES, &badges)),
                    None => (404, r#"{"Success":false,"Error":"Unknown game","Code":"not_found","Status":404}"#.to_string()),
                },
                Some("startsession") => (200, r#"{"Success":true,"ServerNow":1700000000}"#.to_string()),
                Some("awardachievement") => (200, format!(r#"{{"Success":true,"Score":1239,"SoftcoreScore":56,"AchievementID":{},"AchievementsRemaining":1}}"#, form["a"])),
                _ => (200, r#"{"Success":true}"#.to_string()),
            };
            requests.lock().unwrap().push(form);
            let mut stream = reader.into_inner();
            let _ = write!(stream, "HTTP/1.1 {status} X\r\nContent-Type: application/json\r\nContent-Length: {}\r\nConnection: close\r\n\r\n{answer}", answer.len());
        }
    });
    (address, log)
}

/// The logged requests of kind `kind` (their `r`).
pub fn requests(log: &Log, kind: &str) -> Vec<BTreeMap<String, String>> {
    log.lock().unwrap().iter().filter(|form| form.get("r").map(String::as_str) == Some(kind)).cloned().collect()
}
