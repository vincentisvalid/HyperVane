pub mod schema;
pub mod wal;

use rusqlite::{Connection, Result};
use std::sync::{Arc, Mutex};

pub type Pool = Arc<Mutex<Connection>>;

pub fn open(path: &str) -> Result<Pool> {
    let expanded = shellexpand::tilde(path).into_owned();
    if let Some(parent) = std::path::Path::new(&expanded).parent() {
        std::fs::create_dir_all(parent).ok();
    }
    let conn = Connection::open(&expanded)?;
    wal::configure(&conn)?;
    Ok(Arc::new(Mutex::new(conn)))
}

pub fn migrate(pool: &Pool) -> Result<()> {
    let conn = pool.lock().unwrap_or_else(|e| e.into_inner());
    schema::apply(&conn)
}
