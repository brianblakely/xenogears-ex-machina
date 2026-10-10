//! Synthetic GP0/GP1/DMA streams with the VRAM they must produce.

use xem_gpu::{DmaError, Gpu, Memory, MemoryMut, PrimitiveKind, SnapshotError};

fn xy(x: i32, y: i32) -> u32 {
    (x as u32 & 0xFFFF) | (y as u32 & 0xFFFF) << 16
}

fn rgb(r: u32, g: u32, b: u32) -> u32 {
    r | g << 8 | b << 16
}

fn rgb15(r: u16, g: u16, b: u16) -> u16 {
    r | g << 5 | b << 10
}

fn pixel(gpu: &Gpu, x: usize, y: usize) -> u16 {
    gpu.vram()[y * 1024 + x]
}

/// A GPU with the whole VRAM as drawing area.
fn fresh() -> Gpu {
    let mut gpu = Gpu::new();
    gpu.write_gp0_words(&[0xE300_0000, 0xE400_0000 | 1023 | 511 << 10]);
    gpu
}

fn upload(gpu: &mut Gpu, x: i32, y: i32, width: i32, height: i32, pixels: &[u16]) {
    assert_eq!(pixels.len(), (width * height) as usize);
    gpu.write_gp0_words(&[0xA000_0000, xy(x, y), xy(width, height)]);
    for pair in pixels.chunks(2) {
        let high = pair.get(1).copied().unwrap_or(0);
        gpu.write_gp0(u32::from(pair[0]) | u32::from(high) << 16);
    }
}

fn drawn(gpu: &Gpu) -> Vec<(usize, usize)> {
    let mut out = Vec::new();
    for (index, &value) in gpu.vram().iter().enumerate() {
        if value != 0 {
            out.push((index % 1024, index / 1024));
        }
    }
    out
}

#[test]
fn reset_status_and_info() {
    let mut gpu = Gpu::new();
    assert_eq!(gpu.gpustat(), 0x1480_2000);
    gpu.write_gp0_words(&[0xE2012345, 0xE3001234, 0xE4054321, 0xE5123456]);
    for (index, expected) in [
        (2, 0x12345),
        (3, 0x01234),
        (4, 0x54321),
        (5, 0x123456),
        (7, 2),
    ] {
        gpu.write_gp1(0x1000_0000 | index);
        assert_eq!(gpu.read_gpuread(), expected, "info {index}");
    }
    gpu.write_gp1(0x1000_0000); // index 0 leaves GPUREAD alone
    assert_eq!(gpu.read_gpuread(), 2);
    gpu.write_gp1(0x0300_0000);
    gpu.write_gp1(0x0400_0002);
    gpu.write_gp1(0x0800_0001);
    gpu.write_gp0_words(&[0xE100_0215, 0xE600_0003, 0x1F00_0000]);
    let status = gpu.gpustat();
    assert_eq!(status & 0x7FF, 0x215);
    assert_eq!(status >> 11 & 3, 3);
    assert_eq!(status >> 17 & 3, 1);
    assert_eq!(status >> 23 & 1, 0, "display enabled");
    assert_eq!(status >> 24 & 1, 1, "interrupt requested");
    assert_eq!(status >> 25 & 1, 1, "CPU-to-GP0 DMA ready");
    assert_eq!(status >> 29 & 3, 2);
    gpu.write_gp1(0x0200_0000);
    assert!(!gpu.irq_pending());
    gpu.write_gp1(0);
    assert_eq!(gpu.gpustat(), 0x1480_2000);
}

#[test]
fn interlaced_field_toggles_on_vblank() {
    let mut gpu = Gpu::new();
    gpu.write_gp1(0x0800_0024);
    let field = |gpu: &Gpu| (gpu.gpustat() >> 13 & 1, gpu.gpustat() >> 31);
    assert_eq!(field(&gpu), (0, 0));
    gpu.vblank();
    assert_eq!(field(&gpu), (1, 1));
    gpu.vblank();
    assert_eq!(field(&gpu), (0, 0));
}

#[test]
fn fill_rounds_x_and_width_and_ignores_area_and_mask() {
    let mut gpu = Gpu::new(); // drawing area 0,0-0,0
    gpu.write_gp0(0xE600_0003);
    gpu.write_gp0_words(&[0x0200_0000 | rgb(0xFF, 0x80, 0x08), xy(17, 3), xy(20, 2)]);
    let color = rgb15(31, 16, 1);
    for y in 0..8 {
        for x in 0..64 {
            let expected = if (16..48).contains(&x) && (3..5).contains(&y) {
                color
            } else {
                0
            };
            assert_eq!(pixel(&gpu, x, y), expected, "({x}, {y})");
        }
    }
    // Wraps at the right and bottom edges.
    gpu.write_gp0_words(&[0x0200_0000 | rgb(8, 8, 8), xy(1008, 511), xy(32, 2)]);
    for (x, y) in [
        (1008, 511),
        (1023, 511),
        (0, 511),
        (15, 511),
        (0, 0),
        (15, 0),
    ] {
        assert_eq!(pixel(&gpu, x, y), rgb15(1, 1, 1), "({x}, {y})");
    }
    assert_eq!(pixel(&gpu, 16, 0), 0);
}

