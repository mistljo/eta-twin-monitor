/*
 * ETA_Monitor.ino
 * ---------------
 * Liest Messwerte eines ETA-Pelletkessels (Twin, PE, PU, ...)
 * über die RS232-Schnittstelle aus und stellt sie als
 *   • Web-Dashboard  (http://<IP>/)
 *   • JSON-API       (http://<IP>/api)
 * bereit.
 *
 * Hardware:
 *   eByte ECM50-A (ESP32-basiert) an COM9 (USB, nur zum Flashen)
 *   RS232-Verbindung zum ETA-Kessel über UART2 (Pins → config.h)
 *   ACHTUNG: ESP32 arbeitet mit 3,3 V; ETA RS232 mit ±12 V
 *            → MAX3232 (oder ähnlichen) Level-Shifter zwischenschalten,
 *              sofern das ECM50-A keinen onboard RS232-Transceiver hat.
 *
 * Protokoll (Quelle: ETA / Symcon-Community):
 *   • Pakete sind mit { ... } eingeklammert
 *   • MC-Befehl startet zyklische Übertragung
 *   • MD-Pakete  → Messwerte
 *   • MK-Pakete  → Einstellwerte
 *   • IM-Pakete  → Fehlermeldungen
 *
 * Bibliotheken (Arduino Library Manager):
 *   – Arduino-ESP32 Board-Support (espressif/arduino-esp32)
 *   – Keine zusätzlichen Libs nötig (WebServer ist im ESP32-Core)
 *
 * Erste Schritte:
 *   1. config.h öffnen und WLAN-Zugangsdaten + Pins eintragen
 *   2. In Arduino IDE: Board "ESP32 Dev Module" (oder passendes ECM50-A Board)
 *      Port COM9 wählen, flashen
 *   3. Seriellen Monitor bei 115200 Baud öffnen → IP-Adresse ablesen
 *   4. Browser: http://<IP>/
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoOTA.h>
#include <Update.h>
#include <esp_ota_ops.h>
#include "config.h"

// ============================================================
//  Datenstrukturen
// ============================================================

struct EtaParam {
    uint8_t     node;        // ETA-Bus Node-Nummer (16=Kessel, 32/33=Heizungserw.)
    uint8_t     index;       // Monitorlisten-Index (LByte) laut ETA-Protokoll
    uint8_t     block;       // MC-Block, in dem dieser Wert abgefragt wird
    const char* name;        // Anzeigename (ASCII → wird 1:1 zum JSON-Key)
    const char* unit;        // Einheit
    float       scale;       // Teiler für Rohwert  (Spalte "Teiler" der Monitorliste)
    float       value;       // Letzter gültiger Wert
    bool        valid;       // Wurde mindestens ein Wert empfangen?
    uint32_t    stampMs;     // Zeitpunkt des letzten Empfangs
};

// Parameter die vom Kessel angefordert werden.
//
// ALLE Knoten/Index/Teiler unten stammen aus der Monitorliste, die dieser
// Kessel selbst über den /scan-Endpunkt gemeldet hat (Mb-Pakete).
// Es wird NICHTS mehr geraten – geratene Indizes liefern Müllwerte
// (z. B. "Kesselrücklauf 1046 °C" oder "Leistung 632 %").
//
// WICHTIG: Ein MC-Befehl darf max. 20 Parameter enthalten (ETA-Limit) und
// mischt keine Knoten – deshalb ist jeder Block genau einem Knoten zugeordnet.
#define ETA_BLOCK_SIZE  20
#define ETA_NUM_BLOCKS  6

#define U_C   "\xC2\xB0""C"     // °C
#define U_PCT "%"
#define U_NON ""

static EtaParam s_params[] = {
    // ── Block 0 · Knoten 8 · Kesselregelung / Hydraulik ───────
    { ETA_NODE_KESSEL,   8, 0, "Kessel",            U_C,     10.0f },
    { ETA_NODE_KESSEL,   9, 0, "Kessel_Ruecklauf",  U_C,     10.0f },
    { ETA_NODE_KESSEL,  15, 0, "Abgas_Kessel",      U_C,     10.0f },
    { ETA_NODE_KESSEL,  13, 0, "Boiler",            U_C,     10.0f },
    { ETA_NODE_KESSEL,  12, 0, "Puffer_oben",       U_C,     10.0f },
    { ETA_NODE_KESSEL,  11, 0, "Puffer_mitte",      U_C,     10.0f },
    { ETA_NODE_KESSEL,  10, 0, "Puffer_unten",      U_C,     10.0f },
    { ETA_NODE_KESSEL,  75, 0, "Pufferladung",      U_PCT,    1.0f },
    { ETA_NODE_KESSEL,  21, 0, "Boardtemp_Kessel",  U_C,     10.0f },
    { ETA_NODE_KESSEL, 148, 0, "Boardtemp_max_K",   U_C,     10.0f },
    { ETA_NODE_KESSEL,  33, 0, "Abgasgeblaese_Lz",  "h",      1.0f },
    { ETA_NODE_KESSEL,  55, 0, "Wartung_Kessel",    "h",      1.0f },
    { ETA_NODE_KESSEL,  37, 0, "Uebertemp_Zaehler", U_NON,    1.0f },
    { ETA_NODE_KESSEL,  92, 0, "RL_tief_Warnung",   U_NON,    1.0f },
    { ETA_NODE_KESSEL, 110, 0, "Boiler_Soll",       U_C,     10.0f },
    { ETA_NODE_KESSEL,  76, 0, "Puffer_Soll",       U_C,     10.0f },
    { ETA_NODE_KESSEL, 109, 0, "Boiler_Extraladung",U_NON,    1.0f },
    { ETA_NODE_KESSEL,  25, 0, "Pufferpumpe",       U_PCT,    1.0f },
    { ETA_NODE_KESSEL,  24, 0, "Boilerpumpe",       U_PCT,    1.0f },
    { ETA_NODE_KESSEL,  71, 0, "Boiler_Status",     U_NON,    1.0f },

    // ── Block 1 · Knoten 16 · Pelletskessel, Verbrennung ──────
    { ETA_NODE_PELLET,  45, 1, "Betriebszustand",   U_NON,    1.0f },
    { ETA_NODE_PELLET,  20, 1, "Kessel_Pellet",     U_C,     10.0f },
    { ETA_NODE_PELLET,  69, 1, "Kessel_Soll",       U_C,     10.0f },
    { ETA_NODE_PELLET,  19, 1, "Abgas",             U_C,     10.0f },
    { ETA_NODE_PELLET,  59, 1, "Abgas_Soll",        U_C,     10.0f },
    { ETA_NODE_PELLET,  28, 1, "Geblaesedrehzahl",  "U/min",  1.0f },
    { ETA_NODE_PELLET, 143, 1, "Geblaese_Soll",     U_PCT,    1.0f },
    { ETA_NODE_PELLET,  18, 1, "Luftschieber",      U_PCT,   10.0f },
    { ETA_NODE_PELLET,  54, 1, "Luftschieber_Soll", U_PCT,    1.0f },
    { ETA_NODE_PELLET,  67, 1, "Restsauerstoff",    U_PCT,  100.0f },
    { ETA_NODE_PELLET, 103, 1, "Lambdasonde",       U_NON,    1.0f },
    { ETA_NODE_PELLET,  43, 1, "Entaschung",        U_NON,    1.0f },
    { ETA_NODE_PELLET,  38, 1, "Rostklappe",        U_NON,    1.0f },
    { ETA_NODE_PELLET,  66, 1, "Raumaustragung",    U_NON,    1.0f },
    { ETA_NODE_PELLET,  85, 1, "Saugen",            U_NON,    1.0f },
    { ETA_NODE_PELLET,  34, 1, "Austragschnecke",   U_NON,    1.0f },
    { ETA_NODE_PELLET,  27, 1, "Boardtemp",         U_C,     10.0f },
    { ETA_NODE_PELLET, 128, 1, "Boardtemp_max",     U_C,     10.0f },
    { ETA_NODE_PELLET, 126, 1, "Leistung_Soll",     U_PCT,    1.0f },

    // ── Block 2 · Knoten 16 · Zählerstände & Vorrat ───────────
    { ETA_NODE_PELLET,  53, 2, "Betriebsstunden",   "h",      1.0f },
    { ETA_NODE_PELLET,  68, 2, "Gesamtverbrauch",   "t",    100.0f },
    { ETA_NODE_PELLET,  86, 2, "Pelletsvorrat",     "t",    100.0f },
    { ETA_NODE_PELLET, 171, 2, "Wartung_Verbrauch", "t",    100.0f },
    { ETA_NODE_PELLET,  63, 2, "Behaelterinhalt",   "kg",    10.0f },
    { ETA_NODE_PELLET, 140, 2, "kg_seit_Entasch",   "kg",    10.0f },
    { ETA_NODE_PELLET, 117, 2, "Verbr_seit_Entasch","kg",     1.0f },
    { ETA_NODE_PELLET, 129, 2, "Board_ueber_40",    "h",      1.0f },
    { ETA_NODE_PELLET, 130, 2, "Board_ueber_50",    "h",      1.0f },
    { ETA_NODE_PELLET, 131, 2, "Board_ueber_60",    "h",      1.0f },
    { ETA_NODE_PELLET, 132, 2, "Board_ueber_65",    "h",      1.0f },
    { ETA_NODE_PELLET, 133, 2, "Board_ueber_70",    "h",      1.0f },

    // ── Block 3 · Knoten 33 · Heizkreise & Solar ──────────────
    { ETA_NODE_HK,       0, 3, "Aussen",            U_C,     10.0f },
    { ETA_NODE_HK,       1, 3, "Vorlauf_MK1",       U_C,     10.0f },
    { ETA_NODE_HK,       3, 3, "Raum_MK1",          U_C,     10.0f },
    { ETA_NODE_HK,       2, 3, "Vorlauf_MK2",       U_C,     10.0f },
    { ETA_NODE_HK,       4, 3, "Raum_MK2",          U_C,     10.0f },
    { ETA_NODE_HK,      29, 3, "Vorlauf_MK3",       U_C,     10.0f },
    { ETA_NODE_HK,      31, 3, "Raum_MK3",          U_C,     10.0f },
    { ETA_NODE_HK,      75, 3, "Kollektor",         U_C,     10.0f },
    { ETA_NODE_HK,      89, 3, "Puffer_Solar",      U_C,     10.0f },
    { ETA_NODE_HK,     107, 3, "Boiler_oben_Solar", U_C,     10.0f },
    { ETA_NODE_HK,     145, 3, "Boiler_unten_Solar",U_C,     10.0f },
    { ETA_NODE_HK,     138, 3, "Thermostat_heiss",  U_C,     10.0f },
    { ETA_NODE_HK,     139, 3, "Thermostat_kalt",   U_C,     10.0f },
    { ETA_NODE_HK,      12, 3, "Vorlauf_Soll_MK1",  U_C,     10.0f },
    { ETA_NODE_HK,      13, 3, "Vorlauf_Soll_MK2",  U_C,     10.0f },
    { ETA_NODE_HK,      39, 3, "Vorlauf_Soll_MK3",  U_C,     10.0f },
    { ETA_NODE_HK,     152, 3, "Betrieb_MK1",       U_NON,    1.0f },
    { ETA_NODE_HK,     153, 3, "Betrieb_MK2",       U_NON,    1.0f },
    { ETA_NODE_HK,     154, 3, "Betrieb_MK3",       U_NON,    1.0f },
    { ETA_NODE_HK,     112, 3, "Solarleistung",     U_NON,    1.0f },

    // ── Block 4 · Knoten 8 · Status, Sollwerte, Sicherheit ────
    { ETA_NODE_KESSEL,  72, 4, "Puffer_Status",     U_NON,    1.0f },
    { ETA_NODE_KESSEL, 186, 4, "Ruecklauf_Soll",    U_C,     10.0f },
    { ETA_NODE_KESSEL,  73, 4, "Abgas_Soll_K",      U_C,     10.0f },
    { ETA_NODE_KESSEL, 169, 4, "Leistung_Soll_K",   U_PCT,    1.0f },
    { ETA_NODE_KESSEL,  40, 4, "O2_Ist",            U_PCT,  100.0f },
    { ETA_NODE_KESSEL,  74, 4, "O2_Soll",           U_PCT,  100.0f },
    { ETA_NODE_KESSEL, 155, 4, "Stoerungsmeldung",  U_NON,    1.0f },
    { ETA_NODE_KESSEL,  39, 4, "Holzkessel",        U_NON,    1.0f },
    { ETA_NODE_KESSEL,  29, 4, "Ruecklaufmischer",  U_NON,    1.0f },
    { ETA_NODE_KESSEL, 113, 4, "Anfahrentlastung",  U_NON,    1.0f },
    { ETA_NODE_KESSEL,   3, 4, "Kesseltuer",        U_NON,    1.0f },
    { ETA_NODE_KESSEL,   0, 4, "Wassermangel",      U_NON,    1.0f },
    { ETA_NODE_KESSEL,   1, 4, "STB",               U_NON,    1.0f },
    { ETA_NODE_KESSEL,   2, 4, "Kesseltemp_Waechter",U_NON,   1.0f },
    { ETA_NODE_KESSEL,   4, 4, "Sicherung_24VAC",   U_NON,    1.0f },

    // ── Block 5 · Knoten 33 · Heizkreis-Betrieb & Solar ─────
    { ETA_NODE_HK,     140, 5, "Freigabe_MK1",      U_C,     10.0f },
    { ETA_NODE_HK,     141, 5, "Freigabe_MK2",      U_C,     10.0f },
    { ETA_NODE_HK,     142, 5, "Freigabe_MK3",      U_C,     10.0f },
    { ETA_NODE_HK,      10, 5, "Party_MK1",         U_NON,    1.0f },
    { ETA_NODE_HK,      11, 5, "Party_MK2",         U_NON,    1.0f },
    { ETA_NODE_HK,      37, 5, "Party_MK3",         U_NON,    1.0f },
    { ETA_NODE_HK,       5, 5, "Pumpe_MK1",         U_PCT,    1.0f },
    { ETA_NODE_HK,       6, 5, "Pumpe_MK2",         U_PCT,    1.0f },
    { ETA_NODE_HK,      33, 5, "Pumpe_MK3",         U_PCT,    1.0f },
    { ETA_NODE_HK,       7, 5, "Mischer_MK1",       U_NON,    1.0f },
    { ETA_NODE_HK,       8, 5, "Mischer_MK2",       U_NON,    1.0f },
    { ETA_NODE_HK,      35, 5, "Mischer_MK3",       U_NON,    1.0f },
    { ETA_NODE_HK,      78, 5, "Kollektorpumpe",    U_PCT,    1.0f },
    { ETA_NODE_HK,      88, 5, "Solar_Ladung_auf",  U_NON,    1.0f },
    { ETA_NODE_HK,     105, 5, "Solarvorrang",      U_NON,    1.0f },
    { ETA_NODE_HK,     137, 5, "Thermostat",        U_NON,    1.0f },
    { ETA_NODE_HK,      64, 5, "MK1",               U_NON,    1.0f },
    { ETA_NODE_HK,      65, 5, "MK2",               U_NON,    1.0f },
    { ETA_NODE_HK,      66, 5, "MK3",               U_NON,    1.0f },
    { ETA_NODE_HK,      77, 5, "Kollektor_Status",  U_NON,    1.0f },
};
static const int NUM_PARAMS = (int)(sizeof(s_params) / sizeof(EtaParam));

// Für MK-Einstellwerte (Index → Wert Map, max. 32 Einträge)
struct EtaSetting {
    uint16_t index;
    int16_t  value;
    bool     valid;
};
static EtaSetting s_settings[32];
static int s_settingCount = 0;

// ------------------------------------------------------------
//  Schreibpfad {MJ} - Einstellwerte im Kessel setzen
//
//  Nur Parameter aus dieser Freigabeliste sind schreibbar.
//  Regel (Symcon-Thread 16637, Beitrag 48 und 212): Rohwert im Kessel =
//  Anzeigewert * Teiler (Spalte "Teiler" der Parameterliste {Mg}).
//
//  Der Parameterspeicher im Kessel hat vermutlich nur eine begrenzte Zahl
//  von Schreibzyklen (Herstellerangabe unbekannt). Darum gelten harte
//  Grenzen: Mindestabstand je Parameter und Tageslimit insgesamt.
// ------------------------------------------------------------
struct WriteParam {
    const char* name;         // Name in der Freigabeliste
    uint8_t     node;         // Knoten laut Parameterliste
    uint16_t    index;        // Index laut Parameterliste
    uint16_t    teiler;       // Rohwert = Anzeigewert * Teiler
    int16_t     minVal;       // zulaessiger Anzeigewert (min)
    int16_t     maxVal;       // zulaessiger Anzeigewert (max)
    const char* unit;         // Einheit des Anzeigewerts (nur zur Information)
    uint32_t    lastWriteMs;  // 0 = seit Start nie geschrieben
    int32_t     lastRaw;      // zuletzt gesendeter Rohwert, -1 = keiner
    int32_t     ackRaw;       // zuletzt per MK bestaetigter Rohwert, -1 = keiner
    uint32_t    ackMs;        // Zeitpunkt der letzten MK-Bestaetigung
};
static WriteParam s_writeParams[] = {
    // Menue PELL.KES > Zustandstimer > Einschaltverz. (Verzoegerung bis zum
    // Zuenden nach einer Anforderung, 0 = sofort).
    // Der Teiler der Parameterliste ({Mg}) steht hier faelschlich auf 88.
    // Am Kessel-Display verifiziert (25.10.2026): Anzeige in 0,1-h-Schritten,
    // Rohwert 600 = 1,0 h, Rohwert 180 = 0,3 h. Also Rohwert = Zehntelstunden * 60.
    // Der Anzeigewert (val) ist deshalb in Zehntelstunden: 10 = 1,0 h, 240 = 24,0 h.
    { "Einschaltverz", 16, 153, 60, 0, 240, "0.1 h", 0, -1, -1, 0 },
};
static const int NUM_WRITE_PARAMS = (int)(sizeof(s_writeParams) / sizeof(WriteParam));
#define WRITE_MIN_INTERVAL_MS  600000UL    // je Parameter mind. 10 min Abstand
#define WRITE_MAX_PER_DAY      6           // alle Parameter, rollierend 24 h
#define WRITE_WINDOW_MS        86400000UL
static uint32_t s_writeWindowMs   = 0;     // Start des aktuellen 24-h-Fensters
static uint8_t  s_writeCountDay   = 0;     // Schreibvorgaenge im Fenster
static uint32_t s_writeCountTotal = 0;     // Schreibvorgaenge seit Start

// Monitorlisten-Scan (MA/Mb-Protokoll)
struct MbRow {
    uint8_t knoten, hbyte, lbyte, ueberschrift, datentype;
    uint8_t unk11, unk12, teiler, indexEbene;
    char    name[34];
};
static MbRow*  s_mbRows     = nullptr;
static int     s_mbCap      = 0;
static int     s_mbCount    = 0;
static bool    s_scanActive = false;
static bool    s_scanDone   = false;
static uint8_t s_scanLevel  = 1;           // 1=Standard .. 4=Expert
static unsigned long s_scanLastMs = 0;
static int     s_scanRetries = 0;

// Parameterliste (MF/Mg-Protokoll) – Menüeinstellungen samt Ist-, Min-,
// Max- und Standardwert. Im Gegensatz zur Monitorliste sind das keine
// Messwerte, sondern die am Bedienteil einstellbaren Parameter.
struct MgRow {
    uint8_t  knoten, datentyp, einheit, teiler, ebene;
    uint16_t index;
    int16_t  vmin, vmax, vstd, vakt;
    char     name[43];
};
static MgRow*  s_mgRows     = nullptr;
static int     s_mgCap      = 0;
static int     s_mgCount    = 0;
static bool    s_pgActive   = false;
static bool    s_pgDone     = false;
static uint8_t s_pgLevel    = 1;
static unsigned long s_pgLastMs = 0;
static int     s_pgRetries  = 0;

// Textliste (MH/Mi-Protokoll) – Klartexte zu Zustands- und Auswahlwerten
struct MiRow {
    uint8_t  knoten;
    uint16_t index;
    char     name[43];
};
static MiRow*  s_miRows     = nullptr;
static int     s_miCap      = 0;
static int     s_miCount    = 0;
static bool    s_txActive   = false;
static bool    s_txDone     = false;
static unsigned long s_txLastMs = 0;
static int     s_txRetries  = 0;


// Display-Zustand (II/IT-Protokoll, siehe etaSendDisplayMode())
// Das Bedienteil liefert 4 Zeilen à 20 Zeichen, gefolgt von 9 Statuszeichen.
#define DISP_ROWS        4
#define DISP_COLS       20
static String        s_dispText;
static String        s_dispLines[DISP_ROWS];
static String        s_dispStatus;       // 9 Statuszeichen am Paketende
static String        s_dispRaw;          // Rohpaket als Hex, für Protokollanalyse
static bool          s_dispValid  = false;
static bool          s_dispOn     = false;
static unsigned long s_dispRxMs   = 0;    // Zeitpunkt des letzten Display-Pakets
static unsigned long s_dispOnMs   = 0;    // Zeitpunkt des letzten {II}-Einschaltens

// ============================================================
//  Globaler Zustand
// ============================================================

static WebServer   s_server(HTTP_PORT);
static String      s_rxBuf;
static bool        s_frActive = false;   // Rahmen begonnen ('{' empfangen)
static int         s_frNeed   = -1;      // erwartete Rahmenlänge, -1 = noch unbekannt
static unsigned long s_lastDataMs   = 0;
static unsigned long s_lastMcSentMs = 0;
static unsigned long s_blockDoneMs  = 0;   // Wann wurde der letzte Block fertig?
static unsigned long s_cycleStartMs = 0;   // Beginn des aktuellen Gesamtzyklus
static int         s_activeBlock    = 0;   // Aktuell aktiver MC-Block (0..N-1)
static bool        s_awaitingMd     = false;// MC gesendet, Antwort steht aus
static bool        s_apMode         = false;
static unsigned long s_apSinceMs    = 0;      // seit wann im AP-Modus
static unsigned long s_wifiCheckMs  = 0;      // letzter Watchdog-Durchlauf
static unsigned long s_staLostMs    = 0;      // seit wann STA-Verbindung weg
static uint8_t     s_intervalSec    = ETA_INTERVAL_SEC;  // Laufzeit-einstellbar via /interval
static uint8_t     s_blockSec       = ETA_BLOCK_SEC;     // Intervall im MC-Telegramm
static uint32_t    s_mdCount        = 0;   // Anzahl empfangener MD-Pakete
static uint32_t    s_mdTimeouts     = 0;   // Blöcke ohne Antwort

// ---- Knoten-Präsenz ---------------------------------------
// Fehlt ein Modul (z. B. die Heizungserweiterung), antwortet der Kessel
// mit dem Index als Wert ("Echo"). Das wird hier erkannt, damit keine
// Fantasiewerte wie "Pumpe MK1 = 5 %" im Dashboard landen.
struct NodeState { uint8_t node; bool present; uint8_t strikes; };
static NodeState s_nodes[] = {
    { ETA_NODE_KESSEL, true, 0 },
    { ETA_NODE_PELLET, true, 0 },
    { ETA_NODE_HK,     true, 0 },
};
static const int NUM_NODES = (int)(sizeof(s_nodes) / sizeof(NodeState));

// ---- Letzte Fehlermeldung des Kessels (IM-Telegramm) -------
static String        s_lastError;
static unsigned long s_lastErrorMs = 0;

// ---- Hilfsfunktionen für die Blockaufteilung ---------------
static int blockStart(int b) {
    for (int i = 0; i < NUM_PARAMS; i++) if (s_params[i].block == b) return i;
    return -1;
}
static int blockCount(int b) {
    int n = 0;
    for (int i = 0; i < NUM_PARAMS; i++) if (s_params[i].block == b) n++;
    return n;
}
static uint8_t blockNode(int b) {
    int s = blockStart(b);
    return (s < 0) ? 0 : s_params[s].node;
}
static NodeState* findNode(uint8_t node) {
    for (int i = 0; i < NUM_NODES; i++) if (s_nodes[i].node == node) return &s_nodes[i];
    return nullptr;
}
static bool nodePresent(uint8_t node) {
    NodeState* n = findNode(node);
    return n ? n->present : true;
}

// ---- OTA / Safe-Mode ----------------------------------------
// RTC-Speicher übersteht einen Software-Reset, wird aber bei
// Power-Off gelöscht → perfekt für Double-Reset-Erkennung.
// Zweimaliger Reset innerhalb von 5 s → Safe Mode (nur OTA aktiv).
RTC_DATA_ATTR uint32_t g_resetFlag;
#define RESET_MAGIC         0xA5A5A5UL
static bool s_safeMode         = false;  // true → nur OTA, kein ETA
static bool s_otaValidMarked   = false;  // Firmware als gültig markiert?
static bool s_resetFlagCleared = false;  // RTC-Flag nach 5 s gelöscht?

// ============================================================
//  ETA Protokoll – Senden
// ============================================================

/**
 * Sendet den MC-Befehl für den aktuellen Block (s_activeBlock).
 * ETA-Firmware-Limit: max. ETA_BLOCK_SIZE Parameter pro Befehl.
 * Ein Block enthält immer nur einen Knoten (siehe s_params).
 * Rahmen laut ETA-Doku:
 *   { M C <AnzahlNutzdatenbytes> <Prüfsumme> <Intervall> [Node,IdxHB,IdxLB]* }
 *   Prüfsumme = Summe aller Nutzdatenbytes mod 256
 */
