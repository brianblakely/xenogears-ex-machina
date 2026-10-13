//! The gesture recognizers' measures under tracking noise
//! (`xem_assist::reliability`): `cargo run -p xem-assist --example reliability`.
use xem_assist::gesture::{Gesture, Hand};
use xem_assist::reliability::{self, Noise, Setup};

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
    for alone in [Hand::Right, Hand::Left] {
        print!("relaxed posture, {alone:?} hand alone:");
        for (i, name) in reliability::GESTURES.into_iter().filter(|n| *n != "prayer").enumerate() {
            let m = reliability::measure_with(name, 1000, &noise, 201 + i as u64, &Setup::relaxed(Some(alone)));
            print!(" {name} {:.1}%", 100.0 * m.recognised as f32 / m.tries as f32);
        }
        println!();
    }
    println!("an hour's session, tiring (the hands taking turns alone):");
    println!("{:>6} {:>7} {:>7} {:>6} {:>10} {:>6} {:>8} {:>13}", "minute", "tremor", "slowest", "droop", "recognised", "wrong", "latency", "idle fired/min");
    for s in reliability::sustained(6, 10.0, 100, 301) {
        println!("{:>6.0} {:>5.1}mm {:>7} {:>5.0}° {:>9.1}% {:>5.1}% {:>5.2} fr {:>13.2}", s.from, s.noise.1 * 1000.0, s.noise.2, s.droop,
            100.0 * s.recognised as f32 / s.tries as f32, 100.0 * s.wrong as f32 / s.tries as f32, s.latency, s.idle_activations as f32 / 10.0);
    }
    for jitter in [0.003, 0.004] {
        let noisy = Noise { jitter, ..noise };
        print!("per-joint jitter {:.0} mm (a noisier tracker):", jitter * 1000.0);
        for (i, name) in reliability::GESTURES.into_iter().enumerate() {
            let m = reliability::measure(name, 1000, &noisy, 401 + i as u64);
            print!(" {name} {:.1}%", 100.0 * m.recognised as f32 / m.tries as f32);
        }
        println!(" idle {:.2}/min", reliability::false_activations(10.0, &noisy, 9).len() as f32 / 10.0);
    }
}