#[test]
fn flat_triangle_follows_the_top_left_rule() {
    for (b, c) in [((8, 0), (0, 8)), ((0, 8), (8, 0))] {
        let mut gpu = fresh();
        gpu.write_gp0_words(&[0x2000_00F8, xy(0, 0), xy(b.0, b.1), xy(c.0, c.1)]);
        let mut expected = Vec::new();
        for y in 0..8 {
            for x in 0..8 {
                if x + y <= 7 {
                    expected.push((x, y));
                }
            }
        }
        expected.sort_by_key(|&(x, y)| (y, x));
        assert_eq!(drawn(&gpu), expected);
        assert_eq!(pixel(&gpu, 0, 0), rgb15(31, 0, 0));
    }
}

#[test]
fn quad_halves_do_not_overlap_and_skip_last_row_and_column() {
    let mut gpu = fresh();
    gpu.write_gp0_words(&[0x0200_0000 | rgb(80, 0, 0), 0, xy(32, 32)]);
    gpu.write_gp0(0xE100_0020); // semi-transparency B+F
    gpu.write_gp0_words(&[0x2A00_0040, xy(0, 0), xy(10, 0), xy(0, 10), xy(10, 10)]);
    for y in 0..32 {
        for x in 0..32 {
            let expected = if x < 10 && y < 10 { 18 } else { 10 };
            assert_eq!(pixel(&gpu, x, y), expected, "({x}, {y})");
        }
    }
}

#[test]
fn oversized_triangles_and_lines_are_rejected() {
    let mut gpu = fresh();
    gpu.write_gp0_words(&[0x2000_00F8, xy(0, 0), xy(1024, 0), xy(0, 10)]);
    gpu.write_gp0_words(&[0x2000_00F8, xy(0, 0), xy(10, 0), xy(0, 512)]);
    gpu.write_gp0_words(&[0x4000_00F8, xy(0, 0), xy(1024, 1)]);
    assert!(drawn(&gpu).is_empty());
    assert_eq!(gpu.stats().rejected(), 3);
    gpu.write_gp0_words(&[0x2000_00F8, xy(0, 0), xy(1023, 0), xy(0, 511)]);
    assert_eq!(pixel(&gpu, 1000, 0), rgb15(31, 0, 0));
    assert_eq!(gpu.stats().rejected(), 3);
}

#[test]
fn vertices_are_signed_11_bit_plus_offset() {
    let mut gpu = fresh();
    gpu.write_gp0(0xE500_0000 | 100 | 50 << 11);
    // X = 7FFh (-1), Y = 7FEh (-2); bits 11-15 are ignored.
    gpu.write_gp0_words(&[0x6800_00F8, 0xAFFE_AFFF]);
    assert_eq!(drawn(&gpu), vec![(99, 48)]);
    // A negative offset.
    gpu.write_gp0(0xE500_0000 | (-10i32 as u32 & 0x7FF) | (-20i32 as u32 & 0x7FF) << 11);
    gpu.write_gp0_words(&[0x6800_00F8, xy(30, 40)]);
    assert_eq!(pixel(&gpu, 20, 20), rgb15(31, 0, 0));
}

#[test]
fn drawing_area_clips_inclusively() {
    let mut gpu = fresh();
    gpu.write_gp0_words(&[0xE300_0000 | 10 | 20 << 10, 0xE400_0000 | 19 | 29 << 10]);
    gpu.write_gp0_words(&[0x6000_00F8, xy(0, 0), xy(100, 100)]);
    gpu.write_gp0_words(&[0x2000_00F8, xy(0, 0), xy(100, 0), xy(0, 100)]);
    gpu.write_gp0_words(&[0x4000_00F8, xy(0, 25), xy(100, 25)]);
    let points = drawn(&gpu);
    assert_eq!(points.len(), 100);
    assert!(
        points
            .iter()
            .all(|&(x, y)| (10..=19).contains(&x) && (20..=29).contains(&y))
    );
}

/// CLUT at (0, 480): entry 0 transparent, entry i grey i, entry 5 also
/// semi-transparent (bit 15).
fn clut4() -> Vec<u16> {
    (0..16)
        .map(|i| match i {
            0 => 0,
            5 => rgb15(5, 5, 5) | 0x8000,
            _ => rgb15(i, i, i),
        })
        .collect()
}