static void etaSendMonitorConfig() {
    if (s_activeBlock >= ETA_NUM_BLOCKS) s_activeBlock = 0;

    int start = blockStart(s_activeBlock);
    int count = blockCount(s_activeBlock);
    if (start < 0 || count <= 0) return;
    if (count > ETA_BLOCK_SIZE) count = ETA_BLOCK_SIZE;

    const uint8_t interval = s_blockSec;

    // Prüfsumme: Interval + Summe(node + hbyte + index) des Blocks mod 256
    uint16_t ps = interval;
    for (int i = start; i < start + count; i++) {
        ps += s_params[i].node + s_params[i].index;
    }
    uint8_t checksum = (uint8_t)(ps & 0xFF);
    uint8_t numBytes = (uint8_t)(1 + count * 3);

    Serial2.write('{');
    Serial2.write('M');
    Serial2.write('C');
    Serial2.write(numBytes);
    Serial2.write(checksum);
    Serial2.write(interval);
    for (int i = start; i < start + count; i++) {
        Serial2.write((uint8_t)s_params[i].node);
        Serial2.write((uint8_t)0);
        Serial2.write((uint8_t)s_params[i].index);
    }
    Serial2.write('}');

    s_lastMcSentMs = millis();
    s_awaitingMd   = true;
    Serial.printf("[ETA] MC Block %d/%d (Node %u): %d Params, Intervall=%u s\n",
                  s_activeBlock + 1, ETA_NUM_BLOCKS, blockNode(s_activeBlock),
                  count, interval);
}

