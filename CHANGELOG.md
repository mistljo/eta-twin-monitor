# Changelog

Alle nennenswerten Änderungen an der Firmware des ETA Twin Monitors.
Versionsnummer = `FW_VERSION` in `config.h`.

---

## Repository – 2026-10-26

Aufgeräumt für die Veröffentlichung (keine Firmware-Änderung):

- Passwörter aus `README.md`, `config.example.h`, `platformio.ini` und
  `ota_upload.ps1` entfernt. Das OTA-Passwort für `pio run -t upload` kommt
  jetzt aus der Umgebungsvariable `ETA_OTA_PASS`.
- Herstellerdateien (ETAmon-Software, Handbuch, Parameter-/Monitorlisten) und
  Fremdskripte aus dem Repository genommen.
- Hilfsskripte nach `tools/` verschoben; Einmal-Skripte mit privaten Pfaden
  entfernt; `ota_upload.ps1` entfernt (der espota-Weg ist zuverlässiger).
- README: Haftungsausschluss, Sicherheitshinweise, keine privaten IP-Adressen.

---

## 2.5.0 – 2026-10-26

Dashboard komplett für Handy und PC überarbeitet (nur CSS/Layout, keine
Funktionsänderung).

- **Handy (bis 700 px):** einspaltig, randlos, größere Tipp-Flächen (Tabs
  48 px, Tasten 40–50 px), Eingabefelder mit 16 px (kein Auto-Zoom auf iOS).
  Die Anlagengrafik zeigt Kessel, Puffer und Boiler nebeneinander (verkleinert),
  darunter die Messwerte in einem 3er-Raster statt als lange Spalte. Die
  runden Aktionstasten stehen in einer Reihe. Die Fußleiste bleibt einzeilig
  (Datum nur auf größeren Bildschirmen). Wertetabellen scrollen seitlich, die
  erste Spalte bleibt stehen. Kein horizontales Scrollen der Seite mehr.
- **Tablet/PC (ab 900 px):** Wertegruppen als Karten in 2 Spalten, ab 1700 px
  in 3 Spalten; maximale Breite 1480 px.
- Der aktive Reiter wird in die Mitte der Reiterleiste gescrollt, beim
  Reiterwechsel springt die Seite nach oben.
- `viewport-fit=cover`, Safe-Area-Abstände (Notch/Gestenleiste), `theme-color`.

---

## 2.4.6 – 2026-10-26

- Dashboard, Reiter Display: Die orange Markierung von „Anzeige ein“ /
  „Anzeige aus“ folgt jetzt dem tatsächlichen Zustand der Anzeige (auch
  beim Öffnen des Reiters). Bei ausgeschalteter Anzeige steht „Anzeige aus“
  statt der Fehlermeldung „Kein Signal“.
- Die Überschrift „Kesseldisplay“ entfällt. Die Rückfragen für Kessel
  Ein/Aus und Entaschen nennen keine Tastenkürzel mehr.

---

## 2.4.5 – 2026-10-26

- Dashboard, Reiter Display: Die Anzeige ist auf Displaybreite begrenzt
  (vorher volle Seitenbreite). Die Tasten stehen darunter in einem
  schmalen Raster ohne Kürzel in Klammern: Auf/Ab, Ja/Zurück, Ändern/Info;
  Kessel Ein/Aus und Entaschen sind abgesetzt und orange umrandet.

---

## 2.4.4 – 2026-10-26

- Dashboard, Reiter Display: Die Tasten sind ausgeschrieben (Auf, Ab, Ja,
  Zurück, Ändern, Info, Kessel Ein/Aus, Entaschen) und zweispaltig
  angeordnet.
- Die gewählte Zeile (am Zeilenende ein „>“ bzw. „<>“) wird farbig
  hervorgehoben; das „>“ selbst wird ausgeblendet.
- Die Anzeige der Rohdaten (Hex) entfällt im Dashboard. `GET /display`
  liefert `raw` unverändert weiter.

---

## 2.4.3 – 2026-10-26