#[test]
fn textured_sprite_with_4bit_clut_and_flips() {
    let mut gpu = fresh();
    upload(&mut gpu, 0, 480, 16, 1, &clut4());
    // 8x8 texels at page (64, 0): texel (u, v) = (u + v) & 15, four per halfword.
    let index = |u: u16, v: u16| (u + v) & 15;
    let mut texture = Vec::new();
    for v in 0..8 {
        for half in 0..2 {
            texture.push((0..4).fold(0, |word, j| word | index(half * 4 + j, v) << (4 * j)));
        }
    }
    upload(&mut gpu, 64, 0, 2, 8, &texture);
    let clut = 480 << 6;
    let draw = |gpu: &mut Gpu, mode: u32, u0: u32| {
        gpu.write_gp0(0xE100_0001 | mode);
        gpu.write_gp0_words(&[0x6500_0000, xy(100, 100), clut << 16 | u0, xy(8, 8)]);
    };
    let check = |gpu: &Gpu, texel_u: &dyn Fn(u16) -> u16| {
        for v in 0..8u16 {
            for du in 0..8u16 {
                let i = index(texel_u(du), v);
                let expected = clut4()[i as usize];
                assert_eq!(
                    pixel(gpu, 100 + du as usize, 100 + v as usize),
                    expected,
                    "({du}, {v})"
                );
            }
        }
    };
    draw(&mut gpu, 0, 0);
    check(&gpu, &|du| du);
    assert_eq!(pixel(&gpu, 100, 100), 0, "index 0 is transparent");
    assert_eq!(pixel(&gpu, 105, 100) & 0x8000, 0x8000, "texel bit 15 kept");
    // X flip: U decreases from U0.
    let mut gpu2 = gpu.clone();
    for row in 100..108 {
        gpu2.vram_mut()[row * 1024 + 100..row * 1024 + 108].fill(0);
    }
    draw(&mut gpu2, 0x1000, 7);
    check(&gpu2, &|du| 7 - du);
    assert_eq!(gpu2.stats().count(PrimitiveKind::RectangleTextured), 2);
}

#[test]
fn textured_sprites_8bit_and_15bit() {
    let mut gpu = fresh();
    let clut: Vec<u16> = (0..256)
        .map(|i| if i == 0 { 0 } else { 0x7C00 | i })
        .collect();
    upload(&mut gpu, 0, 481, 256, 1, &clut);
    // 4x2 texels at page (128, 0), index 1 + u + 4v, two per halfword.
    let texture: Vec<u16> = (0..2)
        .flat_map(|v| (0..2).map(move |h| (1 + 2 * h + 4 * v) | (2 + 2 * h + 4 * v) << 8))
        .collect();
    upload(&mut gpu, 128, 0, 2, 2, &texture);
    gpu.write_gp0(0xE100_0002 | 1 << 7);
    gpu.write_gp0_words(&[0x6500_0000, xy(10, 10), 481 << 22, xy(4, 2)]);
    for v in 0..2 {
        for u in 0..4 {
            assert_eq!(pixel(&gpu, 10 + u, 10 + v), 0x7C00 | (1 + u + 4 * v) as u16);
        }
    }
    // 15-bit direct at page (192, 256).
    let direct: Vec<u16> = (0..16).map(|i| rgb15(i, 31 - i, 7)).collect();
    upload(&mut gpu, 192 + 3, 256 + 2, 16, 1, &direct);
    gpu.write_gp0(0xE100_0003 | 1 << 4 | 2 << 7);
    gpu.write_gp0_words(&[0x7D00_0000, xy(40, 40), 3 | 2 << 8]); // 16x16 sprite at U 3, V 2
    for (u, &texel) in direct.iter().enumerate() {
        assert_eq!(pixel(&gpu, 40 + u, 40), texel);
    }
    assert_eq!(pixel(&gpu, 40, 41), 0, "transparent row");
}

#[test]
fn textured_polygon_uses_its_texpage_and_updates_draw_mode() {
    let mut gpu = fresh();
    let direct: Vec<u16> = (0..16).map(|i| rgb15(i + 1, 0, 0)).collect();
    upload(&mut gpu, 320, 0, 16, 1, &direct);
    let page = 5 | 2 << 7; // (320, 0), 15-bit
    gpu.write_gp0_words(&[
        0x2D00_0000,
        xy(0, 0),
        0,
        xy(16, 0),
        page << 16 | 16,
        xy(0, 1),
        0x100,
        xy(16, 1),
        0x110,
    ]);
    for (x, &texel) in direct.iter().enumerate() {
        assert_eq!(pixel(&gpu, x, 0), texel);
    }
    assert_eq!(gpu.gpustat() & 0x1FF, page);
}