/** Stoppt die zyklische Übertragung (ME-Befehl). */
static void etaSendMonitorStop() {
    Serial2.write('{');
    Serial2.write('M');
    Serial2.write('E');
    Serial2.write((uint8_t)0);
    Serial2.write((uint8_t)0);
    Serial2.write('}');
    Serial.println("[ETA] ME-Befehl (Stop) gesendet");
}

/** Fordert den nächsten Monitorlisten-Eintrag an (MA-Befehl).
 *  level: 1=Standard, 2=Service, 3=Profi, 4=Expert
 *  first=true setzt den Listenzeiger im Kessel auf den Anfang zurück.
 *  Der Ofen antwortet mit einem {Mb ...}-Paket. */
static void etaSendMonitorListRequest(uint8_t level = 1, bool first = false) {
    // {MA [len=2] [PS=pos+level] [pos] [level]}
    uint8_t pos = first ? 0x00 : 0x01;
    uint8_t ps  = (uint8_t)((pos + level) & 0xFFu);
    Serial2.write('{');
    Serial2.write('M');
    Serial2.write('A');
    Serial2.write((uint8_t)2);
    Serial2.write(ps);
    Serial2.write(pos);
    Serial2.write(level);
    Serial2.write('}');
    s_scanLastMs = millis();
}

/** Beendet einen laufenden Listen-Scan (MO-Befehl). */
static void etaSendScanStop() {
    // {MO \x01 \x01}  (gemäß ETA-Protokoll / IPS-PHP-Referenz)
    Serial2.write('{');
    Serial2.write('M');
    Serial2.write('O');
    Serial2.write((uint8_t)1);
    Serial2.write((uint8_t)1);
    Serial2.write('}');
    Serial.println("[SCAN] MO gesendet (Scan beendet)");
}

/** Fordert den nächsten Eintrag der Parameterliste an (MF-Befehl).
 *  first=true beim ersten Aufruf, danach false.
 *  Der Ofen antwortet mit einem {Mg ...}-Paket. */
static void etaSendParamListRequest(uint8_t level, bool first) {
    // {MF [len=2] [PS=pos+level] [pos] [level]}
    uint8_t pos = first ? 0x00 : 0x01;
    uint8_t ps  = (uint8_t)((pos + level) & 0xFFu);
    Serial2.write('{');
    Serial2.write('M');
    Serial2.write('F');
    Serial2.write((uint8_t)2);
    Serial2.write(ps);
    Serial2.write(pos);
    Serial2.write(level);
    Serial2.write('}');
    s_pgLastMs = millis();
}

/** Fordert den nächsten Eintrag der Textliste an (MH-Befehl).
 *  Der Ofen antwortet mit einem {Mi ...}-Paket. */
static void etaSendTextListRequest(bool first) {
    // {MH [len=1] [PS=pos] [pos]}
    uint8_t pos = first ? 0x00 : 0x01;
    Serial2.write('{');
    Serial2.write('M');
    Serial2.write('H');
    Serial2.write((uint8_t)1);
    Serial2.write(pos);
    Serial2.write(pos);
    Serial2.write('}');
    s_txLastMs = millis();
}

/**
 * Schaltet die Display-Übertragung ein/aus.
 * Bytefolge laut Symcon-Community (Thread 16637, Beitrag 147):
 *   ein:  7B 49 49 01 31 31 7D   = {II <len=1> <PS='1'> <'1'>}
 *   aus:  7B 49 49 01 30 30 7D   = {II <len=1> <PS='0'> <'0'>}
 * Danach sendet der Kessel den Anzeigeinhalt von selbst.
 */
static void etaSendDisplayMode(bool on) {
    uint8_t d = on ? '1' : '0';
    Serial2.write('{');
    Serial2.write('I');
    Serial2.write('I');
    Serial2.write((uint8_t)1);
    Serial2.write(d);      // Prüfsumme = einziges Nutzdatenbyte
    Serial2.write(d);
    Serial2.write('}');
    s_dispOn = on;
    if (on) s_dispOnMs = millis();
    Serial.printf("[DISP] {II} Anzeige %s\n", on ? "EIN" : "AUS");
}

/**
 * Drückt eine Taste am Kessel-Bedienteil.
 *   7B 49 54 01 31 <code> 7D  = {IT <len=1> <'1'> <code>}
 * Codes: 0=Ändern 1=Ja 2=Auf 3=Ab 4=Zurück 5=Info 6=Kessel I/O 7=Entaschen
 */
static void etaSendDisplayKey(uint8_t code) {
    Serial2.write('{');
    Serial2.write('I');
    Serial2.write('T');
    Serial2.write((uint8_t)1);
    Serial2.write((uint8_t)'1');
    Serial2.write(code);
    Serial2.write('}');
    Serial.printf("[DISP] {IT} Taste %u gesendet\n", code);
}

/**
 * Sendet einen Heizungssteuer-Befehl (IH-Dienst / SMS-Fernbedienung).
 *
 * VORAUSSETZUNG: Am Kessel unter Heizkreis → SMS_erlauben = EIN setzen!
 *
 * data0-Bits:
 *   0x01 = Heizung Reset   (physischen Schalter verwenden)
 *   0x02 = Heizung Auto    (Schalter auf AUTO)
 *   0x04 = Heizung Tag     (Schalter auf TAG)
 *   0x08 = Heizung Nacht   (Schalter auf NACHT)
 *   0x10 = Kessel Ein      (Kessel einschalten)
 *   0x20 = Kessel Aus      (Kessel ausschalten)
 *   0x40 = Boiler laden    (Boiler jetzt laden)
 *
 * Protokoll: { I H [count=2] [PS=Data0%256] [Data0] [Data1=0] }
 */
static void etaSendHeatingCmd(uint8_t data0) {
    uint8_t checksum = data0;  // PS = Data0 % 256, da Data1 = 0
    Serial2.write('{');
    Serial2.write('I');
    Serial2.write('H');
    Serial2.write((uint8_t)2);   // 2 Nutzdatenbytes
    Serial2.write(checksum);
    Serial2.write(data0);
    Serial2.write((uint8_t)0);   // Data1 = 0
    Serial2.write('}');
    Serial.printf("[ETA] IH-Befehl gesendet: 0x%02X\n", data0);
}

/** Schreibvorgaenge im laufenden 24-h-Fenster (0, wenn das Fenster abgelaufen ist). */
static uint8_t writeCountToday() {
    if (s_writeWindowMs == 0) return 0;
    return (millis() - s_writeWindowMs >= WRITE_WINDOW_MS) ? 0 : s_writeCountDay;
}

/**
 * Setzt einen Einstellwert im Kessel:
 *   { M J <len=5> <PS> <Knoten> <IdxHi> <IdxLo> <WertHi> <WertLo> }
 * PS = Summe der 5 Nutzdatenbytes mod 256 (Symcon-Thread 16637, Beitrag 48).
 * Der Kessel bestaetigt mit einem {MK ...}-Paket (parseMkPacket).
 * Wird nur ueber handleSet() aufgerufen (Freigabeliste und Limits dort).
 */
static void etaSendParamWrite(uint8_t node, uint16_t index, int16_t raw) {
    uint8_t ih = (uint8_t)(index >> 8);
    uint8_t il = (uint8_t)(index & 0xFF);
    uint8_t vh = (uint8_t)(((uint16_t)raw) >> 8);
    uint8_t vl = (uint8_t)(((uint16_t)raw) & 0xFF);
    uint8_t ps = (uint8_t)(node + ih + il + vh + vl);
    Serial2.write('{');
    Serial2.write('M');
    Serial2.write('J');
    Serial2.write((uint8_t)5);   // 5 Nutzdatenbytes
    Serial2.write(ps);
    Serial2.write(node);
    Serial2.write(ih);
    Serial2.write(il);
    Serial2.write(vh);
    Serial2.write(vl);
    Serial2.write('}');
    Serial.printf("[ETA] MJ-Schreiben  Knoten=%u Index=%u Rohwert=%d\n", node, index, raw);
}

// ============================================================
//  ETA Protokoll – Empfangen / Parsen
// ============================================================

/**
 * Zum nächsten Block weiterschalten.
 * Knoten, die als "nicht vorhanden" erkannt wurden, werden nur noch
 * jeden 10. Zyklus abgefragt – so bleibt der Zyklus kurz, ein später
 * nachgerüstetes Modul wird aber trotzdem wieder gefunden.
 */
static uint32_t s_cycleCount = 0;
static void advanceBlock() {
    for (int guard = 0; guard < ETA_NUM_BLOCKS; guard++) {
        s_activeBlock++;
        if (s_activeBlock >= ETA_NUM_BLOCKS) {
            s_activeBlock = 0;
            s_cycleCount++;
        }
        if (nodePresent(blockNode(s_activeBlock)) || (s_cycleCount % 10) == 0) return;
    }
}

/**
 * Plausibilitätsgrenzen anhand der Einheit.
 * Der Kessel liefert für nicht angeschlossene Fühler teils Müllwerte
 * (z. B. 1045.8 °C am Rücklauf). Solche Werte werden verworfen,
 * statt sie im Dashboard anzuzeigen.
 */
static bool valuePlausible(const EtaParam& p, float v) {
    if (strcmp(p.unit, U_C) == 0)   return (v > -60.0f  && v < 350.0f);
    if (strcmp(p.unit, U_PCT) == 0) return (v >= -1.0f  && v <= 110.0f);
    if (strcmp(p.unit, "V") == 0)   return (v >= -1.0f  && v <= 30.0f);
    return true;
}

/**
 * Nur Messgrößen können negativ werden (z. B. Außentemperatur).
 * Zähler (Betriebsstunden, kg, t, Stück) laufen über 32767 hinaus und
 * müssen vorzeichenlos gelesen werden – sonst kommt z. B. -33058 h heraus.
 */
static bool unitIsSigned(const char* unit) {
    return strcmp(unit, U_C) == 0 || strcmp(unit, "V") == 0 || strcmp(unit, "mV") == 0;
}

/**
 * MD-Paket: Messwerte
 * Inhalt zwischen {} ohne die Klammern:
 *   MD | count | checksum | [nodeNr idxHB idxLB valHi valLo] × N
 *   Länge = N×5 + 4
 */
static void parseMdPacket(const String& pkt) {
    int expectedCount = blockCount(s_activeBlock);
    int expectedLen   = expectedCount * 5 + 4;

    // Tolerante Längenprüfung: wir parsen so viele Parameter wie passen
    int numValues = ((int)pkt.length() - 4) / 5;
    if (numValues <= 0) {
        Serial.printf("[ETA] MD zu kurz: %d Bytes\n", pkt.length());
        return;
    }
    if ((int)pkt.length() != expectedLen) {
        Serial.printf("[ETA] MD Länge %d (erwartet %d für Block %d) – parse trotzdem\n",
                      pkt.length(), expectedLen, s_activeBlock + 1);
    }

    uint8_t  pktNode  = 0;
    int      nEntries = 0;   // Einträge dieses Knotens
    int      nEchoes  = 0;   // davon reine Index-Echos

    for (int i = 1; i <= numValues; i++) {
        int base = i * 5;
        if (base + 3 >= (int)pkt.length()) break;

        uint8_t pkt_node = (uint8_t)pkt[base - 1];  // Node-Byte steht VOR dem Entry-Block
        uint8_t idx      = (uint8_t)pkt[base + 1];
        int rawU         = ((uint8_t)pkt[base + 2] << 8) | (uint8_t)pkt[base + 3];

        if (pktNode == 0) pktNode = pkt_node;
        if (pkt_node == pktNode) {
            nEntries++;
            if (rawU == (int)idx) nEchoes++;
        }

        // ETA: Rohwert 0x8000 → kein Sensor angeschlossen, 0xFFFF → Wert ungültig
        if (rawU == 0x8000 || rawU == 0xFFFF) continue;

        // Passenden Parameter suchen und Wert speichern (Node + Index müssen passen)
        for (int j = 0; j < NUM_PARAMS; j++) {
            if (s_params[j].node == pkt_node && s_params[j].index == idx) {
                int raw = rawU;
                if (unitIsSigned(s_params[j].unit) && raw >= 30000) raw -= 65536;
                float v = (float)raw / s_params[j].scale;
                if (!valuePlausible(s_params[j], v)) {
                    Serial.printf("[ETA] %-22s unplausibel (%.1f) – verworfen\n",
                                  s_params[j].name, v);
                    break;
                }
                s_params[j].value   = v;
                s_params[j].valid   = true;
                s_params[j].stampMs = millis();
                Serial.printf("[ETA] %-22s = %7.1f %s\n",
                              s_params[j].name, v, s_params[j].unit);
                break;
            }
        }
    }

    // Echo-Erkennung: liefert der Kessel für (fast) alle Einträge eines
    // Knotens nur den Index zurück, existiert dieser Knoten nicht.
    NodeState* ns = findNode(pktNode);
    if (ns && nEntries >= 4) {
        if (nEchoes >= nEntries - 1) {
            if (ns->strikes < 3) ns->strikes++;
            if (ns->strikes >= 2 && ns->present) {
                ns->present = false;
                Serial.printf("[ETA] Knoten %u antwortet nur mit Index-Echo → "
                              "Modul nicht vorhanden, Werte werden ausgeblendet\n", pktNode);
                for (int j = 0; j < NUM_PARAMS; j++)
                    if (s_params[j].node == pktNode) s_params[j].valid = false;
            }
        } else {
            ns->strikes = 0;
            if (!ns->present) {
                ns->present = true;
                Serial.printf("[ETA] Knoten %u liefert wieder echte Werte\n", pktNode);
            }
        }
    }

    s_lastDataMs = millis();
    s_mdCount++;
}

