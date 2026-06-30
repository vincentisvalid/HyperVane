use std::io::{self, Read};
use sha1::{Digest, Sha1};

pub fn hash_sha1<R: Read>(mut reader: R) -> io::Result<String> {
    let mut hasher = Sha1::new();
    let mut buf = [0u8; 65536];
    loop {
        let n = reader.read(&mut buf)?;
        if n == 0 { break; }
        hasher.update(&buf[..n]);
    }
    Ok(hasher.finalize().iter().map(|b| format!("{:02X}", b)).collect())
}