#[test]
fn modulation_multiplies_by_color_over_128() {
    let mut gpu = fresh();
    upload(&mut gpu, 192, 0, 1, 1, &[rgb15(16, 8, 31)]);
    gpu.write_gp0(0xE100_0003 | 2 << 7 | 1 << 9); // dither on: rectangles ignore it
    gpu.write_gp0_words(&[0x6C00_0000 | rgb(0x80, 0xFF, 0x40), xy(0, 0), 0]);
    assert_eq!(pixel(&gpu, 0, 0), rgb15(16, 15, 15));
    gpu.write_gp0_words(&[0x6D00_0000 | rgb(0x10, 0x10, 0x10), xy(1, 0), 0]); // raw
    assert_eq!(pixel(&gpu, 1, 0), rgb15(16, 8, 31));
}

#[test]
fn semi_transparency_modes() {
    let expected = [
        rgb15(12, 16, 23),
        rgb15(24, 31, 31),
        rgb15(8, 0, 0),
        rgb15(18, 20, 23),
    ];
    for (mode, &expected) in expected.iter().enumerate() {
        let mut gpu = fresh();
        gpu.write_gp0_words(&[0x0280_8080, 0, xy(16, 1)]);
        gpu.write_gp0(0xE100_0000 | (mode as u32) << 5);
        gpu.write_gp0_words(&[0x6A00_0000 | rgb(64, 128, 248), xy(0, 0)]);
        assert_eq!(pixel(&gpu, 0, 0), expected, "mode {mode}");
        // Opaque rectangle: no blending.
        gpu.write_gp0_words(&[0x6800_0000 | rgb(64, 128, 248), xy(1, 0)]);
        assert_eq!(pixel(&gpu, 1, 0), rgb15(8, 16, 31));
    }
    // Textured: only texels with bit 15 blend.
    let mut gpu = fresh();
    gpu.write_gp0_words(&[0x0280_8080, 0, xy(16, 1)]);
    upload(
        &mut gpu,
        192,
        0,
        2,
        1,
        &[rgb15(8, 8, 8), rgb15(8, 8, 8) | 0x8000],
    );
    gpu.write_gp0(0xE100_0003 | 1 << 5 | 2 << 7);
    gpu.write_gp0_words(&[0x6700_0000, xy(0, 0), 0, xy(2, 1)]);
    assert_eq!(pixel(&gpu, 0, 0), rgb15(8, 8, 8));
    assert_eq!(pixel(&gpu, 1, 0), rgb15(24, 24, 24) | 0x8000);
}

#[test]
fn mask_bit_set_and_check() {
    let mut gpu = fresh();
    gpu.write_gp0(0xE600_0001);
    gpu.write_gp0_words(&[0x6800_00F8, xy(0, 0)]);
    assert_eq!(pixel(&gpu, 0, 0), 0x8000 | 31);
    gpu.write_gp0(0xE600_0002);
    gpu.write_gp0_words(&[0x6800_F800, xy(0, 0)]);
    gpu.write_gp0_words(&[0x6800_F800, xy(1, 0)]);
    assert_eq!(pixel(&gpu, 0, 0), 0x8000 | 31, "masked pixel kept");
    assert_eq!(pixel(&gpu, 1, 0), rgb15(0, 31, 0));
    upload(&mut gpu, 0, 0, 2, 1, &[7, 7]);
    assert_eq!(
        pixel(&gpu, 0, 0),
        0x8000 | 31,
        "transfers check the mask too"
    );
    assert_eq!(pixel(&gpu, 1, 0), 7);
}

#[test]
fn vram_copy_wraps_and_applies_mask() {
    let mut gpu = fresh();
    let pattern: Vec<u16> = (1..=12).collect();
    upload(&mut gpu, 10, 10, 4, 3, &pattern);
    gpu.write_gp0_words(&[0x8000_0000, xy(10, 10), xy(500, 200), xy(4, 3)]);
    for (i, &value) in pattern.iter().enumerate() {
        assert_eq!(pixel(&gpu, 500 + i % 4, 200 + i / 4), value);
    }
    gpu.write_gp0(0xE600_0001);
    gpu.write_gp0_words(&[0x8000_0000, xy(10, 10), xy(1022, 511), xy(4, 3)]);
    assert_eq!(pixel(&gpu, 1022, 511), 0x8000 | 1);
    assert_eq!(pixel(&gpu, 1, 511), 0x8000 | 4);
    assert_eq!(pixel(&gpu, 0, 0), 0x8000 | 7);
    assert_eq!(pixel(&gpu, 1, 1), 0x8000 | 12);
    assert_eq!(gpu.stats().count(PrimitiveKind::VramCopy), 2);
}

