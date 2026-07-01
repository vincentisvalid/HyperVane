from __future__ import annotations

import csv
import html
import re
import sqlite3
from dataclasses import dataclass
from pathlib import Path

from rapidfuzz import fuzz, process

from . import GAMELIST_FILES
from .config import decode_title

SCHEMA = """
CREATE TABLE IF NOT EXISTS games (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    game_id TEXT NOT NULL,
    title TEXT NOT NULL,
    title_norm TEXT NOT NULL,
    platform TEXT NOT NULL,
    source TEXT NOT NULL DEFAULT 'gamelist',
    UNIQUE(game_id, platform)
);

CREATE VIRTUAL TABLE IF NOT EXISTS games_fts USING fts5(
    title,
    platform,
    content='games',
    content_rowid='id'
);

CREATE TRIGGER IF NOT EXISTS games_ai AFTER INSERT ON games BEGIN
    INSERT INTO games_fts(rowid, title, platform) VALUES (new.id, new.title, new.platform);
END;
CREATE TRIGGER IF NOT EXISTS games_ad AFTER DELETE ON games BEGIN
    INSERT INTO games_fts(games_fts, rowid, title, platform)
    VALUES ('delete', old.id, old.title, old.platform);
END;
CREATE TRIGGER IF NOT EXISTS games_au AFTER UPDATE ON games BEGIN
    INSERT INTO games_fts(games_fts, rowid, title, platform)
    VALUES ('delete', old.id, old.title, old.platform);
    INSERT INTO games_fts(rowid, title, platform) VALUES (new.id, new.title, new.platform);
END;
"""


@dataclass
class CatalogEntry:
    game_id: str
    title: str
    platform: str
    score: float = 1.0


def _norm(title: str) -> str:
    text = decode_title(title).lower()
    text = re.sub(r"[^\w\s]", " ", text)
    return re.sub(r"\s+", " ", text).strip()


class Catalog:
    def __init__(self, db_path: Path):
        self.db_path = db_path
        self.conn = sqlite3.connect(db_path)
        self.conn.row_factory = sqlite3.Row
        self.conn.executescript(SCHEMA)

    def close(self) -> None:
        self.conn.close()

    def import_gamelists(self, gamelists_dir: Path, platforms: list[str] | None = None) -> int:
        targets = platforms or list(GAMELIST_FILES)
        total = 0
        for platform in targets:
            path = gamelists_dir / GAMELIST_FILES[platform]
            if not path.exists():
                continue
            total += self._import_csv(path, platform)
        self.conn.commit()
        self.conn.execute("INSERT INTO games_fts(games_fts) VALUES('rebuild')")
        self.conn.commit()
        return total

    def _import_csv(self, path: Path, platform: str) -> int:
        count = 0
        with path.open(newline="", encoding="utf-8") as f:
            reader = csv.DictReader(f, delimiter=";")
            for row in reader:
                game_id = (row.get("Game ID") or "").strip().strip('"')
                title = decode_title((row.get("Game Name") or "").strip().strip('"'))
                if not game_id or not title:
                    continue
                self.conn.execute(
                    """INSERT OR IGNORE INTO games (game_id, title, title_norm, platform)
                       VALUES (?, ?, ?, ?)""",
                    (game_id, title, _norm(title), platform),
                )
                count += 1
        return count

    def count(self, platform: str | None = None) -> int:
        if platform:
            row = self.conn.execute(
                "SELECT COUNT(*) FROM games WHERE platform = ?", (platform,)
            ).fetchone()
        else:
            row = self.conn.execute("SELECT COUNT(*) FROM games").fetchone()
        return int(row[0])

    def search(
        self,
        query: str,
        platform: str | None = None,
        limit: int = 20,
    ) -> list[CatalogEntry]:
        query = query.strip()
        if not query:
            return []

        fts_query = " ".join(f'"{part}"' for part in query.split())
        sql = """
            SELECT g.game_id, g.title, g.platform,
                   bm25(games_fts) AS rank
            FROM games_fts
            JOIN games g ON g.id = games_fts.rowid
            WHERE games_fts MATCH ?
        """
        params: list = [fts_query]
        if platform:
            sql += " AND g.platform = ?"
            params.append(platform)
        sql += " ORDER BY rank LIMIT ?"
        params.append(limit)

        rows = self.conn.execute(sql, params).fetchall()
        if rows:
            return [
                CatalogEntry(
                    game_id=r["game_id"],
                    title=r["title"],
                    platform=r["platform"],
                    score=max(0.0, 1.0 + float(r["rank"]) / 20),
                )
                for r in rows
            ]

        return self._fuzzy_search(query, platform, limit)

    def _fuzzy_search(
        self, query: str, platform: str | None, limit: int
    ) -> list[CatalogEntry]:
        sql = "SELECT game_id, title, platform, title_norm FROM games"
        params: list = []
        if platform:
            sql += " WHERE platform = ?"
            params.append(platform)
        rows = self.conn.execute(sql, params).fetchall()
        if not rows:
            return []

        norm_q = _norm(query)
        choices = [(r["title_norm"], i) for i, r in enumerate(rows)]
        hits = process.extract(
            norm_q,
            [c[0] for c in choices],
            scorer=fuzz.token_sort_ratio,
            limit=limit,
        )

        results: list[CatalogEntry] = []
        for _, score, idx in hits:
            r = rows[idx]
            results.append(
                CatalogEntry(
                    game_id=r["game_id"],
                    title=r["title"],
                    platform=r["platform"],
                    score=score / 100,
                )
            )
        return results

    def list_platform(self, platform: str, amount: int | None = None) -> list[CatalogEntry]:
        sql = "SELECT game_id, title, platform FROM games WHERE platform = ? ORDER BY title"
        params: list = [platform]
        if amount:
            sql += " LIMIT ?"
            params.append(amount)
        rows = self.conn.execute(sql, params).fetchall()
        return [
            CatalogEntry(game_id=r["game_id"], title=r["title"], platform=r["platform"])
            for r in rows
        ]
