//! RetroAchievements' hash of each disc image named (a CHD, a cue sheet or
//! a raw track), as the hosts load its achievements by:
//!
//!     cargo run -p xem-achievements --example disc_hash -- "discs/Xenogears disc 1.chd"

fn main() {
    for path in std::env::args().skip(1) {
        let hash = xem_disc::open_path(std::path::Path::new(&path)).map_err(|error| error.to_string())
            .and_then(|mut source| xem_achievements::disc_hash(source.as_mut()));
        match hash {
            Ok(hash) => println!("{hash}  {path}"),
            Err(error) => println!("error: {error}  {path}"),
        }
    }
}
