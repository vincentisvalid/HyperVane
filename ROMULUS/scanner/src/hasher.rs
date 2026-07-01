use anyhow::Result;
use crc32fast::Hasher as Crc32Hasher;
use sha1::{Digest, Sha1};
use std::fs::File;
use std::io::{BufReader, Read};
use std::path::Path;

const CHUNK: usize = 65536; // 64 KB read buffer

pub struct FileHashes {
    pub sha1: String,
    pub crc32: String,
    pub size: u64,
}

/// Compute SHA1 and CRC32 over the full file contents in a single pass.
pub fn hash_file(path: &Path) -> Result<FileHashes> {
    let file = File::open(path)?;
    let metadata = file.metadata()?;
    let size = metadata.len();

    let mut reader = BufReader::with_capacity(CHUNK, file);
    let mut sha1_hasher = Sha1::new();
    let mut crc32_hasher = Crc32Hasher::new();

    let mut buf = [0u8; CHUNK];
    loop {
        let n = reader.read(&mut buf)?;
        if n == 0 {
            break;
        }
        sha1_hasher.update(&buf[..n]);
        crc32_hasher.update(&buf[..n]);
    }

    let sha1_bytes = sha1_hasher.finalize();
    let sha1 = format!("{:x}", sha1_bytes);
    let crc32 = format!("{:08x}", crc32_hasher.finalize());

    Ok(FileHashes { sha1, crc32, size })
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::io::Write;

    #[test]
    fn test_hash_known_content() {
        let dir = std::env::temp_dir();
        let path = dir.join("romulus_hasher_test.bin");
        {
            let mut f = std::fs::File::create(&path).unwrap();
            f.write_all(b"abc").unwrap();
        }
        let hashes = hash_file(&path).unwrap();
        std::fs::remove_file(&path).ok();
        assert_eq!(hashes.sha1, "a9993e364706816aba3e25717850c26c9cd0d89d");
        assert_eq!(hashes.crc32, "352441c2");
        assert_eq!(hashes.size, 3);
    }
}
