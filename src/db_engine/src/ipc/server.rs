use std::path::Path;

use prost::Message;
use tokio::io::{AsyncReadExt, AsyncWriteExt};
use tokio::net::{UnixListener, UnixStream};
use tracing::warn;

use crate::db::Pool;
use crate::proto::{Envelope, envelope::Payload, SearchResponse};
use crate::search::search_fts;

// Reject frames larger than 64 MiB — prevents OOM from malformed clients.
const MAX_MSG_LEN: usize = 64 * 1024 * 1024;

pub async fn serve(socket_path: &str, pool: Pool) -> anyhow::Result<()> {
    let _ = std::fs::remove_file(socket_path);
    let listener = UnixListener::bind(socket_path)?;

    loop {
        let (stream, _) = listener.accept().await?;
        let pool = pool.clone();
        tokio::spawn(async move {
            if let Err(e) = handle(stream, pool).await {
                warn!("client handler error: {e}");
            }
        });
    }
}

async fn handle(mut stream: UnixStream, pool: Pool) -> anyhow::Result<()> {
    loop {
        let mut len_buf = [0u8; 4];
        if stream.read_exact(&mut len_buf).await.is_err() {
            break;
        }
        let length = u32::from_be_bytes(len_buf) as usize;

        if length > MAX_MSG_LEN {
            anyhow::bail!("frame length {length} exceeds maximum {MAX_MSG_LEN}");
        }

        let mut payload = vec![0u8; length];
        stream.read_exact(&mut payload).await?;

        let env = Envelope::decode(payload.as_slice())?;
        let Some(Payload::SearchRequest(req)) = env.payload else {
            continue;
        };

        let resp: SearchResponse = {
            // unwrap_or_else recovers from a poisoned mutex instead of panicking.
            let conn = pool.lock().unwrap_or_else(|e| e.into_inner());
            search_fts(&conn, &req)?
        };

        let reply = Envelope {
            payload: Some(Payload::SearchResponse(resp)),
        };
        let mut buf = Vec::with_capacity(reply.encoded_len() + 4);
        buf.extend_from_slice(&(reply.encoded_len() as u32).to_be_bytes());
        reply.encode(&mut buf)?;
        stream.write_all(&buf).await?;
    }
    Ok(())
}
