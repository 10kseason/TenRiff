"""Generate a small synthetic LR2oraja score database as text for the C++ tests."""
import hashlib
import sqlite3
import textwrap
from pathlib import Path

root = Path(__file__).resolve().parent
chart = root.parent / "lr2" / "import" / "charts" / "proven.bms"
chart_hash = hashlib.sha256(chart.read_bytes()).hexdigest()
failed_hash = "1" * 64

with sqlite3.connect(":memory:") as db:
    db.execute("PRAGMA page_size = 512")
    db.executescript("""
        CREATE TABLE score (
            sha256 TEXT NOT NULL, mode INTEGER, clear INTEGER,
            epg INTEGER, lpg INTEGER, egr INTEGER, lgr INTEGER,
            egd INTEGER, lgd INTEGER, ebd INTEGER, lbd INTEGER,
            epr INTEGER, lpr INTEGER, ems INTEGER, lms INTEGER,
            notes INTEGER, combo INTEGER, minbp INTEGER,
            avgjudge INTEGER NOT NULL DEFAULT 2147483647, ghost TEXT,
            PRIMARY KEY (sha256, mode)
        );
    """)
    rows = [
        (chart_hash.upper(), 0, 6, 40, 38, 10, 8, 2, 1, 0, 0, 1, 0, 3, 2, 100, 72, 6),
        (chart_hash, 1, 7, 45, 43, 6, 5, 1, 0, 0, 0, 0, 0, 0, 0, 100, 100, 1),
        (chart_hash, 2, 8, 48, 44, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0, 100, 100, 0),
        (failed_hash, 0, 1, 2, 4, 1, 1, 0, 0, 1, 1, 3, 2, 2, 1, 30, 6, 2147483647),
        (chart_hash, 10, 9, 40, 38, 10, 8, 2, 1, 0, 0, 1, 0, 3, 2, 100, 72, 6),
        ("z" * 64, 0, 6, 40, 38, 10, 8, 2, 1, 0, 0, 1, 0, 3, 2, 100, 72, 6),
        ("2" * 64, 0, 6, 40, 38, 10, 8, 2, 1, 0, 0, 1, 0, 3, 2, 10, 72, 6),
    ]
    db.executemany(
        "INSERT INTO score (sha256,mode,clear,epg,lpg,egr,lgr,egd,lgd,ebd,lbd,epr,lpr,ems,lms,notes,combo,minbp) "
        "VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)", rows
    )
    db.commit()
    contents = db.serialize()

(root / "score_fixture.hex").write_text("\n".join(textwrap.wrap(contents.hex(), 96)) + "\n", encoding="ascii", newline="\n")
