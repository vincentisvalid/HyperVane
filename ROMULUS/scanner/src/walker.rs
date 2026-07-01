use crate::disc::{detect_serial_from_image, dir_size, find_ps3_param_sfo, parse_param_sfo};
use crate::hasher::hash_file;
use anyhow::Result;
use rayon::prelude::*;
use serde::Serialize;
use std::path::{Path, PathBuf};
use walkdir::WalkDir;

// Files larger than this will use serial detection instead of full hashing
const LARGE_FILE_THRESHOLD: u64 = 100 * 1024 * 1024; // 100 MB

const DISC_EXTENSIONS: &[&str] = &["iso", "bin", "img", "nrg", "chd", "mdf", "pbp", "cso"];

#[derive(Debug, Serialize)]
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

enum WorkItem {
    RegularFile(PathBuf),
    DiscImage(PathBuf),
    DiscDirectory(PathBuf),
}

/// Walk root, handle PS3 disc dirs as atomic units, and parallelize everything else.
pub fn scan_directory(root: &Path) -> Vec<ScanEntry> {
    // First pass: find PS3 directories (top-level or one level deep)
    let mut disc_dirs: Vec<PathBuf> = Vec::new();
    let mut skip_prefixes: Vec<PathBuf> = Vec::new();

    if let Ok(entries) = std::fs::read_dir(root) {
        for entry in entries.flatten() {
            let p = entry.path();
            if p.is_dir() {
                if find_ps3_param_sfo(&p).is_some() {
                    disc_dirs.push(p.clone());
                    skip_prefixes.push(p);
                }
            }
        }
    }

    // Second pass: collect all other files, skipping paths inside disc dirs
    let mut regular_items: Vec<WorkItem> = Vec::new();
    for entry in WalkDir::new(root)
        .follow_links(false)
        .into_iter()
        .filter_map(|e| e.ok())
        .filter(|e| e.file_type().is_file())
    {
        let path = entry.path().to_path_buf();

        // Skip files that live inside a detected disc directory
        if skip_prefixes.iter().any(|p| path.starts_with(p)) {
            continue;
        }

        let ext = path
            .extension()
            .and_then(|e| e.to_str())
            .unwrap_or("")
            .to_lowercase();

        if DISC_EXTENSIONS.contains(&ext.as_str()) {
            regular_items.push(WorkItem::DiscImage(path));
        } else {
            regular_items.push(WorkItem::RegularFile(path));
        }
    }

    // Add disc dirs as their own work items
    let disc_dir_items: Vec<WorkItem> = disc_dirs
        .into_iter()
        .map(WorkItem::DiscDirectory)
        .collect();

    let all_items: Vec<WorkItem> = regular_items.into_iter().chain(disc_dir_items).collect();

    // Process all items in parallel
    all_items
        .into_par_iter()
        .map(|item| process_item(item))
        .collect()
}

fn process_item(item: WorkItem) -> ScanEntry {
    match item {
        WorkItem::RegularFile(path) => process_regular_file(&path),
        WorkItem::DiscImage(path) => process_disc_image(&path),
        WorkItem::DiscDirectory(path) => process_disc_directory(&path),
    }
}

fn process_regular_file(path: &Path) -> ScanEntry {
    let path_str = path.to_string_lossy().to_string();
    let filename = path
        .file_name()
        .map(|n| n.to_string_lossy().to_string())
        .unwrap_or_default();

    match hash_file(path) {
        Ok(hashes) => ScanEntry::Hash {
            path: path_str,
            filename,
            sha1: hashes.sha1,
            crc32: hashes.crc32,
            size: hashes.size,
        },
        Err(e) => ScanEntry::Error {
            path: path_str,
            message: e.to_string(),
        },
    }
}

fn process_disc_image(path: &Path) -> ScanEntry {
    let path_str = path.to_string_lossy().to_string();
    let filename = path
        .file_name()
        .map(|n| n.to_string_lossy().to_string())
        .unwrap_or_default();

    let size = path
        .metadata()
        .map(|m| m.len())
        .unwrap_or(0);

    // For large disc images, prefer serial detection to avoid stalling I/O
    if size > LARGE_FILE_THRESHOLD {
        if let Some(serial) = detect_serial_from_image(path, 524288) {
            return ScanEntry::Serial {
                path: path_str,
                filename,
                serial,
                title: None,
                size,
            };
        }
        // Fall through to full hash if serial detection failed (with size warning in caller)
    }

    // Small disc images or failed serial detection: full hash
    match hash_file(path) {
        Ok(hashes) => ScanEntry::Hash {
            path: path_str,
            filename,
            sha1: hashes.sha1,
            crc32: hashes.crc32,
            size: hashes.size,
        },
        Err(e) => ScanEntry::Error {
            path: path_str,
            message: e.to_string(),
        },
    }
}

fn process_disc_directory(path: &Path) -> ScanEntry {
    let path_str = path.to_string_lossy().to_string();
    let filename = path
        .file_name()
        .map(|n| n.to_string_lossy().to_string())
        .unwrap_or_default();

    let size = dir_size(path);

    if let Some(sfo_path) = find_ps3_param_sfo(path) {
        match parse_param_sfo(&sfo_path) {
            Ok(sfo) => {
                if let Some(serial) = sfo.title_id {
                    return ScanEntry::Serial {
                        path: path_str,
                        filename,
                        serial,
                        title: sfo.title,
                        size,
                    };
                }
            }
            Err(e) => {
                return ScanEntry::Error {
                    path: path_str,
                    message: format!("PARAM.SFO parse error: {}", e),
                };
            }
        }
    }

    ScanEntry::Error {
        path: path_str,
        message: "Disc directory found but could not extract serial".to_string(),
    }
}

pub fn scan_file(path: &Path) -> Result<ScanEntry> {
    if path.is_dir() {
        return Ok(process_disc_directory(path));
    }

    let ext = path
        .extension()
        .and_then(|e| e.to_str())
        .unwrap_or("")
        .to_lowercase();

    if DISC_EXTENSIONS.contains(&ext.as_str()) {
        Ok(process_disc_image(path))
    } else {
        Ok(process_regular_file(path))
    }
}
