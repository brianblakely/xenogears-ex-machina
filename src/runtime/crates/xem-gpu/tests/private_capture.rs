//! Optional: walks ordering tables from a local RAM image (never committed).
//!
//! `XEM_GPU_RAM` names a raw 2 MiB RAM image (the input of
//! `tools/analysis/gpu_packets.py --ram`) and `XEM_GPU_OT` the list heads as
//! comma-separated hexadecimal addresses. Without them the test passes
//! without checking anything.

use xem_gpu::{Gpu, Memory};

struct Ram(Vec<u8>);

impl Memory for Ram {
    fn read_u32(&self, address: u32) -> u32 {
        let offset = address as usize & 0x1F_FFFC;
        u32::from_le_bytes(self.0[offset..offset + 4].try_into().unwrap())
    }
}

#[test]
fn ordering_tables_from_local_ram_image() {
    let (Ok(path), Ok(heads)) = (std::env::var("XEM_GPU_RAM"), std::env::var("XEM_GPU_OT")) else {
        eprintln!("skipped: set XEM_GPU_RAM and XEM_GPU_OT to walk a local RAM image");
        return;
    };
    let ram = Ram(std::fs::read(&path).expect("XEM_GPU_RAM is readable"));
    assert_eq!(ram.0.len(), 0x20_0000, "a raw 2 MiB RAM image");
    let mut gpu = Gpu::new();
    gpu.write_gp0_words(&[0xE300_0000, 0xE400_0000 | 1023 | 511 << 10]);
    for head in heads.split(',') {
        let head =
            u32::from_str_radix(head.trim().trim_start_matches("0x"), 16).expect("hex list head");
        let summary = gpu
            .dma_linked_list(&ram, head, 1 << 22)
            .expect("the list ends");
        eprintln!(
            "list {head:08x}: {} nodes, {} words",
            summary.nodes, summary.words
        );
    }
    let stats = gpu.take_stats();
    for kind in xem_gpu::PrimitiveKind::ALL {
        eprintln!("{kind:?}: {}", stats.count(kind));
    }
    eprintln!("rejected: {}", stats.rejected());
    assert!(stats.total() > 0, "the lists hold commands");
}
