import json, glob, os

rows = {}
for fn in sorted(glob.glob("_scan*.json")):
    try:
        data = json.load(open(fn, encoding="utf-8-sig"))
    except Exception as e:
        print(f"# {fn}: {e}")
        continue
    if isinstance(data, dict):
        data = data.get("rows", [])
    for r in data:
        k = r.get("k", 0)
        if not k:
            continue
        idx = (r.get("h", 0) << 8) | r.get("l", 0)
        name = (r.get("n") or "").strip()
        if not name:
            continue
        key = (k, idx, name)
        if key not in rows:
            rows[key] = r

print(f"# {len(rows)} eindeutige Eintraege\n")
for (k, idx, name) in sorted(rows):
    r = rows[(k, idx, name)]
    print(f"k={k:<4} i={idx:<5} dt={r.get('dt',0):<5} t={r.get('t',0):<5} e={r.get('ie',0)}  {name}")