#[test]
fn cpu_vram_round_trip() {
    let mut gpu = fresh();
    let pixels: Vec<u16> = (0..9).map(|i| 0x1111 * (i + 1)).collect();
    upload(&mut gpu, 1022, 510, 3, 3, &pixels);
    assert_eq!(pixel(&gpu, 1022, 510), pixels[0]);
    assert_eq!(pixel(&gpu, 0, 510), pixels[2]);
    assert_eq!(pixel(&gpu, 0, 0), pixels[8]);
    // A command after the odd-sized upload's last word is a command again.
    gpu.write_gp0_words(&[0x6800_00F8, xy(5, 5)]);
    assert_eq!(pixel(&gpu, 5, 5), 31);

    gpu.write_gp0_words(&[0xC000_0000, xy(1022, 510), xy(3, 3)]);
    assert_eq!(gpu.gpustat() >> 27 & 1, 1);
    let mut words = Vec::new();
    for _ in 0..5 {
        words.push(gpu.read_gpuread());
    }
    assert_eq!(gpu.gpustat() >> 27 & 1, 0);
    let read: Vec<u16> = words
        .iter()
        .flat_map(|&w| [w as u16, (w >> 16) as u16])
        .collect();
    assert_eq!(&read[..9], &pixels[..]);
    assert_eq!(read[9], 0);
    // Width/height 0 mean the maximum.
    gpu.write_gp0_words(&[0xC000_0000, 0, 0]);
    assert_eq!(gpu.read_gpuread() & 0xFFFF, u32::from(pixels[8]));
}

/// 2 MB of RAM addressed by 24-bit physical addresses.
struct Ram(Vec<u32>);

impl Ram {
    fn new() -> Ram {
        Ram(vec![0; 0x80000])
    }
    fn put(&mut self, address: u32, words: &[u32]) {
        for (i, &word) in words.iter().enumerate() {
            self.write_u32(address + 4 * i as u32, word);
        }
    }
    fn node(&mut self, address: u32, next: u32, words: &[u32]) {
        self.put(address, &[(words.len() as u32) << 24 | next & 0xFF_FFFF]);
        self.put(address + 4, words);
    }
}

impl Memory for Ram {
    fn read_u32(&self, address: u32) -> u32 {
        self.0[(address as usize & 0x1F_FFFF) >> 2]
    }
}

impl MemoryMut for Ram {
    fn write_u32(&mut self, address: u32, value: u32) {
        self.0[(address as usize & 0x1F_FFFF) >> 2] = value;
    }
}

#[test]
fn linked_list_dma_until_terminator() {
    let mut ram = Ram::new();
    let mut gpu = fresh();
    ram.node(0x1000, 0x2000, &[0x0200_00F8, 0, xy(16, 1)]);
    ram.node(0x2000, 0x3000, &[]);
    // A command split over two nodes is still one command.
    ram.node(0x3000, 0x4000, &[0x6000_F800, xy(0, 2)]);
    ram.node(0x4000, 0xFF_FFFF, &[xy(2, 1)]);
    let summary = gpu.dma_linked_list(&ram, 0x8000_1000, 1 << 20).unwrap();
    assert_eq!((summary.nodes, summary.words), (4, 6));
    assert_eq!(pixel(&gpu, 15, 0), 31);
    assert_eq!(pixel(&gpu, 1, 2), rgb15(0, 31, 0));
    assert_eq!(pixel(&gpu, 2, 2), 0);
    // Only bit 23 ends the list.
    ram.node(0x5000, 0x80_0000, &[0x6800_001F, xy(7, 7)]);
    assert_eq!(gpu.dma_linked_list(&ram, 0x5000, 100).unwrap().nodes, 1);
    assert_eq!(gpu.dma_linked_list(&ram, 0xFF_FFFF, 100).unwrap().nodes, 0);
}

#[test]
fn linked_list_dma_reports_loops_and_budget() {
    let mut ram = Ram::new();
    let mut gpu = fresh();
    ram.node(0x100, 0x200, &[0]);
    ram.node(0x200, 0x300, &[0]);
    ram.node(0x300, 0x200, &[0]);
    match gpu.dma_linked_list(&ram, 0x100, 1 << 20) {
        Err(DmaError::Loop { address, summary }) => {
            assert_eq!(address, 0x200);
            assert!(summary.nodes <= 8, "found quickly: {summary:?}");
        }
        other => panic!("expected a loop, got {other:?}"),
    }
    ram.node(0x400, 0x400, &[]);
    assert!(matches!(
        gpu.dma_linked_list(&ram, 0x400, 100),
        Err(DmaError::Loop { address: 0x400, .. })
    ));
    ram.node(0x500, 0x600, &[0, 0]);
    ram.node(0x600, 0xFF_FFFF, &[0, 0]);
    assert_eq!(
        gpu.dma_linked_list(&ram, 0x500, 3),
        Err(DmaError::WordLimit {
            address: 0x600,
            summary: xem_gpu::DmaSummary { nodes: 1, words: 2 }
        })
    );
}