- Dashboard, Reiter Display: Die Tasten sendeten `up`, `down`, `ok`, `back`,
  `change`, `onoff`, `esc`; die Firmware erwartet `auf`, `ab`, `ja`,
  `zurueck`, `aendern`, `kessel`, `entaschen`. Jeder Tastendruck wurde mit
  der Fehlermeldung „Taste: aendern|ja|auf|ab|…“ abgelehnt. Die Tasten
  senden jetzt die richtigen Namen.
- Die Tasten I/O (Kessel Ein/Aus) und E (Entaschen) fragen vor dem Senden
  nach.

---

## 2.4.2 – 2026-10-26

- Das Dashboard (`GET /`) verlangt die Anmeldung (admin + `OTA_PASS`) schon
  beim Öffnen. Seit 2.4.0 verlangten die Tasten der Display-Fernbedienung
  eine Anmeldung, die der Browser bei `fetch()` nicht zuverlässig abfragt;
  die Tastendrücke schlugen dann mit „Tastendruck fehlgeschlagen“ fehl.
  Nach der Anmeldung beim Seitenaufruf sendet der Browser die Zugangsdaten
  bei allen Aufrufen der Seite mit. Lesende Endpunkte (`/api`, `/display`,
  `/set`) bleiben ohne Anmeldung, Home Assistant ist nicht betroffen.
- Dashboard: Antwort 401 beim Tastendruck zeigt „Anmeldung nötig“.

---

## 2.4.1 – 2026-10-25

- `/set`: Der Teiler für `Einschaltverz` war mit 88 aus der Parameterliste
  übernommen und falsch. Am Kessel-Display verifiziert: Anzeige in
  0,1-Stunden-Schritten, Rohwert 600 = 1,0 h, 180 = 0,3 h, also Rohwert =
  Zehntelstunden × 60. Der Wert `val` ist jetzt in Zehntelstunden
  (10 = 1,0 h, 240 = 24,0 h); `GET /set` nennt die Einheit.

---

## 2.4.0 – 2026-10-25

### Schreibpfad für Einstellwerte (`/set`)
- Neues Paket `{MJ len=5 PS Knoten IdxHi IdxLo WertHi WertLo}` zum Setzen
  von Einstellwerten im Kessel (Quelle: Symcon-Thread 16637, Beitrag 48;
  der Kessel bestätigt mit `{MK}`, das die Firmware dem Schreibvorgang
  zuordnet). Rohwert im Kessel = Anzeigewert × Teiler der Parameterliste.
- `GET /set` zeigt Freigabeliste, Limits und Zähler (nur lesend).
  `POST /set?name=Einschaltverz&val=<Minuten>` schreibt (Anmeldung nötig).
- Freigabeliste: derzeit nur `Einschaltverz` (Knoten 16, Index 153,
  Bereich 0–240). Einheit des Werts: **Zehntelstunden** (10 = 1,0 h,
  240 = 24,0 h), Rohwert im Kessel = Zehntelstunden × 60. Am Kessel-Display
  verifiziert (Rohwert 600 = 1,0 h, 180 = 0,3 h). Der Teiler 88 aus der
  Parameterliste `{Mg}` ist bei diesem Eintrag irreführend und wird nicht
  verwendet.
- Schutz des Kesselspeichers (Schreibzyklen sind vom Hersteller nicht
  angegeben): Mindestabstand 10 min je Parameter, höchstens 6 Schreibvorgänge
  je 24 h, kein erneutes Schreiben desselben bestätigten Werts, kein
  Schreiben während einer Listenabfrage. `/api` meldet unter `writes`
  die Zähler `day`, `max_day` und `total`.

### Fehlerbehebungen
- Meldungen im `IM`-Paket (Fehlertext des Kessels) werden auf ASCII
  bereinigt. Ein einzelnes Nicht-ASCII-Byte (z. B. ein Umlaut der
  Kessel-Codepage) machte die `/api`-Antwort zu ungültigem JSON, wodurch
  alle Home-Assistant-Sensoren aus dieser Abfrage ausfielen.