/**
 * MK-Paket: Einstellwerte (z. B. Soll-Temperaturen vom Bedienpult)
 * Inhalt: MK ... idx_hi idx_lo val_hi val_lo ... (Position 5-8)
 */
static void parseMkPacket(const String& pkt) {
    if ((int)pkt.length() < 9) return;
    uint16_t idx = ((uint8_t)pkt[5] << 8) | (uint8_t)pkt[6];
    int16_t  val = (int16_t)(((uint8_t)pkt[7] << 8) | (uint8_t)pkt[8]);
    Serial.printf("[ETA] MK-Wert  Index=%-5d Wert=%d\n", idx, val);

    // Bestaetigung eines {MJ}-Schreibvorgangs erkennen
    uint8_t node = (uint8_t)pkt[4];
    for (int i = 0; i < NUM_WRITE_PARAMS; i++) {
        if (s_writeParams[i].node == node && s_writeParams[i].index == idx) {
            s_writeParams[i].ackRaw = val;
            s_writeParams[i].ackMs  = millis();
        }
    }

    // In der kleinen Settings-Map speichern
    for (int i = 0; i < s_settingCount; i++) {
        if (s_settings[i].index == idx) {
            s_settings[i].value = val;
            return;
        }
    }
    if (s_settingCount < 32) {
        s_settings[s_settingCount++] = { idx, val, true };
    }
}

/**
 * IM-Paket: Fehlermeldung vom Kessel
 * Inhalt: IM ab Position 4
 */
static void parseImPacket(const String& pkt) {
    String msg = pkt.length() > 4 ? pkt.substring(4) : "(leer)";
    // Steuerzeichen und Nicht-ASCII-Bytes ersetzen: Die Meldung landet in /api
    // (JSON). Die Codepage des Kessels ist nicht UTF-8; ein einzelnes
    // Byte > 0x7E (z. B. ein Umlaut) macht das JSON ungueltig und damit alle
    // Home-Assistant-Sensoren aus dieser Antwort unbrauchbar.
    for (int i = 0; i < (int)msg.length(); i++) {
        uint8_t c = (uint8_t)msg[i];
        if (c < 0x20 || c == '"' || c == '\\') msg.setCharAt(i, ' ');
        else if (c > 0x7E)                    msg.setCharAt(i, '?');
    }
    msg.trim();
    s_lastError   = msg;
    s_lastErrorMs = millis();
    Serial.printf("[ETA] FEHLER: %s\n", msg.c_str());
}

/** Mb-Paket: Eine Zeile der Monitorliste (Antwort auf MA-Befehl).
 *  Byte-Layout (pkt ohne { }):
 *    [0]='M' [1]='b' [2]=len [3]=PS [4]=Knoten [5]=HByte [6]=LByte
 *    [7]=Ueberschrift [8]=Datentype [9]=Unk11 [10]=Unk12
 *    [11]=Teiler [12]=IndexEbene [13..45]=Name (max. 33 Zeichen)
 *  Ende-Marker: len=0 und PS=0 */
static void parseMbPacket(const String& pkt) {
    if (!s_scanActive) return;
    s_scanRetries = 0;
    // Ende-Marker: len=0, PS=0
    if ((uint8_t)pkt[2] == 0 && (uint8_t)pkt[3] == 0) {
        s_scanDone   = true;
        s_scanActive = false;
        etaSendScanStop();
        s_awaitingMd   = false;   // MC-Ablauf sofort neu starten
        s_blockDoneMs  = 0;
        s_cycleStartMs = 0;
        Serial.printf("[SCAN] Fertig \xe2\x80\x93 %d Eintr\xC3\xA4ge\n", s_mbCount);
        return;
    }
    if ((int)pkt.length() < 13) {
        etaSendMonitorListRequest(s_scanLevel);
        return;
    }
    if (s_mbCount < s_mbCap) {
        MbRow& r       = s_mbRows[s_mbCount++];
        r.knoten       = (uint8_t)pkt[4];
        r.hbyte        = (uint8_t)pkt[5];
        r.lbyte        = (uint8_t)pkt[6];
        r.ueberschrift = (uint8_t)pkt[7];
        r.datentype    = (uint8_t)pkt[8];
        r.unk11        = (uint8_t)pkt[9];
        r.unk12        = (uint8_t)pkt[10];
        r.teiler       = (uint8_t)pkt[11];
        r.indexEbene   = (uint8_t)pkt[12];
        int nlen = min((int)pkt.length() - 13, 33);
        memset(r.name, 0, sizeof(r.name));
        for (int i = 0; i < nlen; i++) {
            uint8_t c = (uint8_t)pkt[13 + i];
            r.name[i] = (c >= 0x20 && c <= 0x7E) ? (char)c : '_';
        }
        for (int i = nlen - 1; i >= 0 && (uint8_t)r.name[i] <= 0x20; i--) r.name[i] = '\0';
        Serial.printf("[SCAN] #%3d K=%3d L=%3d T=%3d '%s'\n",
                      s_mbCount, r.knoten, r.lbyte, r.teiler, r.name);
    }
    etaSendMonitorListRequest(s_scanLevel);
}

/** Mg-Paket: Eine Zeile der Parameterliste (Antwort auf MF-Befehl).
 *  Byte-Layout (pkt ohne { }):
 *    [0]='M' [1]='g' [2]=len [3]=PS [4]=Knoten [5]=Index_H [6]=Index_L
 *    [7]=Datentyp [8]=Einheit [9]=unbek [10]=unbek [11]=Teiler
 *    [12..13]=Min [14..15]=Max [16..17]=Standard [18..19]=Istwert
 *    [20]=Ebene [21..]=Name (max. 42 Zeichen)
 *  Ende-Marker: len=0 und PS=0 */
static void parseMgPacket(const String& pkt) {
    if (!s_pgActive) return;
    s_pgRetries = 0;
    if ((uint8_t)pkt[2] == 0 && (uint8_t)pkt[3] == 0) {
        s_pgDone   = true;
        s_pgActive = false;
        etaSendScanStop();
        s_awaitingMd   = false;
        s_blockDoneMs  = 0;
        s_cycleStartMs = 0;
        Serial.printf("[PARAM] Fertig - %d Eintraege\n", s_mgCount);
        return;
    }
    if ((int)pkt.length() < 21) {
        etaSendParamListRequest(s_pgLevel, false);
        return;
    }
    if (s_mgCount < s_mgCap) {
        MgRow& r    = s_mgRows[s_mgCount++];
        r.knoten    = (uint8_t)pkt[4];
        r.index     = (uint16_t)(((uint8_t)pkt[5] << 8) | (uint8_t)pkt[6]);
        r.datentyp  = (uint8_t)pkt[7];
        r.einheit   = (uint8_t)pkt[8];
        r.teiler    = (uint8_t)pkt[11];
        r.vmin      = (int16_t)(((uint8_t)pkt[12] << 8) | (uint8_t)pkt[13]);
        r.vmax      = (int16_t)(((uint8_t)pkt[14] << 8) | (uint8_t)pkt[15]);
        r.vstd      = (int16_t)(((uint8_t)pkt[16] << 8) | (uint8_t)pkt[17]);
        r.vakt      = (int16_t)(((uint8_t)pkt[18] << 8) | (uint8_t)pkt[19]);
        r.ebene     = (uint8_t)pkt[20];
        int nlen = min((int)pkt.length() - 21, 42);
        memset(r.name, 0, sizeof(r.name));
        for (int i = 0; i < nlen; i++) {
            uint8_t c = (uint8_t)pkt[21 + i];
            r.name[i] = (c >= 0x20 && c <= 0x7E) ? (char)c : '_';
        }
        for (int i = nlen - 1; i >= 0 && (uint8_t)r.name[i] <= 0x20; i--) r.name[i] = '\0';
        if (s_mgCount <= 5 || (s_mgCount % 25) == 0) {
            Serial.printf("[PARAM] #%3d K=%3d I=%5u akt=%6d '%s'\n",
                          s_mgCount, r.knoten, r.index, r.vakt, r.name);
        }
    }
    etaSendParamListRequest(s_pgLevel, false);
}

/** Mi-Paket: Klartext zu einem Zustands-/Auswahlwert (Antwort auf MH).
 *  Byte-Layout (pkt ohne { }):
 *    [0]='M' [1]='i' [2]=len [3]=PS [4]=Knoten [5]=Index_H [6]=Index_L
 *    [7..]=Text (max. 42 Zeichen)
 *  Ende-Marker: len=0 und PS=0 */
static void parseMiPacket(const String& pkt) {
    if (!s_txActive) return;
    s_txRetries = 0;
    if ((uint8_t)pkt[2] == 0 && (uint8_t)pkt[3] == 0) {
        s_txDone   = true;
        s_txActive = false;
        etaSendScanStop();
        s_awaitingMd   = false;
        s_blockDoneMs  = 0;
        s_cycleStartMs = 0;
        Serial.printf("[TEXT] Fertig - %d Eintraege\n", s_miCount);
        return;
    }
    if ((int)pkt.length() < 7) {
        etaSendTextListRequest(false);
        return;
    }
    if (s_miCount < s_miCap) {
        MiRow& r = s_miRows[s_miCount++];
        r.knoten = (uint8_t)pkt[4];
        r.index  = (uint16_t)(((uint8_t)pkt[5] << 8) | (uint8_t)pkt[6]);
        int nlen = min((int)pkt.length() - 7, 42);
        memset(r.name, 0, sizeof(r.name));
        for (int i = 0; i < nlen; i++) {
            uint8_t c = (uint8_t)pkt[7 + i];
            r.name[i] = (c >= 0x20 && c <= 0x7E) ? (char)c : '_';
        }
        for (int i = nlen - 1; i >= 0 && (uint8_t)r.name[i] <= 0x20; i--) r.name[i] = '\0';
    }
    etaSendTextListRequest(false);
}

/** Zeichen des ETA-Bedienteils in druckbares ASCII übersetzen.
 *  Der Kessel verwendet eine eigene Codepage; bekannt ist bisher nur
 *  0xB3 als Auswahl-Cursor. Alles andere ausserhalb von ASCII (auch
 *  Umlaute) wird zu einem Leerzeichen, damit /display gueltiges JSON bleibt.
 *  (Frueher stand hier case 0xDF: return 'ß' - ein Mehrzeichen-Literal,
 *  das bei UTF-8-Quelltext zu 0x9F wurde und das JSON ungueltig machte.) */
static char dispChar(uint8_t c) {
    if (c >= 0x20 && c <= 0x7E) return (char)c;
    switch (c) {
        case 0xB3: return '>';   // Cursor der aktuellen Zeile
        default:   return ' ';
    }
}

/** Antwortpaket des Bedienteils (Kennung 'IE').
 *  Aufbau: 2 Byte Kennung, 1 Byte Länge, 1 Byte Prüfsumme,
 *  danach DISP_ROWS x DISP_COLS Anzeigezeichen und DISP_STATUS_LEN
 *  Statuszeichen. Das Rohpaket wird zusätzlich als Hex abgelegt. */
static void parseDisplayPacket(const String& pkt) {
    // Solange die Anzeige nicht angefordert wurde, Paket verwerfen.
    // Der Kessel sendet im Anzeigemodus mehrmals pro Sekunde; ohne diese
    // Bremse fragmentiert der Heap und der Webserver wird unerreichbar.
    if (!s_dispOn) return;

    int n = pkt.length(); if (n > 128) n = 128;
    char hex[2 * 128 + 1];
    for (int i = 0; i < n; i++) {
        static const char* H = "0123456789ABCDEF";
        hex[2 * i]     = H[((uint8_t)pkt[i]) >> 4];
        hex[2 * i + 1] = H[((uint8_t)pkt[i]) & 0x0F];
    }
    hex[2 * n] = 0;
    s_dispRaw = hex;

    // Kennung (2) + Länge (1) + Prüfsumme (1) überspringen
    String body = (pkt.length() > 4) ? pkt.substring(4) : String("");
    for (int i = 0; i < (int)body.length(); i++) {
        body.setCharAt(i, dispChar((uint8_t)body[i]));
    }

    String txt;
    txt.reserve(DISP_ROWS * (DISP_COLS + 1));
    for (int r = 0; r < DISP_ROWS; r++) {
        int from = r * DISP_COLS;
        s_dispLines[r] = (from < (int)body.length())
                       ? body.substring(from, min(from + DISP_COLS, (int)body.length()))
                       : String("");
        if (r) txt += '\n';
        txt += s_dispLines[r];
    }
    s_dispText = txt;

    int stFrom = DISP_ROWS * DISP_COLS;
    s_dispStatus = (stFrom < (int)body.length()) ? body.substring(stFrom) : String("");

    s_dispValid = true;
    s_dispRxMs  = millis();
}

