"""Generate synthetic charts and a score DB; no player records or media."""
import hashlib
import sqlite3
from pathlib import Path

root = Path(__file__).resolve().parent / "import"
root.mkdir(exist_ok=True)
charts = root / "charts"
charts.mkdir(exist_ok=True)
base = "#TITLE Synthetic import\n#BPM 165\n#RANK 2\n#TOTAL 300\n#WAV01 missing.wav\n#BMP01 missing.png\n#00001:01\n#00004:01\n"
taps = "".join(f"#{m:03}11:{'01' * 50}\n" for m in range(1, 5))
cases = {
    "proven": (base + taps, 200, 200, 0, "E200Z"),
    "failed": (base + taps + "#SUBTITLE Failed\n", 200, 180, 20, "E90A20E90Z"),
    "long": (base + taps + "#00152:0101\n", 201, 201, 0, "E201Z"),
    "mine": (base + taps + "#001D2:01\n", 200, 200, 0, "E200Z"),
    "random": (base + taps + "#RANDOM 1\n#IF 1\n#SUBTITLE Branch\n#ENDIF\n#ENDRANDOM\n", 200, 200, 0, "E200Z"),
    "rank": (base.replace("#RANK 2", "#RANK 4") + taps, 200, 200, 0, "E200Z"),
    "rank_missing": (base.replace("#RANK 2\n", "") + taps, 200, 200, 0, "E200Z"),
    "missing": (base + taps + "#SUBTITLE Missing ghost\n", 200, 200, 0, ""),
    "invalid": (base.replace("#BPM 165", "#BPM 0") + taps, 200, 200, 0, "E200Z"),
}
db = root / "score.db"
if db.exists():
    raise SystemExit("Fixture already exists; review before regenerating.")
with sqlite3.connect(db) as conn:
    conn.execute("CREATE TABLE score(hash TEXT PRIMARY KEY, clear INTEGER, perfect INTEGER, great INTEGER, good INTEGER, bad INTEGER, poor INTEGER, totalnotes INTEGER, ghost TEXT)")
    for name, (text, notes, perfect, poor, ghost) in cases.items():
        data = text.encode("ascii")
        (charts / (name + ".bms")).write_bytes(data)
        conn.execute("INSERT INTO score VALUES(?,4,?,0,0,0,?,?,?)",
                     (hashlib.md5(data).hexdigest(), perfect, poor, notes, ghost))