#[test]
fn block_dma_both_directions() {
    let mut ram = Ram::new();
    let mut gpu = fresh();
    ram.put(
        0x800,
        &[0xA000_0000, xy(20, 20), xy(2, 2), 0x0002_0001, 0x0004_0003],
    );
    gpu.dma_block_write(&ram, 0x800, 5);
    assert_eq!(pixel(&gpu, 21, 21), 4);
    gpu.write_gp0_words(&[0xC000_0000, xy(20, 20), xy(2, 2)]);
    gpu.dma_block_read(&mut ram, 0x900, 2);
    assert_eq!(ram.read_u32(0x900), 0x0002_0001);
    assert_eq!(ram.read_u32(0x904), 0x0004_0003);
}

#[test]
fn lines_and_polylines() {
    let mut gpu = fresh();
    gpu.write_gp0_words(&[0x4000_00F8, xy(0, 0), xy(5, 0)]);
    assert_eq!(drawn(&gpu), (0..=5).map(|x| (x, 0)).collect::<Vec<_>>());
    let mut gpu = fresh();
    gpu.write_gp0_words(&[0x4800_00F8, xy(0, 10), xy(4, 10), xy(4, 14), 0x5555_5555]);
    gpu.write_gp0_words(&[0x6800_F800, xy(9, 9)]);
    let mut expected: Vec<_> = (0..=4)
        .map(|x| (x, 10))
        .chain((11..=14).map(|y| (4, y)))
        .collect();
    expected.push((9, 9));
    expected.sort_by_key(|&(x, y)| (y, x));
    assert_eq!(drawn(&gpu), expected);
    assert_eq!(gpu.stats().count(PrimitiveKind::Polyline), 1);
    // Gouraud polyline: the end marker replaces a colour word.
    let mut gpu = fresh();
    gpu.write_gp0_words(&[
        0x5800_0000,
        xy(0, 0),
        rgb(0xF8, 0, 0),
        xy(8, 0),
        rgb(0, 0xF8, 0),
        xy(8, 8),
        0x5000_5000,
    ]);
    assert_eq!(pixel(&gpu, 0, 0), 0);
    assert_eq!(pixel(&gpu, 8, 0), rgb15(31, 0, 0));
    assert_eq!(pixel(&gpu, 8, 8), rgb15(0, 31, 0));
    assert_eq!(pixel(&gpu, 4, 0), rgb15(15, 0, 0));
    gpu.write_gp0_words(&[0x6800_F800, xy(20, 20)]);
    assert_eq!(pixel(&gpu, 20, 20), rgb15(0, 31, 0));
}

#[test]
fn gouraud_interpolation_and_dithering() {
    let mut gpu = fresh();
    // Red 0 at x=0 to 240 at x=16, flat in y: value 15x at pixel x.
    gpu.write_gp0_words(&[
        0x3800_0000,
        xy(0, 0),
        0xF0,
        xy(16, 0),
        0,
        xy(0, 4),
        0xF0,
        xy(16, 4),
    ]);
    for y in 0..4 {
        for x in 0..16 {
            assert_eq!(pixel(&gpu, x, y), (15 * x as u16) >> 3, "({x}, {y})");
        }
    }
    // Dithering: Gouraud yes, flat untextured no.
    gpu.write_gp0(0xE100_0200);
    gpu.write_gp0_words(&[0x2800_0080, xy(0, 8), xy(4, 8), xy(0, 12), xy(4, 12)]);
    gpu.write_gp0_words(&[
        0x3800_0080,
        xy(8, 8),
        0x80,
        xy(12, 8),
        0x80,
        xy(8, 12),
        0x80,
        xy(12, 12),
    ]);
    let flat: Vec<u16> = (0..4).map(|x| pixel(&gpu, x, 8)).collect();
    assert_eq!(flat, vec![16; 4]);
    // 0x80 + [-4, 0, -3, 1] >> 3 on row 0 of the matrix (y = 8).
    let dithered: Vec<u16> = (8..12).map(|x| pixel(&gpu, x, 8)).collect();
    assert_eq!(dithered, vec![15, 16, 15, 16]);
    // Row 1: [2, -2, 3, -1].
    let dithered: Vec<u16> = (8..12).map(|x| pixel(&gpu, x, 9)).collect();
    assert_eq!(dithered, vec![16, 15, 16, 15]);
}

#[test]
fn texture_window_masks_and_offsets() {
    let mut gpu = fresh();
    let direct: Vec<u16> = (0..16).map(|i| i + 1).collect();
    upload(&mut gpu, 192, 0, 16, 1, &direct);
    gpu.write_gp0(0xE100_0003 | 2 << 7);
    gpu.write_gp0(0xE200_0000 | 1 | 1 << 10); // mask X 8, offset X 8
    gpu.write_gp0_words(&[0x6500_0000, xy(0, 0), 0, xy(16, 1)]);
    for u in 0..16 {
        assert_eq!(pixel(&gpu, u, 0), (u as u16 | 8) + 1);
    }
}

