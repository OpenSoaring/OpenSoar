# OpenSoar - Unterschiede zu XCSoar

*English version: see `OpenSoar-AddOn.md`.*

OpenSoar ist XCSoar plus ein kleiner Stapel von Erweiterungen.  Diese
Datei ist die vollständige, aktuelle Liste dieser Unterschiede - was
hier nicht steht, verhält sich exakt wie XCSoar.

Hausregel: Sobald einer dieser Punkte in XCSoar übernommen wird, ist
er kein Add-on mehr und wird aus dieser Liste ENTFERNT.  Punkte mit
`[upstream PR]` sind bereits bei XCSoar eingereicht und werden die
Liste voraussichtlich verlassen.

## Zusätzliche Treiber

| Treiber | Hersteller | Anmerkung |
|:------- |:---------- |:--------- |
| **SteFly RemoteStick** | SteFly | Knüppel-Fernbedienung; automatische Erkennung (USB 1209:8500) auf einem eigenen Geräte-Slot - belegt nie einen der frei konfigurierbaren Ports; Manage-Dialog mit Senden / Empfangen / Neustart |
| **SteFly RotaryPanel** | SteFly | Drehknopf-Bedienpanel |
| **Anemoi** | RS-Flight | Echtzeit-Windmessung |
| **Becker AR62xx** | Becker | Funkgeräte-Treiber |
| **FreeVario** | Blaubart | FreeVario-Protokoll |

## Branding und Bedienoberfläche

* OpenSoar-Name, -Logos und -Icons; der Startbildschirm zeigt die
  vollständige Versionsnummer deutlich an
* Testversionen (vX.Y.Z.tN) bauen die rote "Testing"-Variante
  inklusive roter Programm-Icons - eine Testinstallation ist auf
  einen Blick erkennbar; Releases sind grün
* der "Was ist neu"-Schnelleinstieg erscheint nur, wenn sich die
  zugrunde liegende XCSoar-Basisversion (Major.Minor) ändert, nicht
  bei jedem OpenSoar-Update

## Korrekturen vor Upstream

* der Port-Monitor stürzt in MSVC-Debug-Builds nicht mehr ab
  (undefiniertes Verhalten in einem Grid-Container) `[upstream PR]`

(Der Startup-Exit-Code-Fix und der Migrations-Fix wurden in XCSoar
gemerged und haben diese Liste verlassen; der frühere CUPX-Workaround
wurde hinfällig, als upstream das Grundproblem zentral behob.)

## Build- und Release-Infrastruktur

* nativer Windows-Build mit CMake und Visual Studio (2022/2026), nur
  noch OpenGL-Rendering; alle Fremdbibliotheken werden beim ersten
  Build automatisch mitgebaut
* GitHub-CI baut und veröffentlicht das Windows-Paket für jedes
  Release-Tag; `.tN`-Tags werden Pre-Releases mit der Testing-Variante
* Versionsschema: `MAJOR.MINOR` folgen der XCSoar-Basisversion, die
  dritte Zahl ist der OpenSoar-Release-Zähler, `.tN` bezeichnet die
  N-te Testversion, eine numerische vierte Stelle ein Bugfix-Release

## Geplant (in dieser Version noch nicht aktiv)

* OpenVario: gerätespezifische System-Einstellungen (WLAN, Display,
  Rotation, Herunterfahren/Neustart) integriert in OpenSoar statt
  einer separaten Basemenü-Anwendung - der Code-Unterbau ist
  vorbereitet, die Bedienoberfläche folgt in einer der nächsten
  Versionen
