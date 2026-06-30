use rusqlite::{Connection, Result};

/// Enable WAL mode and tuning pragmas for concurrent read/write access.
pub fn configure(conn: &Connection) -> Result<()> {
    conn.execute_batch(
        "PRAGMA journal_mode = WAL;
         PRAGMA synchronous  = NORMAL;
         PRAGMA cache_size   = -32000;   -- 32 MB page cache
         PRAGMA temp_store   = MEMORY;
         PRAGMA mmap_size    = 268435456; -- 256 MB mmap
        ",
    )
}