/** Wertet ein vollständig empfangenes Paket aus (ohne { und }). */
static void dispatchPacket(const String& pkt) {
    if (pkt.startsWith("MD")) {
        parseMdPacket(pkt);
        // Block ist beantwortet → Übertragung stoppen und zum
        // nächsten Block weiterschalten (so wie es die
        // ETA-Referenzimplementierungen machen).
        if (s_awaitingMd) {
            s_awaitingMd  = false;
            s_blockDoneMs = millis();
            etaSendMonitorStop();
            advanceBlock();
        }
    } else if (pkt.startsWith("MK")) {
        parseMkPacket(pkt);
    } else if (pkt.startsWith("IM")) {
        parseImPacket(pkt);
    } else if (pkt.startsWith("Mb")) {
        parseMbPacket(pkt);
    } else if (pkt.startsWith("Mg")) {
        parseMgPacket(pkt);
    } else if (pkt.startsWith("Mi")) {
        parseMiPacket(pkt);
    } else if (pkt.length() >= 2 && (pkt[0] == 'I' || pkt[0] == 'i')) {
        parseDisplayPacket(pkt);
    } else if (pkt.length() > 0) {
        // Unbekanntes Paket vollst\xC3\xA4ndig als Hex + ASCII ausgeben
        Serial.printf("[ETA] Unbekannt [%d B]: ", pkt.length());
        for (int _i = 0; _i < min((int)pkt.length(), 24); _i++)
            Serial.printf("%02X ", (uint8_t)pkt[_i]);
        Serial.printf(" | ");
        for (int _i = 0; _i < min((int)pkt.length(), 24); _i++) {
            char _c = pkt[_i]; Serial.print((_c >= 0x20 && _c < 0x7F) ? _c : '.');
        }
        Serial.println();
    }
}

/** Wird im loop() aufgerufen – liest Zeichen von Serial2 und sammelt
 *  komplette Pakete.
 *
 *  Wichtig: Die Rahmenlänge wird aus dem Längenbyte bestimmt, nicht aus
 *  der schließenden Klammer. Nutzdaten (z. B. Min/Max/Istwerte in
 *  {Mg}-Paketen) können das Byte 0x7D ('}') enthalten und würden ein
 *  rein zeichenbasiertes Framing zerreißen.
 *    Aufbau: '{' Kennung(2) Länge(1) Prüfsumme(1) Nutzdaten(Länge) '}'
 */
static void etaProcessSerial() {
    // Pro Aufruf begrenzt lesen, damit loop() und der Webserver auch im
    // Anzeigemodus (Dauerstrom vom Kessel) reaktionsfähig bleiben.
    int budget = 1024;
    while (Serial2.available() && budget-- > 0) {
        uint8_t c = (uint8_t)Serial2.read();

        if (!s_frActive) {
            if (c == '{') { s_frActive = true; s_rxBuf = ""; s_frNeed = -1; }
            continue;
        }

        // Rahmenlänge erreicht → dieses Byte muss die Endeklammer sein
        if (s_frNeed >= 0 && (int)s_rxBuf.length() >= s_frNeed) {
            s_frActive = false;
            if (c == '}') {
                dispatchPacket(s_rxBuf);
            } else if (c == '{') {
                s_frActive = true; s_rxBuf = ""; s_frNeed = -1;
            } else {
                Serial.println("[ETA] Rahmenfehler - Paket verworfen");
            }
            s_rxBuf = "";
            continue;
        }

        s_rxBuf += (char)c;
        if ((int)s_rxBuf.length() == 4) {
            s_frNeed = 4 + (uint8_t)s_rxBuf[2];   // Kennung+Länge+PS+Nutzdaten
        }
        if (s_rxBuf.length() > 300) {
            s_frActive = false; s_rxBuf = "";
            Serial.println("[ETA] RX-Buffer \xC3\x9C" "berlauf - geleert");
        }
    }
}

// ============================================================
//  HTML-Dashboard (in Flash gespeichert)
// ============================================================

#include "dashboard_html.h"


static String buildDashboard() {
    String html = FPSTR(DASHBOARD_HTML);
    html.replace("%IP%", s_apMode ? WiFi.softAPIP().toString() : WiFi.localIP().toString());
    html.replace("%FWVER%", FW_VERSION);
    return html;
}

// ============================================================
//  Schutz fuer alles, was am Kessel etwas veraendert
// ============================================================

/**
 * Verlangt fuer aendernde Aufrufe (Befehle, Tasten, Scans, Schreibzugriffe)
 *  1. Anmeldung (Benutzer "admin", Passwort OTA_PASS) und
 *  2. den Header "X-ETA".
 * Der Header schuetzt vor Aufrufen von fremden Webseiten: Browser schicken
 * ihn bei Cross-Origin-Aufrufen nur nach einem Preflight, den der ESP32
 * nicht beantwortet. Eine eingeloggte Browser-Sitzung allein genuegt also
 * nicht fuer einen Angriff aus dem Heimnetz.
 * Rueckgabe false: Antwort (401/403) ist bereits gesendet.
 */
static bool requireAuthedPost() {
    if (!s_server.authenticate("admin", OTA_PASS)) {
        s_server.requestAuthentication(BASIC_AUTH, "ETA-Monitor");
        return false;
    }
    if (!s_server.hasHeader("X-ETA")) {
        s_server.send(403, "application/json", "{\"error\":\"Header X-ETA fehlt\"}");
        return false;
    }
    return true;
}

// ============================================================
//  JSON-API  (GET /api)
// ============================================================

static void handleApi() {
    unsigned long now = millis();
    bool online = (s_lastDataMs > 0) && ((now - s_lastDataMs) < 120000UL);
    // Ein Wert gilt als veraltet, wenn er älter als 2,5 Zyklen ist
    unsigned long staleMs = (unsigned long)s_intervalSec * 2500UL;

    String j = "{\"online\":";
    j += online ? "true" : "false";
    j += ",\"uptime_s\":" + String(now / 1000);
    j += ",\"last_data_s\":" + (s_lastDataMs ? String((now - s_lastDataMs) / 1000) : String(-1));
    j += ",\"fw\":\"" FW_VERSION "\"";
    j += ",\"interval\":" + String(s_intervalSec);
    j += ",\"block\":" + String(s_activeBlock + 1);
    j += ",\"blocks\":" + String(ETA_NUM_BLOCKS);
    j += ",\"packets\":" + String(s_mdCount);
    j += ",\"timeouts\":" + String(s_mdTimeouts);
    j += ",\"error\":" + (s_lastError.length() ? ("\"" + s_lastError + "\"") : String("null"));
    j += ",\"error_age_s\":" + (s_lastErrorMs ? String((now - s_lastErrorMs) / 1000) : String(-1));
    j += ",\"writes\":{\"day\":" + String(writeCountToday()) +
         ",\"max_day\":" + String(WRITE_MAX_PER_DAY) +
         ",\"total\":" + String(s_writeCountTotal) + "}";

    // Knoten-Status: welche Module antworten mit echten Werten?
    j += ",\"nodes\":{";
    for (int i = 0; i < NUM_NODES; i++) {
        if (i > 0) j += ",";
        j += "\"" + String(s_nodes[i].node) + "\":" + (s_nodes[i].present ? "true" : "false");
    }
    j += "}";

    j += ",\"sensors\":{";
    for (int i = 0; i < NUM_PARAMS; i++) {
        if (i > 0) j += ",";
        // Namen sind bereits ASCII – Leerzeichen sicherheitshalber ersetzen
        String key = s_params[i].name;
        key.replace(" ", "_");
        bool stale = s_params[i].valid && (now - s_params[i].stampMs) > staleMs;
        j += "\"" + key + "\":{";
        j += "\"value\":" + (s_params[i].valid ? String(s_params[i].value, 2) : "null");
        j += ",\"unit\":\"" + String(s_params[i].unit) + "\"";
        j += ",\"age_s\":" + (s_params[i].valid ? String((now - s_params[i].stampMs) / 1000) : String(-1));
        j += ",\"stale\":" + String(stale ? "true" : "false");
        j += "}";
    }
    j += "}}";

    s_server.sendHeader("Access-Control-Allow-Origin", "*");
    s_server.send(200, "application/json", j);
}

/**
 * POST /cmd?cmd=<befehl>
 * Sendet einen IH-Steuerbefehl an den Kessel.
 * Befehle: auto | tag | nacht | reset | kessel_ein | kessel_aus | boiler
 * HINWEIS: Kessel muss SMS_erlauben = EIN haben (im Heizkreis konfigurieren)!
 */
static void handleCmd() {
    // Befehle an den Kessel nur mit Anmeldung; bewusst ohne CORS-Freigabe
    if (!requireAuthedPost()) return;

    if (s_safeMode) {
        s_server.send(503, "application/json", "{\"error\":\"Safe Mode aktiv\"}");
        return;
    }

    String cmd = s_server.arg("cmd");
    if (cmd.length() == 0) {
        s_server.send(400, "application/json", "{\"error\":\"Parameter 'cmd' fehlt\"}");
        return;
    }

    uint8_t data0 = 0;
    const char* label = nullptr;
    if      (cmd == "reset")      { data0 = 0x01; label = "Heizung Reset"; }
    else if (cmd == "auto")       { data0 = 0x02; label = "Heizung Auto"; }
    else if (cmd == "tag")        { data0 = 0x04; label = "Heizung Tag"; }
    else if (cmd == "nacht")      { data0 = 0x08; label = "Heizung Nacht"; }
    else if (cmd == "kessel_ein") { data0 = 0x10; label = "Kessel Ein"; }
    else if (cmd == "kessel_aus") { data0 = 0x20; label = "Kessel Aus"; }
    else if (cmd == "boiler")     { data0 = 0x40; label = "Boiler laden"; }
    else {
        s_server.send(400, "application/json",
            "{\"error\":\"Unbekannter Befehl. Gueltig: reset|auto|tag|nacht|kessel_ein|kessel_aus|boiler\"}");
        return;
    }

    etaSendHeatingCmd(data0);
    String resp = "{\"ok\":true,\"cmd\":\"";
    resp += cmd;
    resp += "\",\"label\":\"";
    resp += label;
    resp += "\"}";
    s_server.send(200, "application/json", resp);
}

/**
 * /set - Einstellwerte im Kessel schreiben ({MJ})
 *   GET  /set                              Freigabeliste, Limits, Zaehler (ohne Anmeldung, nur lesend)
 *   POST /set?name=Einschaltverz&val=120   Anzeigewert setzen (Anmeldung + Header X-ETA)
 *
 * Schutz des Kesselspeichers:
 *   - nur Parameter der Freigabeliste s_writeParams, nur ganze Zahlen im Wertebereich
 *   - identischer Wert wie beim letzten Schreiben: nichts senden
 *   - Mindestabstand WRITE_MIN_INTERVAL_MS je Parameter
 *   - Tageslimit WRITE_MAX_PER_DAY (rollierend 24 h)
 *   - kein Schreiben waehrend einer Listenabfrage
 * Die Antwort meldet, ob der Kessel mit {MK} bestaetigt hat.
 */
