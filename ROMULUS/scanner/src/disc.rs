use anyhow::{bail, Result};
use std::fs::File;
use std::io::Read;
use std::path::Path;

// PARAM.SFO magic: \x00PSF
const SFO_MAGIC: [u8; 4] = [0x00, 0x50, 0x53, 0x46];

const SFO_FMT_UTF8: u16 = 0x0204;

pub struct SfoData {
    pub title_id: Option<String>,
    pub title: Option<String>,
}

/// Parse a PARAM.SFO binary file and extract TITLE_ID and TITLE.
pub fn parse_param_sfo(path: &Path) -> Result<SfoData> {
    let mut file = File::open(path)?;
    let mut buf = Vec::new();
    file.read_to_end(&mut buf)?;

    if buf.len() < 20 || buf[..4] != SFO_MAGIC {
        bail!("Not a valid PARAM.SFO file");
    }

    let key_table_offset = u32::from_le_bytes(buf[8..12].try_into()?) as usize;
    let data_table_offset = u32::from_le_bytes(buf[12..16].try_into()?) as usize;
    let num_entries = u32::from_le_bytes(buf[16..20].try_into()?) as usize;

    let mut title_id: Option<String> = None;
    let mut title: Option<String> = None;

    for i in 0..num_entries {
        let entry_offset = 20 + i * 16;
        if entry_offset + 16 > buf.len() {
            break;
        }

        let key_offset = u16::from_le_bytes(buf[entry_offset..entry_offset + 2].try_into()?) as usize;
        let data_fmt = u16::from_le_bytes(buf[entry_offset + 2..entry_offset + 4].try_into()?);
        let data_len = u32::from_le_bytes(buf[entry_offset + 4..entry_offset + 8].try_into()?) as usize;
        let data_offset = u32::from_le_bytes(buf[entry_offset + 12..entry_offset + 16].try_into()?) as usize;

        let key_start = key_table_offset + key_offset;
        let key = read_cstring(&buf, key_start);

        if data_fmt == SFO_FMT_UTF8 {
            let data_start = data_table_offset + data_offset;
            let data_end = (data_start + data_len).min(buf.len());
            if data_start < buf.len() {
                let raw = &buf[data_start..data_end];
                let value = String::from_utf8_lossy(raw)
                    .trim_end_matches('\0')
                    .to_string();

                match key.as_str() {
                    "TITLE_ID" => title_id = Some(normalize_serial(&value)),
                    "TITLE" => title = Some(value),
                    _ => {}
                }
            }
        }
    }

    Ok(SfoData { title_id, title })
}

fn read_cstring(buf: &[u8], offset: usize) -> String {
    if offset >= buf.len() {
        return String::new();
    }
    let end = buf[offset..].iter().position(|&b| b == 0).unwrap_or(buf.len() - offset);
    String::from_utf8_lossy(&buf[offset..offset + end]).to_string()
}

// Known PS3 directory structure: <game_dir>/PS3_GAME/PARAM.SFO
pub fn find_ps3_param_sfo(dir: &Path) -> Option<std::path::PathBuf> {
    let candidate = dir.join("PS3_GAME").join("PARAM.SFO");
    if candidate.exists() {
        return Some(candidate);
    }
    // Some dumps place it directly as PARAM.SFO in the folder
    let direct = dir.join("PARAM.SFO");
    if direct.exists() {
        return Some(direct);
    }
    None
}

// Serial patterns for PS1/PS2/PSP detection in raw ISO bytes
// e.g. SLUS-00157, BLES-01330, ULUS-12345
const SERIAL_PATTERNS: &[&[u8]] = &[
    b"SLUS", b"SCUS", b"SCES", b"SLES", b"SLPS", b"SLPM",
    b"SCPS", b"SCAJ", b"PAPX", b"PBPX",
    b"BLES", b"BLUS", b"BLJM", b"BLKS", b"BLAS",
    b"ULUS", b"ULES", b"ULJM", b"ULJS", b"UCUS", b"UCES",
    b"NPUH", b"NPJH", b"NPEH",
];

/// Scan the first `limit` bytes of a file for a disc serial number.
pub fn detect_serial_from_image(path: &Path, limit: usize) -> Option<String> {
    let mut file = File::open(path).ok()?;
    let read_size = limit.min(524288); // max 512 KB scan window
    let mut buf = vec![0u8; read_size];
    let n = file.read(&mut buf).ok()?;
    buf.truncate(n);

    scan_bytes_for_serial(&buf)
}

pub fn scan_bytes_for_serial(buf: &[u8]) -> Option<String> {
    for pattern in SERIAL_PATTERNS {
        if let Some(pos) = find_bytes(buf, pattern) {
            // Expect: XXXX[-_]DDDDD where D is a digit
            let start = pos;
            if start + 10 > buf.len() {
                continue;
            }
            let candidate = &buf[start..start + 10];
            // Verify: 4 alpha, 1 separator, 5 digits
            let alpha_ok = candidate[..4].iter().all(|b| b.is_ascii_alphabetic());
            let sep_ok = candidate[4] == b'-' || candidate[4] == b'_';
            let digits_ok = candidate[5..10].iter().all(|b| b.is_ascii_digit());
            if alpha_ok && sep_ok && digits_ok {
                let raw = std::str::from_utf8(candidate).ok()?;
                return Some(normalize_serial(raw));
            }
        }
    }
    None
}

fn find_bytes(haystack: &[u8], needle: &[u8]) -> Option<usize> {
    haystack.windows(needle.len()).position(|w| w == needle)
}

/// Normalize serial: uppercase, use hyphen separator.
pub fn normalize_serial(s: &str) -> String {
    let s = s.trim().to_uppercase();
    if s.len() >= 9 {
        let mut chars = s.chars();
        let prefix: String = chars.by_ref().take(4).collect();
        let sep: char = chars.next().unwrap_or('-');
        let suffix: String = chars.take(5).collect();
        if sep == '_' || sep == '-' {
            return format!("{}-{}", prefix, suffix);
        }
    }
    s
}

/// Compute directory size by summing all file sizes (non-recursive would miss subdirs).
pub fn dir_size(path: &Path) -> u64 {
    walkdir::WalkDir::new(path)
        .into_iter()
        .filter_map(|e| e.ok())
        .filter(|e| e.file_type().is_file())
        .filter_map(|e| e.metadata().ok())
        .map(|m| m.len())
        .sum()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_normalize_serial() {
        assert_eq!(normalize_serial("SLUS_00157"), "SLUS-00157");
        assert_eq!(normalize_serial("BLES-01330"), "BLES-01330");
        assert_eq!(normalize_serial("slus-00157"), "SLUS-00157");
    }

    #[test]
    fn test_scan_bytes_for_serial() {
        let mut buf = vec![0u8; 64];
        buf[10..20].copy_from_slice(b"SLUS-00157");
        assert_eq!(scan_bytes_for_serial(&buf), Some("SLUS-00157".to_string()));
    }
}
