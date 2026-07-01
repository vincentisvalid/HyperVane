-- Mutable user inventory.  Written by the matcher; enriched by the enrichment worker.
-- Separates clean global reference data from per-user state (play history, custom tags, artwork).

CREATE TABLE IF NOT EXISTS user_library (
    id                  INTEGER PRIMARY KEY AUTOINCREMENT,

    -- Identity
    ref_id              INTEGER,            -- FK → reference_titles.id (NULL if unmatched)
    local_path          TEXT NOT NULL UNIQUE,
    filename            TEXT,

    -- Hashes / identifiers from the scanner
    sha1_hash           TEXT,
    crc32_hash          TEXT,
    file_size           INTEGER,
    serial_number       TEXT,

    -- Catalog metadata (populated on match)
    system_tag          TEXT,
    title               TEXT,
    region              TEXT,
    is_matched          INTEGER NOT NULL DEFAULT 0,

    -- User-specific state
    play_count          INTEGER NOT NULL DEFAULT 0,
    last_played         TEXT,
    custom_title        TEXT,
    custom_tags         TEXT,               -- comma-separated or JSON array

    -- Artwork paths (filled by enrichment worker)
    cover_art_path      TEXT,
    background_art_path TEXT,
    video_preview_path  TEXT,

    -- External API metadata
    description         TEXT,
    developer           TEXT,
    publisher           TEXT,
    release_date        TEXT,
    screenscraper_id    TEXT,

    -- Timestamps
    date_added          TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    date_updated        TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX IF NOT EXISTS idx_lib_sha1
    ON user_library(sha1_hash) WHERE sha1_hash IS NOT NULL;

CREATE INDEX IF NOT EXISTS idx_lib_serial
    ON user_library(serial_number) WHERE serial_number IS NOT NULL;

CREATE INDEX IF NOT EXISTS idx_lib_system
    ON user_library(system_tag);

CREATE INDEX IF NOT EXISTS idx_lib_matched
    ON user_library(is_matched);

-- Auto-update date_updated on any row change
CREATE TRIGGER IF NOT EXISTS trg_lib_updated
AFTER UPDATE ON user_library
BEGIN
    UPDATE user_library SET date_updated = CURRENT_TIMESTAMP WHERE id = NEW.id;
END;