static void handleSet() {
    unsigned long now = millis();

    if (s_server.method() == HTTP_GET) {
        String j = "{\"max_per_day\":" + String(WRITE_MAX_PER_DAY);
        j += ",\"min_interval_s\":" + String(WRITE_MIN_INTERVAL_MS / 1000UL);
        j += ",\"writes_today\":" + String(writeCountToday());
        j += ",\"writes_total\":" + String(s_writeCountTotal);
        j += ",\"params\":[";
        for (int i = 0; i < NUM_WRITE_PARAMS; i++) {
            const WriteParam& p = s_writeParams[i];
            if (i) j += ",";
            j += "{\"name\":\"" + String(p.name) + "\"";
            j += ",\"node\":" + String(p.node);
            j += ",\"index\":" + String(p.index);
            j += ",\"teiler\":" + String(p.teiler);
            j += ",\"unit\":\"" + String(p.unit) + "\"";
            j += ",\"min\":" + String(p.minVal);
            j += ",\"max\":" + String(p.maxVal);
            j += ",\"last_raw\":" + String(p.lastRaw);
            j += ",\"ack_raw\":" + String(p.ackRaw);
            j += ",\"last_write_s_ago\":" + (p.lastWriteMs ? String((now - p.lastWriteMs) / 1000UL) : String(-1));
            j += "}";
        }
        j += "]}";
        s_server.send(200, "application/json", j);
        return;
    }

    if (!requireAuthedPost()) return;

    if (s_safeMode) {
        s_server.send(503, "application/json", "{\"error\":\"Safe Mode aktiv\"}");
        return;
    }
    if (s_scanActive || s_pgActive || s_txActive) {
        s_server.send(409, "application/json", "{\"error\":\"Listenabfrage laeuft, spaeter erneut versuchen\"}");
        return;
    }

    String name = s_server.arg("name");
    WriteParam* wp = nullptr;
    for (int i = 0; i < NUM_WRITE_PARAMS; i++) {
        if (name == s_writeParams[i].name) { wp = &s_writeParams[i]; break; }
    }
    if (!wp) {
        s_server.send(400, "application/json", "{\"error\":\"Parameter nicht in der Freigabeliste (GET /set)\"}");
        return;
    }

    String vs = s_server.arg("val");
    bool numeric = vs.length() > 0 && vs.length() <= 6;
    for (int i = 0; i < (int)vs.length() && numeric; i++) {
        char c = vs[i];
        if (!(isDigit(c) || (i == 0 && c == '-' && vs.length() > 1))) numeric = false;
    }
    if (!numeric) {
        s_server.send(400, "application/json", "{\"error\":\"val fehlt oder ist keine ganze Zahl\"}");
        return;
    }
    long val = vs.toInt();
    if (val < wp->minVal || val > wp->maxVal) {
        s_server.send(400, "application/json",
            "{\"error\":\"val ausserhalb des erlaubten Bereichs\",\"min\":" + String(wp->minVal) +
            ",\"max\":" + String(wp->maxVal) + "}");
        return;
    }
    long raw = val * (long)wp->teiler;
    if (raw > 32767L || raw < -32768L) {
        s_server.send(400, "application/json", "{\"error\":\"Rohwert ausserhalb 16 Bit\"}");
        return;
    }

    // Gleicher Wert wie beim letzten Schreiben: nicht erneut in den Kessel schreiben
    if (wp->lastRaw == raw && wp->ackRaw == raw) {
        s_server.send(200, "application/json",
            "{\"ok\":true,\"changed\":false,\"note\":\"Wert wurde bereits so geschrieben und bestaetigt\"}");
        return;
    }

    if (wp->lastWriteMs != 0 && (now - wp->lastWriteMs) < WRITE_MIN_INTERVAL_MS) {
        unsigned long wait = (WRITE_MIN_INTERVAL_MS - (now - wp->lastWriteMs)) / 1000UL;
        s_server.send(429, "application/json",
            "{\"error\":\"Mindestabstand zwischen zwei Schreibvorgaengen\",\"retry_in_s\":" + String(wait) + "}");
        return;
    }
    if (s_writeWindowMs == 0 || (now - s_writeWindowMs) >= WRITE_WINDOW_MS) {
        s_writeWindowMs = now;
        s_writeCountDay = 0;
    }
    if (s_writeCountDay >= WRITE_MAX_PER_DAY) {
        s_server.send(429, "application/json",
            "{\"error\":\"Tageslimit fuer Schreibvorgaenge erreicht\",\"max_per_day\":" + String(WRITE_MAX_PER_DAY) + "}");
        return;
    }

    uint32_t ackBefore = wp->ackMs;
    etaSendParamWrite(wp->node, wp->index, (int16_t)raw);
    wp->lastRaw     = raw;
    wp->lastWriteMs = now ? now : 1;
    s_writeCountDay++;
    s_writeCountTotal++;

    // Bestaetigung ({MK}) abwarten, hoechstens 2 s
    unsigned long dl = millis() + 2000UL;
    while (wp->ackMs == ackBefore && millis() < dl) { etaProcessSerial(); delay(5); }
    bool acked = (wp->ackMs != ackBefore);

    String r = "{\"ok\":true,\"changed\":true,\"name\":\"" + String(wp->name) + "\"";
    r += ",\"val\":" + String(val) + ",\"raw\":" + String(raw);
    r += ",\"ack\":" + String(acked ? "true" : "false");
    r += ",\"ack_raw\":" + String(wp->ackRaw);
    r += ",\"writes_today\":" + String(s_writeCountDay);
    r += ",\"max_per_day\":" + String(WRITE_MAX_PER_DAY) + "}";
    s_server.send(200, "application/json", r);
}

static void handleRoot() {
    if (s_safeMode) {
        // Im Safe Mode direkt zur OTA-Seite umleiten
        s_server.sendHeader("Location", "/update");
        s_server.send(302, "text/plain", "Safe Mode – redirect to /update");
        return;
    }
    // Das Dashboard enthaelt die Bedienelemente (Tasten, Befehle). Anmeldung
    // schon beim Oeffnen der Seite, damit der Browser die Zugangsdaten fuer
    // die folgenden Aufrufe (fetch) mitsendet. Lesende Endpunkte (/api,
    // /display) bleiben offen, Home Assistant ist davon nicht betroffen.
    if (!s_server.authenticate("admin", OTA_PASS)) {
        return s_server.requestAuthentication(BASIC_AUTH, "ETA-Monitor");
    }
    s_server.send(200, "text/html; charset=utf-8", buildDashboard());
}

static void handleNotFound() {
    s_server.send(404, "text/plain", "Not found");
}

// ============================================================
//  Monitorlisten-Scan  GET/POST /scan
//  ?start=1[&level=1..4]  → Scan starten
//  (kein Param)           → Ergebnis / Status als JSON
// ============================================================
static String jsonEscape(const String& s);

/** Legt den Puffer für einen Listen-Scan an und gibt die beiden anderen
 *  frei – es läuft immer nur ein Scan gleichzeitig. */
static bool scanAlloc(int which) {
    if (which != 0 && s_mbRows) { free(s_mbRows); s_mbRows = nullptr; s_mbCap = 0; s_mbCount = 0; }
    if (which != 1 && s_mgRows) { free(s_mgRows); s_mgRows = nullptr; s_mgCap = 0; s_mgCount = 0; }
    if (which != 2 && s_miRows) { free(s_miRows); s_miRows = nullptr; s_miCap = 0; s_miCount = 0; }
    switch (which) {
        case 0:
            if (!s_mbRows) { s_mbRows = (MbRow*)malloc(700 * sizeof(MbRow)); s_mbCap = s_mbRows ? 700 : 0; }
            return s_mbRows != nullptr;
        case 1:
            if (!s_mgRows) { s_mgRows = (MgRow*)malloc(700 * sizeof(MgRow)); s_mgCap = s_mgRows ? 700 : 0; }
            return s_mgRows != nullptr;
        default:
            if (!s_miRows) { s_miRows = (MiRow*)malloc(1200 * sizeof(MiRow)); s_miCap = s_miRows ? 1200 : 0; }
            return s_miRows != nullptr;
    }
}

static void handleScan() {
    s_server.sendHeader("Access-Control-Allow-Origin", "*");
    if ((s_server.method() == HTTP_POST || s_server.hasArg("start")) && !requireAuthedPost()) return;
    if (s_safeMode) {
        s_server.send(503, "application/json", "{\"error\":\"Safe Mode aktiv\"}");
        return;
    }
    if (s_server.hasArg("start") && s_server.arg("start") == "1") {
        int lvl = s_server.hasArg("level") ? s_server.arg("level").toInt() : 1;
        if (lvl < 1) lvl = 1;  if (lvl > 4) lvl = 4;
        s_scanLevel  = (uint8_t)lvl;
        if (!scanAlloc(0)) {
            s_server.send(507, "application/json", "{\"error\":\"Zu wenig Speicher\"}");
            return;
        }
        s_mbCount    = 0;
        s_scanDone   = false;
        s_scanActive = true;
        s_scanRetries = 0;
        etaSendMonitorStop();   // MC-Datenstrom anhalten
        delay(300);
        etaSendMonitorListRequest(s_scanLevel, true);
        Serial.printf("[SCAN] Gestartet (Level=%d)\n", lvl);
        s_server.send(200, "application/json",
            "{\"ok\":true,\"msg\":\"Scan gestartet\",\"level\":" + String(lvl) + "}");
        return;
    }
    String j = "{\"active\":";
    j += s_scanActive ? "true" : "false";
    j += ",\"done\":";
    j += s_scanDone   ? "true" : "false";
    j += ",\"count\":" + String(s_mbCount);
    j += ",\"rows\":[";
    s_server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    s_server.send(200, "application/json", "");
    s_server.sendContent(j);

    String chunk;
    chunk.reserve(1024);
    for (int i = 0; i < s_mbCount; i++) {
        if (i > 0) chunk += ",";
        const MbRow& r = s_mbRows[i];
        chunk += "{\"k\":" + String(r.knoten);
        chunk += ",\"h\":"  + String(r.hbyte);
        chunk += ",\"l\":"  + String(r.lbyte);
        chunk += ",\"dt\":" + String(r.datentype);
        chunk += ",\"t\":"  + String(r.teiler);
        chunk += ",\"ie\":" + String(r.indexEbene);
        chunk += ",\"n\":\"" + jsonEscape(String(r.name)) + "\"}";
        if (chunk.length() > 800) { s_server.sendContent(chunk); chunk = ""; }
    }
    chunk += "]}";
    s_server.sendContent(chunk);
    s_server.sendContent("");
}

// ============================================================
//  Parameterliste  GET/POST /params
//  ?start=1[&level=1..4]  → Auslesen starten
//  ?q=<text>              → nur Einträge, deren Name den Text enthält
//  (kein Param)           → Ergebnis / Status als JSON
// ============================================================
static void handleParams() {
    s_server.sendHeader("Access-Control-Allow-Origin", "*");
    if ((s_server.method() == HTTP_POST || s_server.hasArg("start")) && !requireAuthedPost()) return;
    if (s_safeMode) {
        s_server.send(503, "application/json", "{\"error\":\"Safe Mode aktiv\"}");
        return;
    }
    if (s_server.hasArg("start") && s_server.arg("start") == "1") {
        int lvl = s_server.hasArg("level") ? s_server.arg("level").toInt() : 1;
        if (lvl < 1) lvl = 1;  if (lvl > 4) lvl = 4;
        s_pgLevel  = (uint8_t)lvl;
        if (!scanAlloc(1)) {
            s_server.send(507, "application/json", "{\"error\":\"Zu wenig Speicher\"}");
            return;
        }
        s_mgCount  = 0;
        s_pgDone   = false;
        s_pgActive = true;
        s_pgRetries = 0;
        etaSendMonitorStop();
        delay(300);
        etaSendParamListRequest(s_pgLevel, true);
        Serial.printf("[PARAM] Gestartet (Level=%d)\n", lvl);
        s_server.send(200, "application/json",
            "{\"ok\":true,\"msg\":\"Parameterliste wird gelesen\",\"level\":" + String(lvl) + "}");
        return;
    }

    String filter = s_server.arg("q");
    filter.toLowerCase();

    // Stückweise senden – bei mehreren hundert Einträgen passt die
    // Antwort sonst nicht mehr zusammenhängend in den Heap.
    s_server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    s_server.send(200, "application/json", "");

    String head = "{\"active\":";
    head += s_pgActive ? "true" : "false";
    head += ",\"done\":";
    head += s_pgDone ? "true" : "false";
    head += ",\"count\":" + String(s_mgCount);
    head += ",\"rows\":[";
    s_server.sendContent(head);

    String chunk;
    chunk.reserve(1024);
    bool first = true;
    for (int i = 0; i < s_mgCount; i++) {
        const MgRow& r = s_mgRows[i];
        if (filter.length()) {
            String n(r.name);
            n.toLowerCase();
            if (n.indexOf(filter) < 0) continue;
        }
        if (!first) chunk += ',';
        first = false;
        chunk += "{\"k\":"   + String(r.knoten);
        chunk += ",\"i\":"   + String(r.index);
        chunk += ",\"dt\":"  + String(r.datentyp);
        chunk += ",\"u\":"   + String(r.einheit);
        chunk += ",\"t\":"   + String(r.teiler);
        chunk += ",\"e\":"   + String(r.ebene);
        chunk += ",\"min\":" + String(r.vmin);
        chunk += ",\"max\":" + String(r.vmax);
        chunk += ",\"std\":" + String(r.vstd);
        chunk += ",\"akt\":" + String(r.vakt);
        chunk += ",\"n\":\"" + jsonEscape(String(r.name)) + "\"}";
        if (chunk.length() > 800) { s_server.sendContent(chunk); chunk = ""; }
    }
    chunk += "]}";
    s_server.sendContent(chunk);
    s_server.sendContent("");
}

// ============================================================
//  Textliste  GET/POST /texts
//  ?start=1  → Auslesen starten
//  Klartexte zu Zustands- und Auswahlwerten.
// ============================================================
static void handleTexts() {
    s_server.sendHeader("Access-Control-Allow-Origin", "*");
    if ((s_server.method() == HTTP_POST || s_server.hasArg("start")) && !requireAuthedPost()) return;
    if (s_safeMode) {
        s_server.send(503, "application/json", "{\"error\":\"Safe Mode aktiv\"}");
        return;
    }
    if (s_server.hasArg("start") && s_server.arg("start") == "1") {
        if (!scanAlloc(2)) {
            s_server.send(507, "application/json", "{\"error\":\"Zu wenig Speicher\"}");
            return;
        }
        s_miCount   = 0;
        s_txDone    = false;
        s_txActive  = true;
        s_txRetries = 0;
        etaSendMonitorStop();
        delay(300);
        etaSendTextListRequest(true);
        Serial.println("[TEXT] Gestartet");
        s_server.send(200, "application/json", "{\"ok\":true,\"msg\":\"Textliste wird gelesen\"}");
        return;
    }

    s_server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    s_server.send(200, "application/json", "");
    String head = "{\"active\":";
    head += s_txActive ? "true" : "false";
    head += ",\"done\":";
    head += s_txDone ? "true" : "false";
    head += ",\"count\":" + String(s_miCount);
    head += ",\"rows\":[";
    s_server.sendContent(head);

    String chunk;
    chunk.reserve(1024);
    for (int i = 0; i < s_miCount; i++) {
        if (i > 0) chunk += ',';
        const MiRow& r = s_miRows[i];
        chunk += "{\"k\":" + String(r.knoten);
        chunk += ",\"i\":" + String(r.index);
        chunk += ",\"n\":\"" + jsonEscape(String(r.name)) + "\"}";
        if (chunk.length() > 800) { s_server.sendContent(chunk); chunk = ""; }
    }
    chunk += "]}";
    s_server.sendContent(chunk);
    s_server.sendContent("");
}

