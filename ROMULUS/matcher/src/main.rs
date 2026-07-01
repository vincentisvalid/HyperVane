mod db;
mod models;

use anyhow::Result;
use clap::Parser;
use models::{LibraryEntry, MatchSummary, ScanEntry};
use std::io::{self, BufRead};

#[derive(Parser)]
#[command(
    name = "matcher",
    about = "ROM matcher — reads scanner NDJSON from stdin, matches against reference catalog, writes to user library",
    long_about = "Reads NDJSON lines from stdin (output of the scanner binary).\n\
                  For each entry, looks up the hash or serial in reference_catalog.db.\n\
                  Writes matched and unmatched results into user_library.db.\n\n\
                  Usage:\n  scanner /path/to/roms | matcher\n  cat scan.ndjson | matcher --catalog /data/reference_catalog.db"
)]
struct Args {
    /// Path to the reference catalog database
    #[arg(long, default_value = "reference_catalog.db")]
    catalog: String,

    /// Path to the user library database (created if absent)
    #[arg(long, default_value = "user_library.db")]
    library: String,

    /// Print a summary to stderr when done
    #[arg(long, default_value_t = true)]
    summary: bool,
}

fn main() -> Result<()> {
    let args = Args::parse();

    let catalog = db::open_catalog(&args.catalog)?;
    let library = db::open_library(&args.library)?;

    let stdin = io::stdin();
    let mut summary = MatchSummary::default();

    for line in stdin.lock().lines() {
        let line = line?;
        let line = line.trim();
        if line.is_empty() {
            continue;
        }

        let entry: ScanEntry = match serde_json::from_str(line) {
            Ok(e) => e,
            Err(e) => {
                eprintln!("warn: could not parse line: {} — {}", line, e);
                continue;
            }
        };

        summary.total += 1;

        match process_entry(&catalog, &library, entry) {
            Ok(matched) => {
                if matched {
                    summary.matched += 1;
                } else {
                    summary.unmatched += 1;
                }
            }
            Err(e) => {
                eprintln!("error: {}", e);
                summary.errors += 1;
            }
        }
    }

    if args.summary {
        eprintln!(
            "Done — {} total, {} matched, {} unmatched, {} errors",
            summary.total, summary.matched, summary.unmatched, summary.errors
        );
    }

    Ok(())
}

fn process_entry(
    catalog: &rusqlite::Connection,
    library: &rusqlite::Connection,
    entry: ScanEntry,
) -> Result<bool> {
    match entry {
        ScanEntry::Error { path, message } => {
            eprintln!("scan error [{}]: {}", path, message);
            Ok(false)
        }

        ScanEntry::Hash {
            path,
            filename,
            sha1,
            crc32,
            size,
        } => {
            let reference = db::lookup_by_hash(catalog, &sha1, &crc32)?;
            let matched = reference.is_some();

            let lib_entry = LibraryEntry {
                local_path: path,
                filename,
                sha1_hash: Some(sha1),
                crc32_hash: Some(crc32),
                file_size: Some(size),
                serial_number: reference.as_ref().and_then(|r| r.serial_number.clone()),
                system_tag: reference.as_ref().and_then(|r| r.system_tag.clone()),
                title: reference.as_ref().and_then(|r| r.title.clone()),
                region: reference.as_ref().and_then(|r| r.region.clone()),
                is_matched: matched,
                ref_id: reference.map(|r| r.id),
            };

            db::upsert_library_entry(library, &lib_entry)?;
            Ok(matched)
        }

        ScanEntry::Serial {
            path,
            filename,
            serial,
            title: scanned_title,
            size,
        } => {
            let reference = db::lookup_by_serial(catalog, &serial)?;
            let matched = reference.is_some();

            let lib_entry = LibraryEntry {
                local_path: path,
                filename,
                sha1_hash: None,
                crc32_hash: None,
                file_size: Some(size),
                serial_number: Some(serial),
                system_tag: reference.as_ref().and_then(|r| r.system_tag.clone()),
                // Prefer catalog title; fall back to title extracted from PARAM.SFO
                title: reference
                    .as_ref()
                    .and_then(|r| r.title.clone())
                    .or(scanned_title),
                region: reference.as_ref().and_then(|r| r.region.clone()),
                is_matched: matched,
                ref_id: reference.map(|r| r.id),
            };

            db::upsert_library_entry(library, &lib_entry)?;
            Ok(matched)
        }
    }
}
