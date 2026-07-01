#!/usr/bin/env python3
"""
Mock Socolite social daemon for testing HyperVane's social features.

Provides fake friends list, user search, chat relay, and activity feed.
Communicates via newline-delimited JSON over /tmp/hypervane-social.sock.

Usage:
  python3 scripts/mock_social_server.py
"""

import asyncio
import json
import random
import time
from pathlib import Path

SOCKET_PATH = "/tmp/hypervane-social.sock"

FAKE_USERS = [
    {"id": "u001", "username": "alice", "display_name": "Alice", "status": "online"},
    {"id": "u002", "username": "bob", "display_name": "Bob", "status": "ingame", "game_title": "Super Mario World"},
    {"id": "u003", "username": "charlie", "display_name": "Charlie", "status": "away"},
    {"id": "u004", "username": "diana", "display_name": "Diana", "status": "online"},
    {"id": "u005", "username": "eve", "display_name": "Eve", "status": "offline"},
    {"id": "u006", "username": "frank", "display_name": "Frank", "status": "online"},
    {"id": "u007", "username": "grace", "display_name": "Grace", "status": "busy"},
    {"id": "u008", "username": "henry", "display_name": "Henry", "status": "online"},
    {"id": "u009", "username": "iris", "display_name": "Iris", "status": "ingame", "game_title": "The Legend of Zelda"},
    {"id": "u010", "username": "jack", "display_name": "Jack", "status": "offline"},
]

# The first time a client connects, send them a friends list
FRIEND_IDS = ["u001", "u002", "u003", "u004", "u005"]


def error_response(message: str) -> dict:
    return {"type": "error", "message": message}


class SocialClient:
    def __init__(self, reader: asyncio.StreamReader, writer: asyncio.StreamWriter):
        self.reader = reader
        self.writer = writer
        self.user_id = "local"
        self.username = "local"

    async def send(self, data: dict) -> None:
        line = json.dumps(data, ensure_ascii=False) + "\n"
        self.writer.write(line.encode())
        await self.writer.drain()

    async def handle(self) -> None:
        print("[social] client connected")
        # Send initial friends list
        friends = [u for u in FAKE_USERS if u["id"] in FRIEND_IDS]
        await self.send({"type": "friends_list", "friends": friends})
        print(f"[social] → sent {len(friends)} friends")

        # Send some initial activities
        activities = [
            {"type": "activity", "activity_type": 0, "user_id": "u002",
             "username": "bob", "message": "Bob started playing Super Mario World",
             "detail": "SNES", "timestamp": "2026-06-30T10:30:00"},
            {"type": "activity", "activity_type": 1, "user_id": "u001",
             "username": "alice", "message": "Alice stopped playing Metroid",
             "detail": "", "timestamp": "2026-06-30T10:15:00"},
            {"type": "activity", "activity_type": 2, "user_id": "u004",
             "username": "diana", "message": "Diana shared a screenshot",
             "detail": "Castlevania: SOTN", "timestamp": "2026-06-30T09:45:00"},
            {"type": "activity", "activity_type": 4, "user_id": "u003",
             "username": "charlie", "message": "Charlie came online",
             "detail": "", "timestamp": "2026-06-30T09:30:00"},
        ]
        for act in activities:
            await self.send(act)
        print(f"[social] → sent {len(activities)} activities")

        # Message history for friend u001 (alice)
        history = [
            {"type": "chat_message", "msg_id": "m1", "from_id": "u001",
             "from_name": "Alice", "text": "Hey! Want to play some SNES?",
             "timestamp": "2026-06-30T09:00:00"},
            {"type": "chat_message", "msg_id": "m2", "from_id": "local",
             "from_name": "You", "text": "Sure! Let me set it up",
             "timestamp": "2026-06-30T09:01:00"},
            {"type": "chat_message", "msg_id": "m3", "from_id": "u001",
             "from_name": "Alice", "text": "I'll host. Join when ready!",
             "timestamp": "2026-06-30T09:02:00"},
        ]
        for h in history:
            await self.send(h)
        print(f"[social] → sent {len(history)} chat messages")

        try:
            buffer = ""
            while True:
                chunk = await self.reader.read(4096)
                if not chunk:
                    break
                buffer += chunk.decode()
                while "\n" in buffer:
                    line, buffer = buffer.split("\n", 1)
                    if not line.strip():
                        continue
                    await self.handle_message(json.loads(line))
        except (asyncio.IncompleteReadError, ConnectionError):
            pass
        finally:
            print("[social] client disconnected")

    async def handle_message(self, msg: dict) -> None:
        msg_type = msg.get("type")
        print(f"[social] ← received: {msg_type}")

        if msg_type == "heartbeat":
            await self.send({"type": "pong"})

        elif msg_type == "add_friend":
            username = msg.get("username", "")
            user = next((u for u in FAKE_USERS if u["username"] == username), None)
            if user:
                await self.send({"type": "friend_added", "user": user})
                print(f"         → added {username}")
            else:
                await self.send(error_response(f"User '{username}' not found"))

        elif msg_type == "remove_friend":
            uid = msg.get("user_id", "")
            await self.send({"type": "friend_removed", "user_id": uid})
            print(f"         → removed {uid}")

        elif msg_type == "set_status":
            status = msg.get("status", "online")
            game = msg.get("game_title", "")
            print(f"         → status={status} game={game}")
            # Broadcast to all friends
            for fid in FRIEND_IDS:
                update = {
                    "type": "status_changed",
                    "user_id": "local",
                    "status": status,
                    "game_title": game,
                }
                await self.send(update)

        elif msg_type == "chat_message":
            to_id = msg.get("to_id", "")
            text = msg.get("text", "")
            image_b64 = msg.get("image", "")
            relay = {
                "type": "chat_message",
                "msg_id": f"m{random.randint(100, 999)}",
                "from_id": "local",
                "from_name": "You",
                "text": text,
                "timestamp": time.strftime("%Y-%m-%dT%H:%M:%S"),
            }
            if image_b64:
                relay["image"] = image_b64
            await self.send(relay)
            print(f"         → relayed message to {to_id}")

        elif msg_type == "search_users":
            query = msg.get("query", "").lower()
            results = [
                u for u in FAKE_USERS if query in u["username"].lower()
            ]
            await self.send({
                "type": "search_results",
                "results": results,
                "query": query,
            })
            print(f"         → {len(results)} results for '{query}'")

        elif msg_type == "post_activity":
            act_type = msg.get("activity_type", 0)
            message = msg.get("message", "")
            detail = msg.get("detail", "")
            broadcast = {
                "type": "activity",
                "activity_type": act_type,
                "user_id": "local",
                "username": "You",
                "message": message,
                "detail": detail,
                "timestamp": time.strftime("%Y-%m-%dT%H:%M:%S"),
            }
            image_b64 = msg.get("image", "")
            if image_b64:
                broadcast["image"] = image_b64
            await self.send(broadcast)
            print(f"         → broadcast activity: {message}")

        elif msg_type == "set_avatar":
            print(f"         → avatar updated ({len(msg.get('image', ''))} bytes)")


async def main() -> None:
    Path(SOCKET_PATH).unlink(missing_ok=True)
    server = await asyncio.start_unix_server(
        lambda r, w: asyncio.create_task(SocialClient(r, w).handle()),
        path=SOCKET_PATH,
    )
    print(f"[social] Socolite mock server listening on {SOCKET_PATH}")
    async with server:
        await server.serve_forever()


if __name__ == "__main__":
    asyncio.run(main())