// ============================================================
//  Display-Steuerung  GET /display  POST /display?btn=...
//  GET              → Display-Inhalt anfordern (IA) + JSON liefern
//  POST ?btn=ok|esc|up|down|left|right  → Taste drücken (IB)
// ============================================================
/** GET  /interval          → {"interval":60,"min":10,"max":255}
 *  POST /interval?val=N   → neues Intervall setzen (10–255 s) + MC sofort neu senden */
static void handleInterval() {
    s_server.sendHeader("Access-Control-Allow-Origin", "*");
    if (s_server.method() == HTTP_POST && !requireAuthedPost()) return;
    if (s_server.method() == HTTP_POST && s_server.hasArg("val")) {
        int v = s_server.arg("val").toInt();
        if (v < 10)  v = 10;
        if (v > 255) v = 255;
        s_intervalSec = (uint8_t)v;
        s_awaitingMd   = false;   // Zyklus sofort neu starten
        s_blockDoneMs  = 0;
        s_cycleStartMs = 0;
        Serial.printf("[HTTP] Intervall auf %d s gesetzt\n", s_intervalSec);
    }
    String j = "{\"interval\":";
    j += s_intervalSec;
    j += ",\"min\":10,\"max\":255}";
    s_server.send(200, "application/json", j);
}

static String jsonEscape(const String& s) {
    String o;
    o.reserve(s.length() + 8);
    for (int i = 0; i < (int)s.length(); i++) {
        char c = s[i];
        if      (c == '"')  o += "\\\"";
        else if (c == '\\') o += "\\\\";
        else if (c == '\n') o += "\\n";
        else if ((uint8_t)c < 0x20) o += ' ';
        else                o += c;
    }
    return o;
}

// Tastencodes des ETA-Bedienteils (Symcon-Community Thread 16637, Beitrag 147)
struct DispKey { const char* name; uint8_t code; };
static const DispKey s_dispKeys[] = {
    { "aendern",  0 }, { "ja",       1 }, { "auf",      2 }, { "ab",       3 },
    { "zurueck",  4 }, { "info",     5 }, { "kessel",   6 }, { "entaschen",7 },
};

static void handleDisplay() {
    s_server.sendHeader("Access-Control-Allow-Origin", "*");
    if (s_safeMode) {
        s_server.send(503, "application/json", "{\"error\":\"Safe Mode aktiv\"}");
        return;
    }
    // Jeder Zugriff verlängert die Laufzeit der Anzeige (siehe loop())
    if (s_dispOn) s_dispOnMs = millis();

    if (s_server.hasArg("on")) {
        bool on = (s_server.arg("on") != "0");
        if (!on) s_dispValid = false;
        etaSendDisplayMode(on);        // Der Kessel schickt die Anzeige unaufgefordert; kurz darauf warten
        if (on) {
            unsigned long dl = millis() + 3000UL;
            while (!s_dispValid && millis() < dl) { etaProcessSerial(); delay(5); }
        }
        s_server.send(200, "application/json",
            String("{\"ok\":true,\"on\":") + (on ? "true" : "false") +
            ",\"valid\":" + (s_dispValid ? "true" : "false") + "}");
        return;
    }

    if (s_server.hasArg("key")) {
        if (!requireAuthedPost()) return;   // Tastendruecke am Kessel nur mit Anmeldung
        String k = s_server.arg("key");
        k.toLowerCase();
        int code = -1;
        for (const DispKey& dk : s_dispKeys) {
            if (k == dk.name) { code = dk.code; break; }
        }
        if (code < 0 && k.length() && isDigit(k[0])) {
            int n = k.toInt();
            if (n >= 0 && n <= 7) code = n;
        }
        if (code < 0) {
            s_server.send(400, "application/json",
                "{\"error\":\"Taste: aendern|ja|auf|ab|zurueck|info|kessel|entaschen oder 0..7\"}");
            return;
        }
        etaSendDisplayKey((uint8_t)code);
        unsigned long dl = millis() + 2000UL;
        unsigned long before = s_dispRxMs;
        while (s_dispRxMs == before && millis() < dl) { etaProcessSerial(); delay(5); }
        s_server.send(200, "application/json",
            "{\"ok\":true,\"key\":\"" + k + "\",\"code\":" + String(code) + "}");
        return;
    }

    // Reine Abfrage: eventuell wartende Pakete einsammeln
    etaProcessSerial();

    String j = "{\"on\":";
    j += s_dispOn ? "true" : "false";
    j += ",\"valid\":";
    j += s_dispValid ? "true" : "false";
    j += ",\"lines\":[";
    for (int r = 0; r < DISP_ROWS; r++) {
        if (r) j += ',';
        j += '"' + jsonEscape(s_dispLines[r]) + '"';
    }
    j += "]";
    j += ",\"text\":\"" + jsonEscape(s_dispText) + "\"";
    j += ",\"status\":\"" + jsonEscape(s_dispStatus) + "\"";
    j += ",\"raw\":\"" + s_dispRaw + "\"";
    j += ",\"age_ms\":" + String(s_dispValid ? (long)(millis() - s_dispRxMs) : -1L);
    j += "}";
    s_server.send(200, "application/json", j);
}

// ============================================================
//  OTA – Web-Upload  (GET /update  +  POST /update)
// ============================================================

