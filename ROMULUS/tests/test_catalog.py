import csv
from pathlib import Path

from romulus.catalog import Catalog


def test_catalog_import_and_search(tmp_path: Path):
    gamelists = tmp_path / "gamelists"
    gamelists.mkdir()
    csv_path = gamelists / "gameslist (ps1).csv"
    with csv_path.open("w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f, delimiter=";")
        writer.writerow(["Game ID", "Game Name"])
        writer.writerow(['"123"', '"Castlevania: Symphony of the Night"'])
        writer.writerow(['"456"', '"Final Fantasy VII"'])

    db = tmp_path / "catalog.db"
    catalog = Catalog(db)
    count = catalog.import_gamelists(gamelists, platforms=["ps1"])
    assert count == 2
    assert catalog.count("ps1") == 2

    hits = catalog.search("Castlevania", platform="ps1")
    assert hits
    assert "Castlevania" in hits[0].title
    catalog.close()
