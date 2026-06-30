use std::io::{self, Read};
use crc32fast::Hasher;

pub fn hash_crc32<R: Read>(mut reader: R) -> io::Result<String> {
    let mut hasher = Hasher::new();
    let mut buf = [0u8; 65536];
    loop {
        let n = reader.read(&mut buf)?;
        if n == 0 { break; }
        hasher.update(&buf[..n]);
    }
    Ok(format!("{:08X}", hasher.finalize()))
}
