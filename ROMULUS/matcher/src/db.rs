use crate::models::{LibraryEntry, ReferenceTitle};
use anyhow::Result;
use rusqlite::{params, Connection, OptionalExtension};

const REFERENCE_CATALOG_SCHEMA: &str = "
CREATE TABLE IF NOT EXISTS reference_titles (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    system_tag TEXT,
    serial_number TEXT,
    title TEXT,
    region TEXT,
    sha1_hash TEXT,
    crc32_hash TEXT
);
CREATE INDEX IF NOT EXISTS idx_sha1 ON reference_titles(sha1_hash) WHERE sha1_hash IS NOT NULL;
CREATE INDEX IF NOT EXISTS idx_crc32 ON reference_titles(crc32_hash) WHERE crc32_hash IS NOT NULL;
CREATE INDEX IF NOT EXISTS idx_serial ON reference_titles(serial_number) WHERE serial_number IS NOT NULL;
";

const USER_LIBRARY_SCHEMA: &str = "
CREATE TABLE IF NOT EXISTS user_library (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    ref_id INTEGER,
    local_path TEXT NOT NULL UNIQUE,
    filename TEXT,
    sha1_hash TEXT,
    crc32_hash TEXT,
    file_size INTEGER,
    serial_number TEXT,
    system_tag TEXT,
    title TEXT,
    region TEXT,
    is_matched INTEGER NOT NULL DEFAULT 0,
    play_count INTEGER NOT NULL DEFAULT 0,
    last_played TEXT,
    custom_title TEXT,
    custom_tags TEXT,
    cover_art_path TEXT,
    background_art_path TEXT,
    video_preview_path TEXT,
    description TEXT,
    developer TEXT,
    publisher TEXT,
    release_date TEXT,
    screenscraper_id TEXT,
    date_added TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    date_updated TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);
CREATE INDEX IF NOT EXISTS idx_lib_sha1 ON user_library(sha1_hash) WHERE sha1_hash IS NOT NULL;
CREATE INDEX IF NOT EXISTS idx_lib_serial ON user_library(serial_number) WHERE serial_number IS NOT NULL;
CREATE TRIGGER IF NOT EXISTS trg_lib_updated
AFTER UPDATE ON user_library
BEGIN
    UPDATE user_library SET date_updated = CURRENT_TIMESTAMP WHERE id = NEW.id;
END;
";

pub fn open_catalog(path: &str) -> Result<Connection> {
    let conn = Connection::open(path)?;
    conn.execute_batch("PRAGMA journal_mode=WAL; PRAGMA synchronous=NORMAL;")?;
    conn.execute_batch(REFERENCE_CATALOG_SCHEMA)?;
    Ok(conn)
}

pub fn open_library(path: &str) -> Result<Connection> {
    let conn = Connection::open(path)?;
    conn.execute_batch("PRAGMA journal_mode=WAL; PRAGMA synchronous=NORMAL;")?;
    conn.execute_batch(USER_LIBRARY_SCHEMA)?;
    Ok(conn)
}

/// Look up a title by SHA1 hash, then by CRC32 if SHA1 misses.
pub fn lookup_by_hash(
    conn: &Connection,
    sha1: &str,
    crc32: &str,
) -> Result<Option<ReferenceTitle>> {
    let by_sha1: Option<ReferenceTitle> = conn
        .query_row(
            "SELECT id, system_tag, serial_number, title, region
             FROM reference_titles WHERE sha1_hash = ?1 LIMIT 1",
            params![sha1],
            map_reference_title,
        )
        .optional()?;

    if by_sha1.is_some() {
        return Ok(by_sha1);
    }

    conn.query_row(
        "SELECT id, system_tag, serial_number, title, region
         FROM reference_titles WHERE crc32_hash = ?1 LIMIT 1",
        params![crc32],
        map_reference_title,
    )
    .optional()
    .map_err(Into::into)
}

/// Look up a title by disc serial number.
pub fn lookup_by_serial(
    conn: &Connection,
    serial: &str,
) -> Result<Option<ReferenceTitle>> {
    conn.query_row(
        "SELECT id, system_tag, serial_number, title, region
         FROM reference_titles WHERE serial_number = ?1 LIMIT 1",
        params![serial],
        map_reference_title,
    )
    .optional()
    .map_err(Into::into)
}

fn map_reference_title(row: &rusqlite::Row<'_>) -> rusqlite::Result<ReferenceTitle> {
    Ok(ReferenceTitle {
        id: row.get(0)?,
        system_tag: row.get(1)?,
        serial_number: row.get(2)?,
        title: row.get(3)?,
        region: row.get(4)?,
    })
}

/// Upsert a library entry. Preserves user data (play_count, custom_title, artwork) on re-scan.
pub fn upsert_library_entry(conn: &Connection, entry: &LibraryEntry) -> Result<()> {
    conn.execute(
        "INSERT INTO user_library
             (local_path, filename, sha1_hash, crc32_hash, file_size,
              serial_number, system_tag, title, region, is_matched, ref_id)
         VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11)
         ON CONFLICT(local_path) DO UPDATE SET
             filename        = excluded.filename,
             sha1_hash       = excluded.sha1_hash,
             crc32_hash      = excluded.crc32_hash,
             file_size       = excluded.file_size,
             serial_number   = excluded.serial_number,
             system_tag      = CASE WHEN excluded.is_matched THEN excluded.system_tag ELSE system_tag END,
             title           = CASE WHEN excluded.is_matched THEN excluded.title       ELSE title       END,
             region          = CASE WHEN excluded.is_matched THEN excluded.region      ELSE region      END,
             is_matched      = excluded.is_matched,
             ref_id          = excluded.ref_id,
             date_updated    = CURRENT_TIMESTAMP",
        params![
            entry.local_path,
            entry.filename,
            entry.sha1_hash,
            entry.crc32_hash,
            entry.file_size.map(|s| s as i64),
            entry.serial_number,
            entry.system_tag,
            entry.title,
            entry.region,
            entry.is_matched as i32,
            entry.ref_id,
        ],
    )?;
    Ok(())
}
