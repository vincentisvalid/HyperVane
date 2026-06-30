use rusqlite::{Connection, Result};

const SCHEMA_V1: &str = r#"
CREATE TABLE IF NOT EXISTS roms (
    id          TEXT PRIMARY KEY,
    title       TEXT NOT NULL,
    platform    TEXT NOT NULL,
    region      TEXT NOT NULL DEFAULT '',
    file_path   TEXT NOT NULL UNIQUE,
    file_size   INTEGER NOT NULL DEFAULT 0,
    crc32       TEXT NOT NULL DEFAULT '',
    sha1        TEXT NOT NULL DEFAULT '',
    verified    INTEGER NOT NULL DEFAULT 0,  -- 0=unknown, 1=matched No-Intro/Redump
    added_at    INTEGER NOT NULL DEFAULT (unixepoch())
);

CREATE VIRTUAL TABLE IF NOT EXISTS roms_fts USING fts5(
    title,
    platform,
    region,
    content='roms',
    content_rowid='rowid'
);

CREATE TRIGGER IF NOT EXISTS roms_ai AFTER INSERT ON roms BEGIN
    INSERT INTO roms_fts(rowid, title, platform, region)
    VALUES (new.rowid, new.title, new.platform, new.region);
END;

CREATE TRIGGER IF NOT EXISTS roms_ad AFTER DELETE ON roms BEGIN
    INSERT INTO roms_fts(roms_fts, rowid, title, platform, region)
    VALUES ('delete', old.rowid, old.title, old.platform, old.region);
END;

CREATE TRIGGER IF NOT EXISTS roms_au AFTER UPDATE ON roms BEGIN
    INSERT INTO roms_fts(roms_fts, rowid, title, platform, region)
    VALUES ('delete', old.rowid, old.title, old.platform, old.region);
    INSERT INTO roms_fts(rowid, title, platform, region)
    VALUES (new.rowid, new.title, new.platform, new.region);
END;

CREATE INDEX IF NOT EXISTS idx_roms_platform ON roms(platform);
CREATE INDEX IF NOT EXISTS idx_roms_crc32    ON roms(crc32);
CREATE INDEX IF NOT EXISTS idx_roms_sha1     ON roms(sha1);
"#;

pub fn apply(conn: &Connection) -> Result<()> {
    conn.execute_batch(SCHEMA_V1)
}
