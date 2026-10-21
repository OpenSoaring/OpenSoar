# OpenSoar - Unterschiede zu XCSoar

*English version: see `OpenSoar-AddOn.md`.*

OpenSoar ist XCSoar plus ein kleiner Stapel von Erweiterungen.  Diese
Datei ist die vollständige, aktuelle Liste dieser Unterschiede - was
hier nicht steht, verhält sich exakt wie XCSoar.

Hausregel: Sobald einer dieser Punkte in XCSoar übernommen wird, ist
er kein Add-on mehr und wird aus dieser Liste ENTFERNT.  Punkte mit
`[upstream PR]` sind bereits bei XCSoar eingereicht und werden die
Liste voraussichtlich verlassen.

Diese Fassung der Liste gehört zu OpenSoar 7.46.26, das auf XCSoar
7.46 aufbaut.

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
* die Über-Seite nennt OpenSoar und seine Webseite, zeigt das
  Sponsor-Logo neben dem OpenSoar-Logo und enthält die OpenSoar-News
  als eigene Seite

## Start und Beenden

* der Profildialog ist der Startbildschirm: er erscheint immer, mit
  einstellbarem Countdown ("Start-Timeout"); jede Eingabe stoppt den
  Countdown, Null wartet auf den Benutzer; `-profile=X` wählt X vor
* die Fliegen/Simulator-Frage erscheint nur mit der Kommandozeilen-
  Option `-ask` (`-simulator` funktioniert wie bisher)
* ein Dialog zum Beenden: Beenden / Neu starten / Gerät neu starten /
  Ausschalten - jeweils nur, wo das Ziel es wirklich kann (Android:
  Beenden und Neu starten; Kobo und OpenVario: alle vier; Desktop:
  Beenden und Neu starten); auf dem OpenVario steht Ausschalten vorn,
  und die Tasten Q und X öffnen den Dialog auf Beenden bzw.
  Ausschalten
* überall, wo XCSoar zum manuellen Beenden und Neustarten auffordert,
  bietet OpenSoar den internen Neustart direkt an
* eine geräteweite Systemeinstellung stellt das XCSoar-Verhalten
  wieder her, für alle, die es lieber so haben

## Geräteeinstellungen außerhalb des Profils

* Systemeinstellungen (XCSoar-Verhalten, Geräte im Profil,
  Display-Auflösung des Geräts, Testversionen) liegen in einer Datei
  außerhalb des Datenordners - sie gehören zum Gerät, nicht zum
  Profil, und wandern nicht mit, wenn der Datenordner auf ein anderes
  Gerät kopiert wird
* die NMEA-Geräte und ihre Ports liegen in `device_ports.xcd`, einer
  JSON-Datei mit nur den Nicht-Standard-Feldern; eine alte
  key=value-Datei wird automatisch umgewandelt; ein Schalter stellt
  pro Gerät den XCSoar-Weg (Ports im Profil) wieder her
* auf dem OpenVario liegen diese Dateien auf der Datenpartition, ein
  Firmware-Upgrade behält sie also
* der Cache (Kartenkacheln, Wetterdateien) liegt außerhalb des
  Datenordners (`~/data/.cache/OpenSoar` auf dem OpenVario, der
  XDG-Cache-Ordner unter Linux)

## OpenVario

* die Systemfunktionen sind eine Konfigurationsseite, System >
  OpenVario, statt eines eigenen Menüs und des separaten
  OpenVarioBaseMenu; WLAN und Datenübertragung nutzen die normalen
  XCSoar-Seiten `[upstream PR]`:
  - welches Programm das Image nach dem Booten startet (XCSoar oder
    OpenSoar)
  - das laufende Image und das Firmware-Upgrade aus dem
    Download-Verzeichnis oder von einem USB-Stick, mit Download,
    Löschen und einem Filter für die Hardware des Geräts
  - sensord und variod als Schalter, Sensor-Kalibrierung
  - Sicherung und Wiederherstellung des Systems auf einen USB-Stick:
    die zu kopierenden Ordner der Datenpartition werden mit ihrer
    Größe ausgewählt, der freie Platz auf dem Stick wird angezeigt
    und geprüft, der Fortschritt wird angezeigt; auch Sticks ohne
    Partitionstabelle werden gefunden
* Setup > Screen dreht das Display und setzt die Helligkeit schon
  beim Ändern der Zeile, kalibriert den Touchscreen und behält die
  Ausrichtung über einen Neustart; ein Schalter "Touchscreen" sagt
  dem Programm, ob ein Touch-Panel angeschlossen ist; der SSH-Server
  ist ein Schalter unter Setup > Netzwerk