- `dispChar()`: Das Zeichenliteral `'ß'` war ein Mehrzeichen-Literal und
  lieferte das Byte 0x9F statt eines ASCII-Zeichens (Compiler-Warnung
  `-Wmultichar`/`-Woverflow`). `/display` konnte dadurch ungültiges JSON
  ausgeben. Nicht-ASCII-Zeichen der Anzeige werden jetzt zu Leerzeichen.
- WLAN-Fallback-Passwort und OTA-Passwort werden nicht mehr auf der
  seriellen Konsole ausgegeben.

### Sicherheit
- `POST /cmd`, `POST /interval`, `/display?key=…` sowie `POST /scan`,
  `/params`, `/texts` (und jeder Aufruf mit `?start=`) verlangen jetzt
  Anmeldung (Benutzer `admin`, Passwort `OTA_PASS`) **und** den Header
  `X-ETA`. Der Header verhindert Aufrufe von fremden Webseiten (CSRF), weil
  Browser ihn nur nach einem Preflight senden, den der ESP32 nicht beantwortet.
- `/cmd` sendet keinen `Access-Control-Allow-Origin` mehr.
- Lesende Aufrufe (`/api`, `/display`, `/display?on=…`, GET auf `/params`,
  `/texts`, `/scan` ohne `start`) bleiben ohne Anmeldung, damit Home Assistant
  weiter ohne Zugangsdaten liest.
- Das Dashboard sendet den Header bei den ändernden Aufrufen mit; der Browser
  fragt beim ersten Mal nach dem Passwort.

---

## 2.3.0 – 2026-08-15

- WLAN-Watchdog: Rückkehr aus dem Fallback-AP ohne Power-Cycle
  (Commit „WLAN-Watchdog: Rueckkehr aus dem Fallback-AP ohne Power-Cycle“).

---

## 2.2.0 – 2026-08-03

### Messwerte
- `s_params[]` auf **106 Sensoren in 6 Blöcken** erweitert (vorher 49 in 4).
  Neu unter anderem: `Boiler_Soll`, `Puffer_Soll`, `Ruecklauf_Soll`,
  `Abgas_Soll_K`, `Pufferpumpe`, `Boilerpumpe`, `Kollektorpumpe`,
  `Pumpe_MK1`–`MK3`, `Mischer_MK1`–`MK3`, `Vorlauf_Soll_MK1`–`MK3`,
  `Freigabe_MK1`–`MK3`, `Party_MK1`–`MK3`, `Betrieb_MK1`–`MK3`,
  `O2_Ist`, `O2_Soll`, `Leistung_Soll`, `Leistung_Soll_K`,
  `Boiler_Status`, `Puffer_Status`, `Kollektor_Status`, `Solarvorrang`,
  `Solar_Ladung_auf`, `Stoerungsmeldung`, `Holzkessel`, `Anfahrentlastung`,
  `Kesseltuer`, `Wassermangel`, `STB`, `Kesseltemp_Waechter`, `Sicherung_24VAC`
- Doppelten `Ruecklauf_Ist` entfernt
- `0xFFFF` wird zusätzlich zu `0x8000` als ungültig verworfen
- Alle Aufzählungstexte werden aus der Textliste des Kessels aufgelöst
  (`Text[dt + Wert]`, Prozentwerte mit `dt=37` ausgenommen)

### Endpunkte
- `GET/POST /params` – Parameterliste (`{MF}`/`{Mg}`), 405 Einträge, chunked
- `GET/POST /texts` – Textliste (`{MH}`/`{Mi}`), 538 Einträge, chunked
- `/scan`, `/params` und `/texts` liefern die Antwort jetzt blockweise und
  brechen bei zu wenig Heap mit HTTP 507 ab statt zu abstürzen

### Dashboard
- Komplett neu im Stil des ETAtouch-Bedienteils: dunkler Hintergrund,
  ETA-Gelb, gelb umrandeter Rahmen, Statusbalken, Anlagengrafiken mit
  Wertekästchen, runde Aktionsbuttons, Fußzeile mit Außentemperatur und Uhr