static const char UPDATE_HTML[] PROGMEM = R"UPHTML(
<!DOCTYPE html>
<html lang="de">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ETA Monitor &#x2013; Firmware Update</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:'Segoe UI',sans-serif;background:#0f172a;color:#e2e8f0;min-height:100vh}
header{background:linear-gradient(135deg,#c2410c,#9a3412);padding:18px 24px}
header h1{font-size:1.4rem;font-weight:700;color:#fff}
.wrap{max-width:600px;margin:32px auto;padding:0 16px}
.card{background:#1e293b;border:1px solid #334155;border-radius:12px;padding:24px}
h2{margin-bottom:12px;font-size:1.1rem}
.info{font-size:.85rem;color:#94a3b8;margin-bottom:20px;line-height:1.6}
.info b{color:#e2e8f0}
input[type=file]{width:100%;padding:10px;background:#0f172a;border:1px solid #334155;border-radius:8px;color:#e2e8f0;margin-bottom:14px;cursor:pointer}
button{background:#ea580c;color:#fff;border:none;padding:11px;border-radius:8px;cursor:pointer;font-size:1rem;width:100%;font-weight:600}
button:hover{background:#c2410c}
button:disabled{background:#374151;cursor:not-allowed}
#progwrap{display:none;margin-top:16px}
progress{width:100%;height:18px;border-radius:4px}
#stat{font-size:.85rem;color:#94a3b8;margin-top:8px;text-align:center}
.warn{background:#7c2d12;border:1px solid #9a3412;border-radius:8px;padding:12px;margin-bottom:16px;font-size:.85rem}
a{color:#f97316}
</style></head>
<body>
<header><h1>&#x1F4E6; Firmware Update</h1></header>
<div class="wrap">
<div class="card">
<h2>Neue Firmware hochladen</h2>
%WARNBLOCK%
<div class="info">
Version: <b>%VERSION%</b> &bull; Modus: <b>%MODE%</b><br><br>
.bin aus Arduino IDE: <i>Sketch &#x2192; Kompilierte Bin&#xE4;rdatei exportieren</i>
</div>
<form id="frm">
<input type="file" id="fw" accept=".bin" required>
<button type="submit">&#x2B06; Hochladen &amp; Neustart</button>
</form>
<div id="progwrap">
<progress id="bar" value="0" max="100"></progress>
<div id="stat">Lade hoch...</div>
</div></div>
<p style="margin-top:16px;font-size:.8rem"><a href="/">&larr; Zur&#xFC;ck zum Dashboard</a></p>
</div>
<script>
document.getElementById('frm').addEventListener('submit',function(e){
e.preventDefault();
var f=document.getElementById('fw').files[0];if(!f)return;
var fd=new FormData();fd.append('firmware',f);
var xhr=new XMLHttpRequest();
xhr.open('POST','/update');
xhr.withCredentials=true;
xhr.upload.onprogress=function(ev){
  if(ev.lengthComputable){
    var p=Math.round(ev.loaded/ev.total*100);
    document.getElementById('bar').value=p;
    document.getElementById('stat').textContent=p+'% ('+Math.round(ev.loaded/1024)+' KB)';
  }
};
xhr.onload=function(){
  document.getElementById('stat').textContent=xhr.responseText;
  if(xhr.status===200)setTimeout(function(){location.href='/';},7000);
};
xhr.onerror=function(){document.getElementById('stat').textContent='Verbindungsfehler!';};
document.getElementById('progwrap').style.display='block';
document.querySelector('button[type=submit]').disabled=true;
xhr.send(fd);
});
</script>
</body></html>
)UPHTML";

static String buildUpdatePage(bool safeMode) {
    String html = FPSTR(UPDATE_HTML);
    html.replace("%VERSION%", FW_VERSION);
    html.replace("%MODE%",    safeMode ? "&#x26A0; SAFE MODE" : "Normal");
    html.replace("%WARNBLOCK%",
        safeMode
        ? "<div class='warn'>&#x26A0; <b>SAFE MODE</b> &ndash; Doppel-Reset erkannt. Nur OTA aktiv.<br>Normalbetrieb: Ger&auml;t <b>einmal</b> normal neu starten.</div>"
        : "");
    return html;
}

/** GET /update – Passwortgeschützte Update-Seite */
static void handleUpdateGet() {
    if (!s_server.authenticate("admin", OTA_PASS)) {
        return s_server.requestAuthentication(BASIC_AUTH, "ETA-OTA");
    }
    s_server.send(200, "text/html; charset=utf-8", buildUpdatePage(s_safeMode));
}

/** Upload-Callback – empfängt die .bin-Daten chunk-weise */
static void handleUpdateUpload() {
    HTTPUpload& upload = s_server.upload();
    if (upload.status == UPLOAD_FILE_START) {
        Serial.printf("[OTA-Web] Start: %s\n", upload.filename.c_str());
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
            Update.printError(Serial);
        }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.isRunning()) {
            if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
                Update.printError(Serial);
            }
            Serial.print(".");
        }
    } else if (upload.status == UPLOAD_FILE_END) {
        Serial.println();
        if (Update.isRunning()) {
            if (Update.end(true)) {
                Serial.printf("[OTA-Web] OK \xe2\x80\x93 %u Bytes\n", upload.totalSize);
            } else {
                Update.printError(Serial);
            }
        }
    }
}

/** POST /update – Abschluss: Ergebnis senden, ggf. neu starten */
static void handleUpdateComplete() {
    if (!s_server.authenticate("admin", OTA_PASS)) {
        return s_server.requestAuthentication(BASIC_AUTH, "ETA-OTA");
    }
    s_server.sendHeader("Connection", "close");
    if (Update.hasError()) {
        s_server.send(500, "text/plain; charset=utf-8",
                      "Update fehlgeschlagen! Bitte erneut versuchen.");
    } else {
        s_server.send(200, "text/plain; charset=utf-8",
                      "Update OK! Neustart in 5 s...");
        delay(3000);
        ESP.restart();
    }
}

// ============================================================
//  ArduinoOTA – Netzwerk-Upload (Arduino IDE Netzwerk-Port)
// ============================================================

static void setupArduinoOta() {
    ArduinoOTA.setHostname(OTA_HOSTNAME);
    ArduinoOTA.setPassword(OTA_PASS);
    ArduinoOTA.onStart([]() {
        String t = (ArduinoOTA.getCommand() == U_FLASH) ? "Sketch" : "Dateisystem";
        Serial.println("\n[ArduinoOTA] Start: " + t);
    });
    ArduinoOTA.onEnd([]() {
        Serial.println("\n[ArduinoOTA] Fertig \xe2\x80\x93 Neustart...");
    });
    ArduinoOTA.onProgress([](unsigned int p, unsigned int t) {
        Serial.printf("[ArduinoOTA] %3u%%\r", (p * 100) / t);
    });
    ArduinoOTA.onError([](ota_error_t err) {
        Serial.printf("\n[ArduinoOTA] Fehler [%u]: ", err);
        if      (err == OTA_AUTH_ERROR)    Serial.println("Authentifizierung");
        else if (err == OTA_BEGIN_ERROR)   Serial.println("Begin");
        else if (err == OTA_CONNECT_ERROR) Serial.println("Verbindung");
        else if (err == OTA_RECEIVE_ERROR) Serial.println("Empfang");
        else if (err == OTA_END_ERROR)     Serial.println("Ende");
    });
    ArduinoOTA.begin();
    Serial.printf("[ArduinoOTA] Bereit \xe2\x80\x93 Hostname: '%s'\n", OTA_HOSTNAME);
}

// ============================================================
//  Setup & Loop
// ============================================================

void setup() {
    // ---- Double-Reset-Erkennung (muss als allererstes laufen) ---
    // RTC-Speicher wird bei Reset erhalten, bei Power-Off gelöscht.
    // Ist das Magic bereits gesetzt → zweiter Reset innerhalb 5 s → Safe Mode.
    s_safeMode  = (g_resetFlag == RESET_MAGIC);
    g_resetFlag = RESET_MAGIC; // wird in loop() nach 5 s auf 0 gesetzt

    Serial.begin(115200);
    delay(200);
    Serial.println("\n\n==============================");
    Serial.printf("  ETA Monitor  v%s%s\n", FW_VERSION,
                  s_safeMode ? "  [SAFE MODE]" : "");
    Serial.println("==============================");
    if (s_safeMode) {
        Serial.println("[SAFE] Doppel-Reset erkannt!");
        Serial.println("[SAFE] Normalbetrieb unterdrueckt – nur OTA aktiv");
        Serial.println("[SAFE] Zum Normalbetrieb: einmal normal neu starten");
    }

    // ---- UART2 für ETA (nur im Normalbetrieb) -------------------
    if (!s_safeMode) {
        Serial2.setRxBufferSize(1024);  // MD-Antwort bis ~110 Bytes/Block, Puffer großzügig
        Serial2.begin(ETA_BAUD, SERIAL_8N1, ETA_RX_PIN, ETA_TX_PIN);
        Serial.printf("[UART] Serial2: %d Baud, RX=GPIO%d, TX=GPIO%d\n",
                      ETA_BAUD, ETA_RX_PIN, ETA_TX_PIN);
        s_rxBuf.reserve(256);
        s_dispRaw.reserve(280);
        s_dispText.reserve(DISP_ROWS * (DISP_COLS + 1));
        for (int r = 0; r < DISP_ROWS; r++) s_dispLines[r].reserve(DISP_COLS + 1);
        // Falls vor einem Neustart die Anzeige aktiv war, w\u00fcrde der Kessel
        // weitersenden \u2013 daher sicherheitshalber abschalten.
        s_dispOn = true;                // damit etaSendDisplayMode() wirkt
        etaSendDisplayMode(false);
        memset(s_settings, 0, sizeof(s_settings));
        for (int i = 0; i < NUM_PARAMS; i++) {
            s_params[i].value = 0.0f;
            s_params[i].valid = false;
        }
    }

    // ---- WiFi verbinden -----------------------------------------
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    Serial.printf("[WiFi] Verbinde mit '%s'", WIFI_SSID);
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < WIFI_CONNECT_TRIES) {
        delay(500);
        Serial.print(".");
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("\n[WiFi] Verbunden! IP: %s\n",
                      WiFi.localIP().toString().c_str());
        s_apMode = false;
    } else {
        WiFi.mode(WIFI_AP);
        WiFi.softAP(AP_SSID, AP_PASS);
        s_apMode    = true;
        s_apSinceMs = millis();
        Serial.printf("\n[WiFi] Kein WLAN \xe2\x86\x92 Access Point '%s'\n", AP_SSID);
        Serial.println("[WiFi] AP-Passwort: siehe config.h (AP_PASS)");
        Serial.printf("[WiFi] AP-IP:       %s\n", WiFi.softAPIP().toString().c_str());
    }

    // Firmware als gültig markieren → verhindert automatischen OTA-Rollback
    // (wirkt nur, wenn Bootloader mit CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y)
    esp_ota_mark_app_valid_cancel_rollback();
    s_otaValidMarked = true;
    Serial.println("[OTA]  Firmware als gueltig markiert (Rollback deaktiviert)");

    // ---- ArduinoOTA (Netzwerk-Upload via Arduino IDE) -----------
    setupArduinoOta();

    // ---- Web-Server ---------------------------------------------
    s_server.on("/",       HTTP_GET,  handleRoot);
    s_server.on("/api",    HTTP_GET,  handleApi);
    s_server.on("/cmd",    HTTP_POST, handleCmd);
    s_server.on("/update", HTTP_GET,  handleUpdateGet);
    s_server.on("/update", HTTP_POST, handleUpdateComplete, handleUpdateUpload);
    s_server.on("/scan",    HTTP_GET,  handleScan);
    s_server.on("/scan",    HTTP_POST, handleScan);
    s_server.on("/params",  HTTP_GET,  handleParams);
    s_server.on("/params",  HTTP_POST, handleParams);
    s_server.on("/texts",   HTTP_GET,  handleTexts);
    s_server.on("/texts",   HTTP_POST, handleTexts);
    s_server.on("/display",  HTTP_GET,  handleDisplay);
    s_server.on("/display",  HTTP_POST, handleDisplay);
    s_server.on("/interval", HTTP_GET,  handleInterval);
    s_server.on("/interval", HTTP_POST, handleInterval);
    s_server.on("/set",      HTTP_GET,  handleSet);
    s_server.on("/set",      HTTP_POST, handleSet);
    s_server.onNotFound(handleNotFound);
    {
        // Header "X-ETA" muss vom WebServer mitgelesen werden (siehe requireAuthedPost)
        static const char* s_hdrKeys[] = { "X-ETA" };
        s_server.collectHeaders(s_hdrKeys, 1);
    }
    s_server.begin();
    Serial.printf("[HTTP] Server auf Port %d\n", HTTP_PORT);

    // ---- ETA Datenabfrage (nur Normalbetrieb) -------------------
    if (!s_safeMode) {
        Serial.println("[ETA]  Sende MC-Befehl in 2 s...");
        delay(2000);
        s_cycleStartMs = millis();
        etaSendMonitorConfig();
    }

    String ip = s_apMode ? WiFi.softAPIP().toString()
                         : WiFi.localIP().toString();
    Serial.println("[OK]   Bereit:");
    Serial.printf("       Dashboard:  http://%s/\n",       ip.c_str());
    Serial.printf("       OTA-Update: http://%s/update  (Benutzer admin, Passwort siehe config.h)\n",
                  ip.c_str());
    if (s_safeMode) {
        Serial.println("[SAFE] Warte auf Firmware-Upload...");
    }
}

// ---------------------------------------------------------------
// WLAN-Watchdog
// Startet der ESP nach einem Stromausfall vor dem Router, findet er
// beim Booten kein WLAN und faellt in den Fallback-AP. Bisher blieb
// er dort bis zu einem manuellen Power-Cycle haengen. Der Watchdog
// sucht im AP-Modus regelmaessig nach dem heimischen WLAN und startet
// neu, sobald es wieder da ist. Der Fallback-AP selbst bleibt voll
// erhalten - er ist nur keine Sackgasse mehr.
// ---------------------------------------------------------------
static void wifiWatchdog() {
    const unsigned long now = millis();
    if (now - s_wifiCheckMs < WIFI_CHECK_MS) return;
    s_wifiCheckMs = now;

    if (s_apMode) {
        // Erst nach der Wartezeit erneut suchen - ein Scan blockiert
        // einige Sekunden, das soll nicht dauernd passieren.
        if (now - s_apSinceMs < WIFI_AP_RETRY_MS) return;
        s_apSinceMs = now;

        WiFi.mode(WIFI_AP_STA);              // Scan braucht die STA-Seite
        int n = WiFi.scanNetworks();
        bool found = false;
        for (int i = 0; i < n; i++) {
            if (WiFi.SSID(i) == String(WIFI_SSID)) { found = true; break; }
        }
        WiFi.scanDelete();

        if (found) {
            Serial.printf("[WiFi] '%s' wieder erreichbar -> Neustart\n", WIFI_SSID);
            delay(200);
            ESP.restart();
        } else {
            WiFi.mode(WIFI_AP);              // zurueck in den reinen AP-Modus
            Serial.println("[WiFi] AP-Modus: WLAN weiterhin nicht sichtbar");
        }
        return;
    }

    // Normalbetrieb: Verbindungsverlust ueberwachen
    if (WiFi.status() == WL_CONNECTED) {
        s_staLostMs = 0;
        return;
    }
    if (s_staLostMs == 0) {
        s_staLostMs = now;
        Serial.println("[WiFi] Verbindung verloren - versuche neu zu verbinden");
        WiFi.reconnect();
        return;
    }
    if (now - s_staLostMs > WIFI_STA_LOST_MS) {
        Serial.println("[WiFi] Zu lange ohne Verbindung -> Neustart");
        delay(200);
        ESP.restart();
    }
}

void loop() {
    ArduinoOTA.handle();          // Netzwerk-OTA (Arduino IDE)
    s_server.handleClient();      // Web-Server + Web-OTA
    wifiWatchdog();               // WLAN-Rueckkehr / Verbindungsverlust

    if (!s_safeMode) {
        etaProcessSerial();

        // Der Kessel sendet im Anzeigemodus dauerhaft Displaydaten.
        // Wenn 2 Minuten lang niemand mehr auf /display zugreift,
        // wird die Anzeige automatisch wieder abgeschaltet.
        if (s_dispOn && (millis() - s_dispOnMs) > 120000UL) {
            Serial.println("[DISP] Zeitüberschreitung – Anzeige wird abgeschaltet");
            etaSendDisplayMode(false);
            s_dispOnMs = 0;
            s_dispValid = false;
        }

        // Parameter-Scan: Bleibt eine Antwort aus, wird die Anfrage
        // wiederholt. Nach mehreren Fehlversuchen wird abgebrochen,
        // damit der Messbetrieb nicht dauerhaft blockiert bleibt.
        if (s_pgActive && (millis() - s_pgLastMs) > 1500UL) {
            if (++s_pgRetries > 12) {
                Serial.printf("[PARAM] Abbruch nach %d Eintraegen (keine Antwort)\n", s_mgCount);
                s_pgActive = false;
                s_pgDone   = true;
                etaSendScanStop();
                s_awaitingMd   = false;
                s_blockDoneMs  = 0;
                s_cycleStartMs = 0;
            } else {
                etaSendParamListRequest(s_pgLevel, false);
            }
        }

        // Gleicher Schutz für den Monitorlisten-Scan.
        if (s_scanActive && (millis() - s_scanLastMs) > 1500UL) {
            if (++s_scanRetries > 12) {
                Serial.printf("[SCAN] Abbruch nach %d Eintraegen (keine Antwort)\n", s_mbCount);
                s_scanActive = false;
                s_scanDone   = true;
                etaSendScanStop();
                s_awaitingMd   = false;
                s_blockDoneMs  = 0;
                s_cycleStartMs = 0;
            } else {
                etaSendMonitorListRequest(s_scanLevel, false);
            }
        }

        // Gleicher Schutz für die Textliste.
        if (s_txActive && (millis() - s_txLastMs) > 1500UL) {
            if (++s_txRetries > 12) {
                Serial.printf("[TEXT] Abbruch nach %d Eintraegen (keine Antwort)\n", s_miCount);
                s_txActive = false;
                s_txDone   = true;
                etaSendScanStop();
                s_awaitingMd   = false;
                s_blockDoneMs  = 0;
                s_cycleStartMs = 0;
            } else {
                etaSendTextListRequest(false);
            }
        }

        // Ablauf pro Block:
        //   MC senden → auf MD warten → ME senden → nächster Block
        // Erst wenn alle Blöcke durch sind, wird bis zum Ende des
        // Gesamtzyklus (s_intervalSec) gewartet.
        // Während eines MA/Mb-Scans werden keine MC-Blöcke gesendet.
        if (!s_scanActive && !s_pgActive && !s_txActive) {
            unsigned long now = millis();

            if (s_awaitingMd) {
                // Antwort bleibt aus → Block überspringen statt hängen zu bleiben
                unsigned long limit = (unsigned long)s_blockSec * 1000UL + 10000UL;
                if ((now - s_lastMcSentMs) > limit) {
                    s_mdTimeouts++;
                    Serial.printf("[ETA] Block %d ohne Antwort (Timeout) – weiter\n",
                                  s_activeBlock + 1);
                    s_awaitingMd  = false;
                    s_blockDoneMs = now;
                    etaSendMonitorStop();
                    advanceBlock();
                }
            } else {
                // Nach dem letzten Block bis zum Zyklusende pausieren,
                // dazwischen nur kurz Luft holen.
                unsigned long wait = 300UL;
                if (s_activeBlock == 0 && s_cycleStartMs != 0) {
                    unsigned long cycle   = (unsigned long)s_intervalSec * 1000UL;
                    unsigned long elapsed = now - s_cycleStartMs;
                    wait = (elapsed < cycle) ? (cycle - elapsed) : 0UL;
                }
                if ((now - s_blockDoneMs) >= wait) {
                    if (s_activeBlock == 0) s_cycleStartMs = now;
                    etaSendMonitorConfig();
                }
            }
        }
    }

    // Double-Reset-Fenster nach 5 s schließen
    // (danach löst ein einzelner Reset keinen Safe Mode mehr aus)
    if (!s_resetFlagCleared && millis() > 5000UL) {
        g_resetFlag        = 0;
        s_resetFlagCleared = true;
        Serial.println("[OTA] Double-Reset-Fenster abgelaufen (5 s)");
    }
}