#[test]
fn display_readout_15bit() {
    let mut gpu = fresh();
    gpu.write_gp1(0x0500_0000 | 16 | 8 << 10);
    gpu.write_gp1(0x0600_0000 | 0x260 | (0x260 + 320 * 8) << 12);
    gpu.write_gp1(0x0700_0000 | 0x18 | (0x18 + 224) << 10);
    gpu.write_gp1(0x0800_0001);
    let info = gpu.display();
    assert_eq!((info.width, info.height, info.x, info.y), (320, 224, 16, 8));
    gpu.vram_mut()[(8 + 2) * 1024 + 16 + 5] = rgb15(31, 16, 0);
    let (width, height, pixels) = gpu.display_rgba8();
    assert_eq!((width, height), (320, 224));
    assert!(
        pixels.iter().all(|&b| b == 0 || b == 0xFF),
        "disabled display is black"
    );
    gpu.write_gp1(0x0300_0000);
    let (_, _, pixels) = gpu.display_rgba8();
    let at = (2 * 320 + 5) * 4;
    assert_eq!(&pixels[at..at + 4], &[255, 132, 0, 255]);
    assert_eq!(gpu.vram_rgba8().len(), 1024 * 512 * 4);
}

#[test]
fn display_readout_24bit() {
    let mut gpu = fresh();
    gpu.write_gp1(0x0300_0000);
    gpu.write_gp1(0x0500_0000 | 100 | 20 << 10);
    gpu.write_gp1(0x0600_0000 | 0x260 | (0x260 + 320 * 8) << 12);
    gpu.write_gp1(0x0700_0000 | 0x10 | (0x10 + 240) << 10);
    gpu.write_gp1(0x0800_0011);
    // Pixels (1, 2, 3) and (4, 5, 6) as bytes R G B R G B from X 100.
    upload(&mut gpu, 100, 20, 3, 1, &[0x0201, 0x0403, 0x0605]);
    let (width, height, pixels) = gpu.display_rgba8();
    assert_eq!((width, height), (320, 240));
    assert!(gpu.display().color24);
    assert_eq!(&pixels[..8], &[1, 2, 3, 255, 4, 5, 6, 255]);
}

#[test]
fn display_480i_doubles_lines_and_368_mode() {
    let mut gpu = Gpu::new();
    gpu.write_gp1(0x0600_0000 | 0x260 | (0x260 + 368 * 7) << 12);
    gpu.write_gp1(0x0700_0000 | 0x10 | (0x10 + 240) << 10);
    gpu.write_gp1(0x0800_0064);
    let info = gpu.display();
    assert_eq!((info.width, info.height, info.mode_width), (368, 480, 368));
}

#[test]
fn stats_count_per_kind_and_reset() {
    let mut gpu = fresh();
    gpu.write_gp0_words(&[0x2000_00F8, xy(0, 0), xy(4, 0), xy(0, 4)]);
    gpu.write_gp0_words(&[0x3000_0000, xy(0, 0), 0, xy(4, 0), 0, xy(0, 4)]);
    gpu.write_gp0(0);
    let stats = gpu.take_stats();
    assert_eq!(stats.count(PrimitiveKind::PolygonFlat), 1);
    assert_eq!(stats.count(PrimitiveKind::PolygonGouraud), 1);
    assert_eq!(stats.count(PrimitiveKind::Environment), 2);
    assert_eq!(stats.count(PrimitiveKind::Other), 1);
    assert_eq!(stats.total(), 5);
    assert_eq!(gpu.stats().total(), 0);
}

/// Feeds the same words to a GPU and to a copy restored from its snapshot.
fn assert_resumes(mut gpu: Gpu, rest: &[u32]) {
    let saved = gpu.save();
    assert_eq!(saved, gpu.save(), "saving is deterministic");
    let mut restored = Gpu::new();
    restored.load(&saved).unwrap();
    assert_eq!(restored.save(), saved);
    gpu.write_gp0_words(rest);
    restored.write_gp0_words(rest);
    assert_eq!(gpu.save(), restored.save());
    assert_eq!(gpu.read_gpuread(), restored.read_gpuread());
}

#[test]
fn snapshot_round_trips_mid_command_and_transfer() {
    let mut gpu = fresh();
    gpu.write_gp1(0x0800_0037);
    gpu.write_gp1(0x0500_1234);
    gpu.write_gp0_words(&[0xE100_0123, 0xE200_1234, 0xE500_1234, 0xE600_0001]);
    gpu.write_gp0_words(&[0x0212_3456, xy(0, 0), xy(64, 64)]);

    // Half a quad.
    let mut partial = gpu.clone();
    partial.write_gp0_words(&[0x2800_F800, xy(0, 0), xy(10, 0)]);
    assert_resumes(partial, &[xy(0, 10), xy(10, 10)]);

    // A polyline waiting for its next colour.
    let mut polyline = gpu.clone();
    polyline.write_gp0_words(&[0x5800_0000, xy(0, 0), 0xF8, xy(8, 0)]);
    assert_resumes(
        polyline,
        &[0xF800, xy(8, 8), 0x5555_5555, 0x6800_00F8, xy(1, 1)],
    );

    // An upload and a read-back in progress.
    let mut transfer = gpu.clone();
    transfer.write_gp0_words(&[0xC000_0000, xy(0, 0), xy(4, 1)]);
    transfer.read_gpuread();
    transfer.write_gp0_words(&[0xA000_0000, xy(100, 100), xy(3, 3), 0x1111_2222]);
    assert_resumes(
        transfer,
        &[0x3333_4444, 0x5555_6666, 0x7777_8888, 0x9999_AAAA],
    );
}