- 10 Reiter: Anlage, Kessel, Puffer, Warmwasser, Heizkreise, Solar, Lager,
  Sys, Parameter, Display
- Speicherfarben wie am Kesseldisplay: kalt blau, warm violett, heiß rot
- Anlagengrafik läuft nicht mehr über, Umbruch nur noch unter 700 px
- Messblock-Anzeige in Fußzeile und Sys-Reiter war um 1 zu hoch

### Kompatibilität
- **Kein API-Schlüssel wurde umbenannt oder entfernt**, die Struktur von
  `/api` ist unverändert (`sensors.<Name>.value/unit/age_s/stale`)
- `blocks` steigt von 4 auf 6: ein vollständiger Umlauf dauert bei
  `interval=60` jetzt 6 statt 4 Minuten, `age_s` wird entsprechend größer.
  Eigene Verfügbarkeitsschwellen auf mindestens 420 s anheben oder `stale`
  verwenden, das die Blockzahl bereits berücksichtigt

### Dokumentation
- README: Rahmenformat `len + 6` inklusive Warnung vor `0x7D` in `{Mg}`,
  Byte-Layouts von `{Mb}`/`{Mg}`/`{Mi}`, Enum-Schema `dt + Wert`,
  vollständige Sensor- und Aufzählungstabellen, Dashboard-Kapitel
- Hinweis, dass Wochentagsprogramme der Heizzeiten über `{Mg}` nicht
  erreichbar sind

---

## 2.1.0 – 2026-08-03

### Messwerte
- Monitorliste des Kessels per `/scan` ausgelesen und `s_params[]` neu
  aufgebaut: 49 Parameter in 4 Blöcken, Knoten 8/16/33 statt des nicht
  existierenden Knotens 32
- Zweierkomplement nur noch für Messgrößen (°C, V, mV); Zähler laufen über
  32767 hinaus und werden unsigned gelesen
- Plausibilitätsprüfung für Temperatur, Prozent und Spannung

### Display
- `{II}`/`{IT}`-Protokoll implementiert statt der wirkungslosen
  `{IA}`/`{IB}`-Befehle
- Antwortpaket `IE` wird in 4 Zeilen à 20 Zeichen plus 9 Statusbytes zerlegt
- `/display` mit `?on=0|1` und
  `?key=aendern|ja|auf|ab|zurueck|info|kessel|entaschen`
- Dashboard-Reiter mit Tastenfeld, Auto-Refresh und Hex-Ansicht
- Lesebudget pro Loop, vorreservierte Puffer und Abschaltung nach 2 min
  Inaktivität, damit der Anzeigestrom den Webserver nicht blockiert

### Dokumentation
- README-Abschnitt 6 als vollständige API-Referenz für Fremdsysteme,
  Abschnitt 7 dokumentiert das Display-Protokoll
- Home-Assistant-Teil entfernt, wird in einem eigenen Projekt gepflegt

### Sonstiges
- `dashboard_html.h` wird erstmals versioniert

---

## 2.0.0 – 2026-05-25

- 17 neue `s_params[]`-Einträge in Block 2 und 3:
  Gebläse Soll, Störung, RL-Mischer, Türkontakt, letzter Zustand,
  MK3 komplett (Knoten 33), Stokerschnecke, Kipprost, Zündung aus,
  kg Aschekübel, Freigabe MK1 und MK2
- `MA`/`Mb`-Monitorlisten-Scan: `GET /scan?start=1[&level=1..4]`,
  Ergebnis als JSON-Array über `GET /scan`
- Erster Versuch eines Display-Protokolls über `IA`/`IB`
  (in 2.1.0 durch `{II}`/`{IT}` ersetzt)
- Dashboard: MK3-Kacheln, Baumansicht, Sektion „Diagnose & Aktorik“
- MC-Rotation pausiert während des Scans

---

## 1.9.0 – 2026-05-22

- Erste veröffentlichte Version
- ESP32-basierter Monitor für ETA Pelletskessel über RS232
- Web-Dashboard und JSON-API
- OTA-Update über ArduinoOTA
- `config.example.h` als Vorlage, `config.h` in `.gitignore`
