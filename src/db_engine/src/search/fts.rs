use rusqlite::{Connection, Result};
use crate::proto::{RomEntry, SearchRequest, SearchResponse};

/// Wraps each whitespace-delimited token in FTS5 phrase quotes so that user
/// input cannot inject FTS5 operators (AND / OR / NOT / *).
/// Empty input returns an empty string, which the caller interprets as
/// "no FTS filter — return all rows".
fn fts_phrase(raw: &str) -> String {
    raw.split_whitespace()
        .map(|t| format!("\"{}\"", t.replace('"', "")))
        .collect::<Vec<_>>()
        .join(" ")
}

fn row_to_entry(row: &rusqlite::Row<'_>) -> rusqlite::Result<RomEntry> {
    Ok(RomEntry {
        id:        row.get(0)?,
        title:     row.get(1)?,
        platform:  row.get(2)?,
        region:    row.get(3)?,
        file_path: row.get(4)?,
        file_size: row.get::<_, i64>(5)? as u64,
        crc32:     row.get(6)?,
        sha1:      row.get(7)?,
        verified:  row.get::<_, i32>(8)? != 0,
    })
}

pub fn search_fts(conn: &Connection, req: &SearchRequest) -> Result<SearchResponse> {
    let limit  = if req.limit  == 0 { 50 } else { req.limit  } as i64;
    let offset = req.offset as i64;
    let safe_q = fts_phrase(&req.query);
    let use_fts        = !safe_q.is_empty();
    let has_platform   = !req.platform.is_empty();

    let (results, total_hits) = match (use_fts, has_platform) {
        // ── FTS search, no platform filter ────────────────────────────────────
        (true, false) => {
            let total: i64 = conn.query_row(
                "SELECT COUNT(*) FROM roms_fts
                 JOIN roms r ON roms_fts.rowid = r.rowid
                 WHERE roms_fts MATCH ?1",
                rusqlite::params![&safe_q],
                |r| r.get(0),
            )?;
            let mut stmt = conn.prepare_cached(
                "SELECT r.id, r.title, r.platform, r.region,
                        r.file_path, r.file_size, r.crc32, r.sha1, r.verified
                 FROM roms_fts JOIN roms r ON roms_fts.rowid = r.rowid
                 WHERE roms_fts MATCH ?1
                 ORDER BY rank LIMIT ?2 OFFSET ?3",
            )?;
            let rows = stmt
                .query_map(rusqlite::params![&safe_q, limit, offset], row_to_entry)?
                .collect::<Result<Vec<_>>>()?;
            (rows, total as u32)
        }

        // ── FTS search + platform filter ──────────────────────────────────────
        (true, true) => {
            let total: i64 = conn.query_row(
                "SELECT COUNT(*) FROM roms_fts
                 JOIN roms r ON roms_fts.rowid = r.rowid
                 WHERE roms_fts MATCH ?1 AND r.platform = ?2",
                rusqlite::params![&safe_q, &req.platform],
                |r| r.get(0),
            )?;
            let mut stmt = conn.prepare_cached(
                "SELECT r.id, r.title, r.platform, r.region,
                        r.file_path, r.file_size, r.crc32, r.sha1, r.verified
                 FROM roms_fts JOIN roms r ON roms_fts.rowid = r.rowid
                 WHERE roms_fts MATCH ?1 AND r.platform = ?2
                 ORDER BY rank LIMIT ?3 OFFSET ?4",
            )?;
            let rows = stmt
                .query_map(rusqlite::params![&safe_q, &req.platform, limit, offset], row_to_entry)?
                .collect::<Result<Vec<_>>>()?;
            (rows, total as u32)
        }

        // ── Full library, platform filter only ────────────────────────────────
        (false, true) => {
            let total: i64 = conn.query_row(
                "SELECT COUNT(*) FROM roms WHERE platform = ?1",
                rusqlite::params![&req.platform],
                |r| r.get(0),
            )?;
            let mut stmt = conn.prepare_cached(
                "SELECT id, title, platform, region,
                        file_path, file_size, crc32, sha1, verified
                 FROM roms WHERE platform = ?1
                 ORDER BY title LIMIT ?2 OFFSET ?3",
            )?;
            let rows = stmt
                .query_map(rusqlite::params![&req.platform, limit, offset], row_to_entry)?
                .collect::<Result<Vec<_>>>()?;
            (rows, total as u32)
        }

        // ── Full library, no filters (initial load) ───────────────────────────
        (false, false) => {
            let total: i64 =
                conn.query_row("SELECT COUNT(*) FROM roms", [], |r| r.get(0))?;
            let mut stmt = conn.prepare_cached(
                "SELECT id, title, platform, region,
                        file_path, file_size, crc32, sha1, verified
                 FROM roms ORDER BY title LIMIT ?1 OFFSET ?2",
            )?;
            let rows = stmt
                .query_map(rusqlite::params![limit, offset], row_to_entry)?
                .collect::<Result<Vec<_>>>()?;
            (rows, total as u32)
        }
    };

    Ok(SearchResponse { results, total_hits })
}
