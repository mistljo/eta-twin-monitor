"""
export_eta_data.py
==================
Liest alle aktuellen Messwerte vom ETA-Monitor-ESP32 über die HTTP-API
und exportiert sie als JSON und XML (mit Zeitstempel).

Verwendung:
    python export_eta_data.py                    # IP aus eta_config.ini
    python export_eta_data.py 192.168.1.50       # IP direkt angeben
    python export_eta_data.py --loop 60          # alle 60 s wiederholen

Ausgabe:
    eta_export_YYYY-MM-DD_HH-MM-SS.json
    eta_export_YYYY-MM-DD_HH-MM-SS.xml
"""

import argparse
import json
import sys
import time
import configparser
import os
from datetime import datetime
from pathlib import Path
from urllib.request import urlopen
from urllib.error import URLError, HTTPError
import xml.etree.ElementTree as ET
from xml.dom import minidom


# ---------------------------------------------------------------------------
#  Konfiguration
# ---------------------------------------------------------------------------

CONFIG_FILE = Path(__file__).parent / "eta_config.ini"
OUTPUT_DIR  = Path(__file__).parent / "exports"

TIMEOUT_S   = 10          # HTTP-Timeout in Sekunden
DEFAULT_PORT = 80


def load_config() -> str:
    """Liest die ESP32-IP aus eta_config.ini (Abschnitt [eta], Schlüssel ip)."""
    cfg = configparser.ConfigParser()
    cfg.read(CONFIG_FILE, encoding="utf-8")
    return cfg.get("eta", "ip", fallback=None)


def save_config(ip: str) -> None:
    """Schreibt die IP in eta_config.ini für spätere Aufrufe."""
    cfg = configparser.ConfigParser()
    cfg.read(CONFIG_FILE, encoding="utf-8")
    if "eta" not in cfg:
        cfg["eta"] = {}
    cfg["eta"]["ip"] = ip
    with open(CONFIG_FILE, "w", encoding="utf-8") as f:
        cfg.write(f)
    print(f"  IP gespeichert in {CONFIG_FILE}")


# ---------------------------------------------------------------------------
#  Datenabruf
# ---------------------------------------------------------------------------

def fetch_api(ip: str, port: int = DEFAULT_PORT) -> dict:
    """Ruft http://<ip>/api ab und gibt das geparste JSON zurück."""
    url = f"http://{ip}:{port}/api"
    print(f"  Verbinde mit {url} …", end=" ", flush=True)
    try:
        with urlopen(url, timeout=TIMEOUT_S) as resp:
            raw = resp.read().decode("utf-8")
            data = json.loads(raw)
            print("OK")
            return data
    except HTTPError as e:
        print(f"FEHLER (HTTP {e.code})")
        raise
    except URLError as e:
        print(f"FEHLER ({e.reason})")
        raise
    except json.JSONDecodeError as e:
        print(f"FEHLER (JSON ungültig: {e})")
        raise


# ---------------------------------------------------------------------------
#  JSON-Export
# ---------------------------------------------------------------------------

def save_json(data: dict, filepath: Path) -> None:
    with open(filepath, "w", encoding="utf-8") as f:
        json.dump(data, f, ensure_ascii=False, indent=2)
    print(f"  JSON  → {filepath}")


# ---------------------------------------------------------------------------
#  XML-Export
# ---------------------------------------------------------------------------

def save_xml(data: dict, filepath: Path, timestamp: str) -> None:
    """Konvertiert die API-Antwort in ein strukturiertes XML."""
    root = ET.Element("EtaExport")
    root.set("timestamp", timestamp)
    root.set("online", str(data.get("online", False)).lower())
    root.set("uptime_s", str(data.get("uptime_s", 0)))
    root.set("last_data_s", str(data.get("last_data_s", -1)))

    sensors_el = ET.SubElement(root, "Sensors")
    sensors_el.set("count", str(len(data.get("sensors", {}))))

    sensors: dict = data.get("sensors", {})
    for key, entry in sorted(sensors.items()):
        s = ET.SubElement(sensors_el, "Sensor")
        s.set("name", key)
        value = entry.get("value")
        unit  = entry.get("unit", "")
        s.set("unit", unit)
        s.set("valid", "false" if value is None else "true")
        s.text = "" if value is None else str(value)

    # Hübsch formatiert ausgeben
    raw_xml = ET.tostring(root, encoding="unicode")
    pretty  = minidom.parseString(raw_xml).toprettyxml(indent="  ", encoding=None)
    # minidom fügt <?xml ...?> hinzu – das behalten wir
    with open(filepath, "w", encoding="utf-8") as f:
        f.write(pretty)
    print(f"  XML   → {filepath}")