* eine am Gerät angeschlossene Tastatur schreibt Kleinbuchstaben und
  alle Sonderzeichen im Layout aus XKB_DEFAULT_LAYOUT `[upstream PR]`
* eigene Tasten-, Menü- und Gestenbelegung für das OpenVario, in
  Eingabedateien nach Themen

## Karte

* Web-Mercator-Projektion mit den Kachel-Zoomstufen als Zoomschritte
  (der Kartentitel zeigt die Stufe), so dass Rasterkacheln pixelgenau
  erscheinen und die Topographie nicht mehr gegen Wegpunkte und
  Lufträume verrutscht
* eine Grundkarte pro Seite: Gelände und Topographie, nur
  Topographie, ein leerer Hintergrund oder OpenStreetMap-Kacheln, die
  im Cache bleiben
* Topographie, Lufträume und ihre Beschriftungen kommen aus
  zwischengespeicherten Ebenen und werden nur neu gezeichnet, wenn
  sich die Ansicht ändert - viel schneller bei großen Maßstäben,
  wodurch die Zoom-Grenze für die Mali-400-GPU überflüssig wurde
* Luftraum-Füllmodi "Dünne Linien" und "Dünne Linien, Warnungen
  füllen"
* ein Tipp auf den Endanflugbalken öffnet die Flugeinstellungen, die
  mit dem MacCready-Wert beginnen
* Wetter- und Kartenüberlagerungen nehmen ihre Kachelstufe von der
  Karte; die Flugmodus- und GPS-Symbole behalten eine physische
  Mindestgröße

## Gleitrechner

* ein Start beginnt einen neuen Flugweg und Wettbewerb, ein
  Wiederstart zählt den Flug davor also nicht mehr mit
* ein kurzer Zeitsprung zurück in den Daten eines Loggers beendet
  den Flug nicht mehr, und die Ausklinkung wird erst 100 m über dem
  Start gesucht

## Eingabe und Schnellmenü

* mehrere Eingabe-Event-Dateien, gelesen in der Reihenfolge ihrer
  Namen, ein Thema pro Datei, auf XCSoars unveränderter default.xci
* der Schnellmenü-Stil "Dynamisch": eine eigene Liste vor dem Flug,
  im Flug und nach der Landung, die Start und Landung folgt; so viele
  Spalten wie passen, jede Schaltfläche an ihrem Platz; die
  Kartenanzeige-Schalter im Dialog "Kartenanzeige"
* das Analysis-Event nimmt einen Seitennamen; die Taste K blättert
  rückwärts durch die InfoBox-Layouts; mit -touchscreen bleibt der
  Mauszeiger auf Desktops sichtbar

## Flugbuch

* jeder Flug im Info-Menü und in `logbook.csv` im Datenverzeichnis:
  Start und Landung, Startart und Ausklinken, Besatzung, Flugzeug,
  Logger, freie und DMSt-Strecke mit Geschwindigkeit und Punkten,
  maximale Höhe und die Logdateien; gebildet aus den aufgezeichneten
  IGC-Dateien und NMEA-Logs, wobei die Dateien von Recorder 1 und 2
  Vorrang haben, exportierbar auf einen USB-Stick; IGC-Dateien landen
  im Ordner "igc"

## Vario-Seite

* ein großes rundes Vario als eigene Seite, nach dem Display des
  Larus-Frontend-Projekts, mit Ansichten je Flugzustand einstellbar
  `[upstream PR geplant]`

## Momentanwind

* der Live-Wind von Sensoren wie Larus und Anemoi erscheint auf der
  Karte als zweiter Pfeil neben dem Mittelwert und wird von der
  Vario-Seite verwendet

## Wiedergabe

* LRSX-Sensorlogs werden direkt abgespielt; der Wiedergabe-Dialog ist
  ein Kassettendeck mit Sprüngen vor und zurück, zum Start und ans
  Ende

## InfoBoxen

* OpenSoars eigene InfoBoxen, nummeriert ab 500, damit XCSoar-Updates
  sie nie verschieben, mit eigenen Gruppen in der Auswahl

## Geräte

* ein eingetragenes, aber nicht angeschlossenes Gerät wird einmal pro
  Minute neu versucht und einmal pro Stunde protokolliert
* die System-Seite zeigt die benutzten Geräte (ohne den RemoteStick
  auf seinem eigenen Slot) und öffnet die Geräteliste
* Display-Auflösungen für 5,7"- und 7"-Geräte wie das SteFlyNav und
  eine Display-Auflösung für das Gerät selbst in den
  Systemeinstellungen
* eine Larus-Sensorbox lässt sich in der Geräteliste verwalten: der
  Dialog zeigt die Firmware-Version, die die Box gemeldet hat ($PLARF)

## Dialoge

