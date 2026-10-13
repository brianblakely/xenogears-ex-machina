//! The gesture recognizers' measures under tracking noise
//! (`xem_assist::reliability`): `cargo run -p xem-assist --example reliability`.
use xem_assist::gesture::Gesture;
use xem_assist::reliability::{self, Noise};

fn main() {
    let noise = Noise::default();
    println!("noise: {noise:?}");
    println!("{:<12} {:>6} {:>10} {:>7} {:>6} {:>8}", "gesture", "tries", "recognised", "missed", "wrong", "latency");
    for (i, name) in reliability::GESTURES.into_iter().enumerate() {
        let m = reliability::measure(name, 1000, &noise, 101 + i as u64);
        println!("{:<12} {:>6} {:>9.1}% {:>6.1}% {:>5.1}% {:>5.2} fr", m.name, m.tries,
            100.0 * m.recognised as f32 / m.tries as f32, 100.0 * m.missed as f32 / m.tries as f32,
            100.0 * m.wrong as f32 / m.tries as f32, m.latency);
    }
    let minutes = 30.0;
    let fired = reliability::false_activations(minutes, &noise, 7);
    let toggles = fired.iter().filter(|g| matches!(g, Gesture::Pause { .. } | Gesture::Inspect { .. } | Gesture::Settings | Gesture::ScaleBegin)).count();
    println!("idle play: {} activations in {minutes} minutes ({:.2} a minute), {toggles} of them toggles", fired.len(), fired.len() as f32 / minutes);
    println!("rapid pause/resume: {} toggles of 200 grabs a twelfth of a second apart", reliability::rapid_pauses(200, 6, &noise, 3));
    println!("tracking lost mid-grab, found closed: {} toggles of 200", reliability::reacquired_fists(200, &noise, 5));
}