#[test]
fn snapshot_load_rejects_bad_data_and_keeps_state() {
    let mut gpu = fresh();
    gpu.write_gp0_words(&[0x0212_3456, xy(0, 0), xy(16, 16)]);
    let saved = gpu.save();
    let mut other = Gpu::new();
    other.write_gp0_words(&[0x2800_F800, xy(0, 0)]);
    let before = other.save();

    assert_eq!(
        other.load(&saved[..saved.len() - 1]),
        Err(SnapshotError::Length)
    );
    let mut extra = saved.clone();
    extra.push(0);
    assert_eq!(other.load(&extra), Err(SnapshotError::Length));
    let mut magic = saved.clone();
    magic[0] = b'Y';
    assert_eq!(other.load(&magic), Err(SnapshotError::Magic));
    let mut version = saved.clone();
    version[8] = 9;
    assert_eq!(other.load(&version), Err(SnapshotError::Version(9)));
    let mut register = saved.clone();
    let draw_mode = 12 + 1024 * 512 * 2;
    register[draw_mode + 3] = 0xFF;
    assert!(matches!(
        other.load(&register),
        Err(SnapshotError::Invalid(_))
    ));
    assert_eq!(other.save(), before);

    other.load(&saved).unwrap();
    assert_eq!(other.vram(), gpu.vram());
}

/// Whether pixel `p` is inside triangle `t` by the top-left rule, tested
/// directly on the edge functions.
fn covers(t: [(i64, i64); 3], p: (i64, i64)) -> bool {
    let cross = |a: (i64, i64), b: (i64, i64), c: (i64, i64)| {
        (b.0 - a.0) * (c.1 - a.1) - (b.1 - a.1) * (c.0 - a.0)
    };
    let area = cross(t[0], t[1], t[2]);
    if area == 0 {
        return false;
    }
    let t = if area < 0 { [t[0], t[2], t[1]] } else { t };
    [(0, 1), (1, 2), (2, 0)].iter().all(|&(i, j)| {
        let (a, b) = (t[i], t[j]);
        let w = cross(a, b, p);
        let (dx, dy) = (b.0 - a.0, b.1 - a.1);
        w > 0 || (w == 0 && (dy < 0 || (dy == 0 && dx > 0)))
    })
}

#[test]
fn random_triangles_cover_by_the_rule_and_shared_edges_do_not_overlap() {
    let mut seed = 0x1234_5678u32;
    let mut random = || {
        seed = seed.wrapping_mul(1_664_525).wrapping_add(1_013_904_223);
        (seed >> 8) as i32 % 96 - 16
    };
    for _ in 0..300 {
        let points: Vec<(i32, i32)> = (0..4).map(|_| (random(), random())).collect();
        let word = |(x, y): (i32, i32)| xy(x, y);
        let mut gpu = fresh();
        gpu.write_gp0(0xE500_0000 | 16 | 16 << 11); // keep negative points on screen
        gpu.write_gp0(0xE100_0020); // B+F
        gpu.write_gp0_words(&[
            0x2200_0008,
            word(points[0]),
            word(points[1]),
            word(points[2]),
        ]);
        let t = [0, 1, 2].map(|i| (i64::from(points[i].0 + 16), i64::from(points[i].1 + 16)));
        for y in 0..128 {
            for x in 0..128 {
                let expected = u16::from(covers(t, (x as i64, y as i64)));
                assert_eq!(pixel(&gpu, x, y), expected, "{points:?} at ({x}, {y})");
            }
        }
        // The second triangle shares edge 1-2; with 0 and 3 on opposite
        // sides the two must not overlap.
        let side = |p: (i32, i32)| {
            let (a, b) = (points[1], points[2]);
            (b.0 - a.0) * (p.1 - a.1) - (b.1 - a.1) * (p.0 - a.0)
        };
        if side(points[0]).signum() * side(points[3]).signum() < 0 {
            gpu.write_gp0_words(&[
                0x2200_0008,
                word(points[1]),
                word(points[2]),
                word(points[3]),
            ]);
            assert!(gpu.vram().iter().all(|&p| p <= 1), "{points:?} overlap");
        }
    }
}
