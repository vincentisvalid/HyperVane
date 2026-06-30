use hypervane_db::{db, ipc};
use tracing_subscriber::EnvFilter;

const SOCKET_PATH: &str = "/tmp/hypervane-db.sock";
const DB_PATH: &str = "~/.local/share/hypervane/library.db";

#[tokio::main]
async fn main() -> anyhow::Result<()> {
    tracing_subscriber::fmt()
        .with_env_filter(EnvFilter::from_default_env())
        .init();

    let pool = db::open(DB_PATH)?;
    db::migrate(&pool)?;

    tracing::info!("DB engine listening on {SOCKET_PATH}");
    ipc::serve(SOCKET_PATH, pool).await
}
