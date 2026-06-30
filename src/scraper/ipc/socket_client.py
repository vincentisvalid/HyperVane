"""Unix domain socket server — receives scrape requests from the UI."""

from __future__ import annotations

import asyncio
import logging
import struct
from pathlib import Path
from typing import TYPE_CHECKING

import hypervane_pb2 as pb

if TYPE_CHECKING:
    from scraper.ytdlp_worker import YtdlpWorker, ScrapeJob

log = logging.getLogger(__name__)

_FRAME_HEADER = struct.Struct(">I")  # 4-byte big-endian length prefix


class IPCSocketServer:
    def __init__(self, socket_path: str, worker: YtdlpWorker) -> None:
        self._socket_path = socket_path
        self._worker = worker
        self._server: asyncio.Server | None = None

    async def serve_forever(self) -> None:
        Path(self._socket_path).unlink(missing_ok=True)
        self._server = await asyncio.start_unix_server(
            self._handle_client, path=self._socket_path
        )
        async with self._server:
            await self._server.serve_forever()

    async def close(self) -> None:
        if self._server:
            self._server.close()
            await self._server.wait_closed()

    async def _handle_client(
        self,
        reader: asyncio.StreamReader,
        writer: asyncio.StreamWriter,
    ) -> None:
        peer = writer.get_extra_info("peername", "<unknown>")
        log.info("Client connected: %s", peer)
        try:
            async for envelope in self._read_envelopes(reader):
                await self._dispatch(envelope, writer)
        except asyncio.IncompleteReadError:
            pass
        finally:
            writer.close()
            await writer.wait_closed()
            log.info("Client disconnected: %s", peer)

    async def _dispatch(
        self,
        envelope: pb.Envelope,
        writer: asyncio.StreamWriter,
    ) -> None:
        if envelope.HasField("search_request"):
            req = envelope.search_request
            log.debug("Search request: query=%s", req.query)
            # Forward to DB engine via its own socket (not yet wired)

    @staticmethod
    async def _read_envelopes(reader: asyncio.StreamReader):
        while True:
            header = await reader.readexactly(_FRAME_HEADER.size)
            (length,) = _FRAME_HEADER.unpack(header)
            payload = await reader.readexactly(length)
            env = pb.Envelope()
            env.ParseFromString(payload)
            yield env
