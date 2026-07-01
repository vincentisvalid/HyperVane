use serde::{Deserialize, Serialize};

/// Scan entry emitted by the scanner binary (NDJSON).
#[derive(Debug, Deserialize)]
#[serde(tag = "type", rename_all = "snake_case")]
pub enum ScanEntry {
    Hash {
        path: String,
        filename: String,
        sha1: String,
        crc32: String,
        size: u64,
    },
    Serial {
        path: String,
        filename: String,
        serial: String,
        title: Option<String>,
        size: u64,
    },
    Error {
        path: String,
        message: String,
    },
}

/// A row from reference_catalog.db.
#[derive(Debug)]
pub struct ReferenceTitle {
    pub id: i64,
    pub system_tag: Option<String>,
    pub serial_number: Option<String>,
    pub title: Option<String>,
    pub region: Option<String>,
}

/// Data written into user_library.db.
#[derive(Debug)]
pub struct LibraryEntry {
    pub local_path: String,
    pub filename: String,
    pub sha1_hash: Option<String>,
    pub crc32_hash: Option<String>,
    pub file_size: Option<u64>,
    pub serial_number: Option<String>,
    pub system_tag: Option<String>,
    pub title: Option<String>,
    pub region: Option<String>,
    pub is_matched: bool,
    pub ref_id: Option<i64>,
}

/// Summary printed to stderr after matching.
#[derive(Debug, Default, Serialize)]
pub struct MatchSummary {
    pub total: usize,
    pub matched: usize,
    pub unmatched: usize,
    pub errors: usize,
}
