//! Checks against the user's own discs, skipped when they are absent.
//!
//! XEM_DISC1 / XEM_DISC2: the CHDs (default `discs/Xenogears disc N.chd` in
//! the repository). XEM_DISC1_CUE / XEM_DISC2_CUE: the raw tracks `chdman
//! extractcd` wrote (default `.local/discs/discN.cue`). XEM_EXTRACT1 /
//! XEM_EXTRACT2: tools/extraction/disc_files.py output (default
//! `.local/extract/discN`), whose manifest the index is checked against. Nothing
//! here writes or keeps disc bytes.

use std::cell::RefCell;
use std::path::{Path, PathBuf};
use std::rc::Rc;

use xem_disc::identify::hex;
use xem_disc::sector::Form;
use xem_disc::{
    ChdSource, Error, FileReadAt, Index, KnownDisc, PrefetchedFile, ReadAt, SECTOR_SIZE, Sector,
    SectorSource, file_sha256, game_index, identify, open_path, read_data,
};

fn repo() -> PathBuf {
    Path::new(env!("CARGO_MANIFEST_DIR")).join("../../..")
}

fn input(var: &str, default: PathBuf) -> Option<PathBuf> {
    let path = std::env::var_os(var).map(PathBuf::from).unwrap_or(default);
    if path.exists() {
        Some(path)
    } else {
        eprintln!("skipping: {} not present (set {var})", path.display());
        None
    }
}

fn chd(n: u8) -> Option<PathBuf> {
    input(
        &format!("XEM_DISC{n}"),
        repo().join(format!("discs/Xenogears disc {n}.chd")),
    )
}

/// The extractor's manifest: (boot name, lba, size, sha256) and its records.
struct Manifest {
    boot: (String, u32, usize, String),
    files: Vec<(usize, u32, i32, Option<String>)>,
}

/// A line-oriented read of manifest.json as disc_files.py writes it.
fn manifest(dir: &Path) -> Manifest {
    let text = std::fs::read_to_string(dir.join("manifest.json")).expect("manifest");
    let mut fields: Vec<(String, String)> = Vec::new();
    for line in text.lines() {
        if let Some((key, value)) = line.trim().trim_end_matches(',').split_once(": ") {
            fields.push((
                key.trim_matches('"').to_string(),
                value.trim_matches('"').to_string(),
            ));
        }
    }
    let get = |at: usize, key: &str| {
        assert_eq!(fields[at].0, key);
        fields[at].1.clone()
    };
    let boot = (
        get(1, "name"),
        get(2, "lba").parse().unwrap(),
        get(3, "size").parse().unwrap(),
        get(4, "sha256"),
    );
    let mut files = Vec::new();
    let mut at = 6;
    while at < fields.len() {
        let slot = get(at, "slot").parse().unwrap();
        let lba = get(at + 1, "lba").parse().unwrap();
        let size: i32 = get(at + 2, "size").parse().unwrap();
        let sha = (size > 0).then(|| get(at + 3, "sha256"));
        at += if size > 0 { 4 } else { 3 };
        files.push((slot, lba, size, sha));
    }
    Manifest { boot, files }
}

fn check_disc(n: u8, known: KnownDisc) {
    let Some(path) = chd(n) else { return };
    let mut source = open_path(&path).expect("open CHD");
    let id = identify(&mut source).expect("identify");
    assert_eq!(id.disc, Some(known));
    assert_eq!(id.sha256, known.executable_sha256());
    assert_eq!(
        id.boot_path,
        format!("SLUS_006.{};1", if n == 1 { 64 } else { 69 })
    );

    // The embedded index is the one on disc (all whole records; the last,
    // partial record's byte is where the two differ) and names this disc.
    let index = game_index(&id.executable).expect("embedded index");
    let on_disc = Index::from_disc(&mut source).expect("index on disc");
    assert_eq!(index.records(), on_disc.records());
    assert_eq!(
        &index.directory_table()[..on_disc.directory_table().len()],
        on_disc.directory_table()
    );
    assert_eq!(index.disc_number(), Some(u16::from(n)));
    // The mode overlays: field is file 0xE of directory (0, 1), the battle
    // overlay file 0x10 (docs/later-phases.md, Images and their numbers).
    let field_slot = if n == 1 { 36 } else { 31 };
    assert_eq!(index.slot(1, 0xE), Some(field_slot));
    assert_eq!(index.slot(1, 0x10), Some(field_slot + 2));

    let Some(extract) = input(
        &format!("XEM_EXTRACT{n}"),
        repo().join(format!(".local/extract/disc{n}")),
    ) else {
        return;
    };
    let manifest = manifest(&extract);
    assert_eq!(
        manifest.boot,
        (
            id.boot_path.trim_end_matches(";1").to_string(),
            id.boot_lba,
            id.executable.len(),
            hex(&id.sha256)
        )
    );
    let ours: Vec<(usize, u32, i32)> = index
        .entries()
        .map(|(slot, r)| (slot, r.lba, r.size))
        .collect();
    let theirs: Vec<(usize, u32, i32)> = manifest.files.iter().map(|f| (f.0, f.1, f.2)).collect();
    assert_eq!(ours, theirs);

    // Sector reads against the extracted files and their digests: a spread
    // of files, the overlays among them.
    let files: Vec<_> = manifest.files.iter().filter(|f| f.2 > 0).collect();
    let mut checked = 0;
    for (k, (slot, lba, size, sha)) in files.iter().enumerate() {
        if k % 61 != 0 && *slot != field_slot {
            continue;
        }
        if *size > 4 << 20 {
            continue; // keep the test quick; large files are movies and archives
        }
        let data = read_data(&mut source, *lba, *size as usize).expect("file data");
        let extracted =
            std::fs::read(extract.join(format!("files/{slot:04}.bin"))).expect("extracted file");
        assert!(
            data == extracted,
            "slot {slot} differs from its extracted copy"
        );
        let record = index.record(*slot).unwrap();
        assert_eq!(Some(hex(&file_sha256(&mut source, record).unwrap())), *sha);
        checked += 1;
    }
    assert!(checked > 40, "only {checked} files compared");

    check_against_raw_track(n, &mut source, &index, &id);
    check_prefetched(&path, &mut source);
}

