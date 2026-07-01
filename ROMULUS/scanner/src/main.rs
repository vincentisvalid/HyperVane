mod disc;
mod hasher;
mod walker;

use anyhow::Result;
use clap::Parser;
use std::io::{self, BufWriter, Write};
use std::path::PathBuf;
use walker::{scan_directory, scan_file};

#[derive(Parser)]
#[command(
    name = "scanner",
    about = "ROM file scanner — emits NDJSON of file hashes and disc serials to stdout",
    long_about = "Scans a directory (or single file) for ROM files.\n\
                  Outputs one JSON object per line to stdout, suitable for piping into the matcher.\n\n\
                  Usage:\n  scanner /path/to/roms | matcher\n  scanner /path/to/roms > scan.ndjson"
)]
struct Args {
    /// Path to scan (directory or single file)
    path: PathBuf,

    /// Pretty-print JSON output (one record per line by default)
    #[arg(long)]
    pretty: bool,
}

fn main() -> Result<()> {
    let args = Args::parse();
    let stdout = io::stdout();
    let mut out = BufWriter::new(stdout.lock());

    if args.path.is_file() {
        let entry = scan_file(&args.path)?;
        let line = if args.pretty {
            serde_json::to_string_pretty(&entry)?
        } else {
            serde_json::to_string(&entry)?
        };
        writeln!(out, "{}", line)?;
    } else if args.path.is_dir() {
        let entries = scan_directory(&args.path);
        for entry in entries {
            let line = if args.pretty {
                serde_json::to_string_pretty(&entry)?
            } else {
                serde_json::to_string(&entry)?
            };
            writeln!(out, "{}", line)?;
        }
    } else {
        anyhow::bail!("Path does not exist: {}", args.path.display());
    }

    out.flush()?;
    Ok(())
}
