//! The PS-X EXE the BIOS loads: header fields and the text it copies to RAM.

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct ExeHeader {
    pub pc: u32,
    pub gp: u32,
    pub text_address: u32,
    pub text_size: u32,
}

pub const HEADER_SIZE: usize = 0x800;

pub fn parse(exe: &[u8]) -> Result<ExeHeader, String> {
    if exe.len() < HEADER_SIZE || &exe[..8] != b"PS-X EXE" {
        return Err("not a PS-X EXE".into());
    }
    let word = |offset: usize| u32::from_le_bytes(exe[offset..offset + 4].try_into().unwrap());
    let header = ExeHeader { pc: word(0x10), gp: word(0x14), text_address: word(0x18), text_size: word(0x1C) };
    if HEADER_SIZE + header.text_size as usize > exe.len() {
        return Err(format!("text size {:#x} exceeds the file", header.text_size));
    }
    Ok(header)
}

/// The text the BIOS copies to `text_address`.
pub fn text<'a>(exe: &'a [u8], header: &ExeHeader) -> &'a [u8] {
    &exe[HEADER_SIZE..HEADER_SIZE + header.text_size as usize]
}