/// The CHD's sectors are the raw track's, Form 1 and Form 2 alike.
fn check_against_raw_track(
    n: u8,
    chd: &mut dyn SectorSource,
    index: &Index,
    id: &xem_disc::Identification,
) {
    let Some(cue) = input(
        &format!("XEM_DISC{n}_CUE"),
        repo().join(format!(".local/discs/disc{n}.cue")),
    ) else {
        return;
    };
    let mut bin = open_path(&cue).expect("open cue");
    assert_eq!(bin.sector_count(), chd.sector_count());
    let last = chd.sector_count() - 1;
    // The largest file is a movie: its sectors interleave Form 2 records.
    let (_, movie) = index.entries().max_by_key(|(_, r)| r.size).unwrap();
    let mut lbas = vec![0, 16, 24, 39, 40, id.boot_lba, last];
    lbas.extend(movie.lba..movie.lba + 64);
    let (mut a, mut b) = ([0u8; SECTOR_SIZE], [0u8; SECTOR_SIZE]);
    let mut form2 = 0;
    for lba in lbas {
        chd.read_sector(lba, &mut a).unwrap();
        bin.read_sector(lba, &mut b).unwrap();
        assert!(
            a == b,
            "sector {lba} differs between the CHD and the raw track"
        );
        let sector = Sector(&a);
        assert!(sector.has_sync());
        assert_eq!(sector.header().lba(), Some(lba));
        if sector.form() == Some(Form::Form2) {
            form2 += 1;
            assert_eq!(sector.user_data().unwrap().len(), 2324);
        }
    }
    assert!(form2 > 0, "no Form 2 sector in the movie's first 64");
    let mut range = vec![0u8; 20 * SECTOR_SIZE];
    let mut single = vec![0u8; 20 * SECTOR_SIZE];
    bin.read_sectors(movie.lba, &mut range).unwrap();
    chd.read_sectors(movie.lba, &mut single).unwrap();
    assert!(range == single);
    assert!(matches!(
        chd.read_sector(last + 1, &mut a),
        Err(Error::OutOfRange { .. })
    ));
}

/// The browser design over the real CHD: a chunk store filled on demand.
fn check_prefetched(path: &Path, native: &mut dyn SectorSource) {
    let mut file = FileReadAt::open(path).unwrap();
    let store = Rc::new(RefCell::new(PrefetchedFile::new(
        file.len(),
        64 << 10,
        2 << 20,
    )));
    let mut round_trips = 0;
    let mut fetch = |store: &Rc<RefCell<PrefetchedFile>>| {
        let missing = store.borrow_mut().take_missing();
        assert!(!missing.is_empty(), "not ready without a missing range");
        for range in missing {
            let mut bytes = vec![0u8; (range.end - range.start) as usize];
            file.read_at(range.start, &mut bytes).unwrap();
            store.borrow_mut().fill(range.start, &bytes).unwrap();
        }
        round_trips += 1;
    };
    let mut source = loop {
        match ChdSource::open(store.clone(), 1 << 20) {
            Ok(source) => break source,
            Err(Error::NotReady) => fetch(&store),
            Err(err) => panic!("open: {err}"),
        }
    };
    let (mut a, mut b) = ([0u8; SECTOR_SIZE], [0u8; SECTOR_SIZE]);
    for lba in [16, 24, 100_000, 200_000, source.sector_count() - 1] {
        loop {
            match source.read_sector(lba, &mut a) {
                Ok(()) => break,
                Err(Error::NotReady) => fetch(&store),
                Err(err) => panic!("read {lba}: {err}"),
            }
        }
        native.read_sector(lba, &mut b).unwrap();
        assert!(a == b, "sector {lba} differs through the chunk store");
    }
    assert!(store.borrow().resident_bytes() <= 2 << 20);
    assert!(round_trips < 40, "{round_trips} round trips");
}

#[test]
fn disc1() {
    check_disc(1, KnownDisc::Disc1);
}

#[test]
fn disc2() {
    check_disc(2, KnownDisc::Disc2);
}