# ---------------------------------------------------------------------------
#  Zusammenfassung auf Konsole
# ---------------------------------------------------------------------------

def print_summary(data: dict) -> None:
    sensors = data.get("sensors", {})
    valid   = [(k, v) for k, v in sensors.items() if v.get("value") is not None]
    invalid = [(k, v) for k, v in sensors.items() if v.get("value") is None]

    print(f"\n  {'Parameter':<30} {'Wert':>10}  Einheit")
    print(f"  {'-'*50}")
    for key, entry in sorted(valid):
        val  = entry["value"]
        unit = entry.get("unit", "")
        val_str = f"{val:.1f}" if isinstance(val, float) else str(val)
        print(f"  {key:<30} {val_str:>10}  {unit}")

    if invalid:
        print(f"\n  --- Noch keine Daten ({len(invalid)} Parameter) ---")
        for key, _ in sorted(invalid):
            print(f"  {key}")

    print(f"\n  Online: {'JA' if data.get('online') else 'NEIN'}")
    print(f"  Uptime: {data.get('uptime_s', 0) // 3600} h "
          f"{(data.get('uptime_s', 0) % 3600) // 60} min")
    last = data.get("last_data_s", -1)
    print(f"  Letzte Daten: {last} s ago" if last >= 0 else "  Letzte Daten: noch keine")
    print(f"  Sensoren gesamt: {len(sensors)}  |  gültig: {len(valid)}")


# ---------------------------------------------------------------------------
#  Hauptprogramm
# ---------------------------------------------------------------------------

def run_once(ip: str, port: int) -> bool:
    """Einen Abruf durchführen. Gibt True bei Erfolg zurück."""
    ts      = datetime.now()
    ts_str  = ts.strftime("%Y-%m-%d_%H-%M-%S")
    ts_iso  = ts.isoformat()

    OUTPUT_DIR.mkdir(exist_ok=True)
    base = OUTPUT_DIR / f"eta_export_{ts_str}"

    try:
        data = fetch_api(ip, port)
    except Exception:
        return False

    # Zeitstempel in Daten einbetten
    data["export_timestamp"] = ts_iso

    save_json(data, base.with_suffix(".json"))
    save_xml(data,  base.with_suffix(".xml"), ts_iso)
    print_summary(data)
    return True


def main() -> None:
    global OUTPUT_DIR
    parser = argparse.ArgumentParser(
        description="ETA Monitor – Daten exportieren (JSON + XML)"
    )
    parser.add_argument(
        "ip",
        nargs="?",
        help="IP-Adresse des ESP32 (z. B. 192.168.1.50). "
             "Wird in eta_config.ini gespeichert.",
    )
    parser.add_argument(
        "--port", type=int, default=DEFAULT_PORT,
        help=f"HTTP-Port (Standard: {DEFAULT_PORT})",
    )
    parser.add_argument(
        "--loop", type=int, metavar="SEKUNDEN",
        help="Wiederholt den Export alle N Sekunden (0 = einmalig)",
    )
    parser.add_argument(
        "--outdir", type=str,
        help=f"Ausgabeverzeichnis (Standard: {OUTPUT_DIR})",
    )
    args = parser.parse_args()

    if args.outdir:
        OUTPUT_DIR = Path(args.outdir)

    # IP ermitteln
    ip = args.ip or load_config()
    if not ip:
        ip = input("IP-Adresse des ESP32 eingeben: ").strip()
        if not ip:
            print("FEHLER: Keine IP-Adresse angegeben.", file=sys.stderr)
            sys.exit(1)

    # IP speichern damit nächstes Mal kein Argument nötig
    save_config(ip)

    print(f"\n=== ETA Monitor Export  ({datetime.now().strftime('%d.%m.%Y %H:%M:%S')}) ===")

    if args.loop:
        print(f"  Modus: Loop alle {args.loop} Sekunden (Ctrl+C zum Beenden)\n")
        try:
            while True:
                run_once(ip, args.port)
                print(f"\n  Warte {args.loop} s …\n")
                time.sleep(args.loop)
        except KeyboardInterrupt:
            print("\n  Beendet.")
    else:
        success = run_once(ip, args.port)
        sys.exit(0 if success else 1)


if __name__ == "__main__":
    main()