* Fortschrittsbalken zeigen ihren Text und die Prozentzahl im Balken
* die Bildschirmtastatur hat eine Sonderzeichen-Seite für alle
  druckbaren ASCII-Zeichen `[upstream PR]`
* schreibgeschützte Felder öffnen ihren vollen Inhalt nur dort, wo
  das Feld ihn abschneidet; eine neue Sprache, die
  Display-Ausrichtung, die Helligkeit und der Schalter Testversionen
  wirken sofort

## Repository und Downloads

* die Download-Listen bieten auch das OpenSoar-Repository, die
  OpenVario-Images und die Pakete des aktuellen OpenSoar-Release an
  (mit System > System > Testversionen auch die Testversionen); jede
  Datei zeigt Datum und Einzelheiten in einer zweiten Zeile
* Repository-Einträge werden vor und nach dem Download geprüft: ein
  Name ohne zum Typ passende Endung wird mit einer Meldung abgelehnt,
  und eine heruntergeladene Datei, deren Inhalt nicht zum Typ passt
  (eine "404"-Webseite, eine leere Datei, das falsche Format), ergibt
  eine klare Fehlermeldung
* die RASP-Datei wird nur noch auf der RASP-Seite der
  Wettereinstellungen gewählt, nicht mehr unter Standortdateien

## Frequenzkarte

* eine kleine Frequenzlisten-Datei pro Wettbewerb (`*.xcf`, gewählt
  unter System > Standortdateien > Funkfrequenzen), ein Fingertipp
  zur Standby- oder Aktiv-Frequenz; einfacher Text
  ("Name : Frequenz") oder JSON; erreichbar über Info-Seite 4/4, das
  RemoteStick-Menü und das Eingabe-Event `FrequencyCard`

## Korrekturen vor Upstream

* dieselbe Statusmeldung, die viele Male hintereinander kommt, wird
  einmal angezeigt
* Wetterüberlagerungen: libtiff wird mit zlib gebaut, so dass die
  komprimierten Vorhersagebilder gelesen werden können
* Blockbilder in formatiertem Text behalten ihre physische Größe auf
  Displays mit hoher Pixeldichte
* der Konfigurationsdialog stürzt bei einem späteren Download nicht
  mehr ab, das Kobo-Menü startet das Programm unter seinem eigenen
  Namen, und der Linux-Build findet seine Übersetzungen

## Schon in XCSoar

Diese OpenSoar-Änderungen wurden in XCSoar übernommen und haben die
Liste verlassen: der Exit-Code beim Beenden während des Starts und die
Migration vor dem Laden des Profils (in XCSoar 7.45.3) sowie in
XCSoar 7.46 der Port-Monitor-Fix für leere Grids, der durch gcc 15
kaputte Winkelvergleich, die Sicherung, die gerade benutzte Dateien
auslässt, der wiederholte serielle Schreibversuch unter Windows, der
Filter der Download-Auswahl nach Land und Text und die SDL-Timer, die
bei ständig eintreffenden Ereignissen stehen blieben.  Die
Eingabe-Events InfoBoxGeometry, DarkMode und TextSize stammen
ebenfalls aus XCSoar 7.46.  Der frühere CUPX-Workaround wurde
hinfällig, als upstream das Grundproblem zentral behob.

## Build- und Release-Infrastruktur

* nativer Windows-Build mit CMake und Visual Studio (2022/2026), nur
  noch OpenGL-Rendering; alle Fremdbibliotheken werden beim ersten
  Build automatisch mitgebaut; jeder Branch baut in einem eigenen
  Ordner mit eigener Solution, und ein Build im Ordner eines anderen
  Branches bricht ab, bevor etwas übersetzt wird
* OpenGL überall, auch im make-Build (TARGET=WIN64 ist der
  OpenGL/ANGLE-Build)
* make: Marken-Ziele und Paketnamen aus `OpenSoar.config`,
  OpenVario-Ziele OPENVARIO und CUBIE2; `MakeAll.sh` baut alle
  veröffentlichten Ziele und sammelt die Pakete (Windows-Zip mit den
  ANGLE-DLLs, Debian-Paket, KoboRoot.tgz)
* das Log heißt OpenSoar.log, die Logs der letzten neun Läufe bleiben
  erhalten; langsame Kartenbilder werden mit dem Teil protokolliert,
  der die Zeit brauchte
* GitHub-CI baut und veröffentlicht das Windows-Paket für jedes
  Release-Tag; `.tN`-Tags werden Pre-Releases mit der Testing-Variante
* Versionsschema: `MAJOR.MINOR` folgen der XCSoar-Basisversion, die
  dritte Zahl ist der OpenSoar-Release-Zähler, `.tN` bezeichnet die
  N-te Testversion, eine numerische vierte Stelle ein Bugfix-Release
