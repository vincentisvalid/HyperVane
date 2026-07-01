-- Immutable reference index populated by the catalog builder.
-- Sources: No-Intro (cartridge ROMs), Redump (disc images).
-- Never modified after initial import — treat as read-only at runtime.

CREATE TABLE IF NOT EXISTS reference_titles (
    id            INTEGER PRIMARY KEY AUTOINCREMENT,
    system_tag    TEXT,          -- e.g. "PS1", "SNES", "GBA"
    serial_number TEXT,          -- e.g. "SLUS-00157"  (disc systems)
    title         TEXT,          -- canonical game title from the DAT
    region        TEXT,          -- e.g. "USA", "Europe", "Japan"
    sha1_hash     TEXT,          -- 40-char hex SHA-1
    crc32_hash    TEXT           -- 8-char hex CRC-32
);

-- Primary lookup path: hash-based identity for cartridge ROMs
CREATE UNIQUE INDEX IF NOT EXISTS idx_sha1
    ON reference_titles(sha1_hash) WHERE sha1_hash IS NOT NULL;

CREATE INDEX IF NOT EXISTS idx_hashes
    ON reference_titles(sha1_hash, crc32_hash);

-- Secondary lookup path: serial-based identity for disc systems
CREATE INDEX IF NOT EXISTS idx_serial
    ON reference_titles(serial_number) WHERE serial_number IS NOT NULL;
