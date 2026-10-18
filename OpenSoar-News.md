# OpenSoar News

OpenSoar v7.45.25 is built on top of XCSoar 7.45.3, so it brings along
the many fixes of the XCSoar 7.45 branch.  On top of that come the
OpenSoar additions listed below, most of all a reworked OpenVario
integration, a log book, a quick menu per flight phase and a much
faster map.

OpenSoar is built ON TOP of XCSoar: every OpenSoar version is the
current XCSoar master plus a small, well-defined stack of OpenSoar
additions (see `OpenSoar-AddOn.md` for the complete list of
differences).  This file therefore lists ONLY the OpenSoar additions
and changes per version - everything the XCSoar base brings along is
documented upstream in `NEWS.txt`.

House rule: as soon as a feature listed here is merged into XCSoar
itself, it stops being an OpenSoar item - it is removed from
`OpenSoar-AddOn.md` and future entries here simply ride along with the
XCSoar base.  Entries marked `[upstream PR]` are already submitted to
XCSoar and expected to disappear from this list.

Only releases are listed.  A test version (vX.Y.Z.tN) shows the
release it is heading for, in whatever state the list has reached.

The pre-7.45 history (OpenSoar 7.43/7.44 with its merge-based
bookkeeping) lives in the old repository and is not repeated here -
7.45.25 is the first release built with the rebase workflow.

---

## OpenSoar 7.45.25 - not released yet

Base: XCSoar 7.45.3 of 2026-09-22, that is XCSoar master as of
2026-08-28 (the 7.45 feature set: OpenGL rendering on all targets,
upstream SkySight, reworked data-file layout, PEV, dark mode, ...)
plus the fixes of 7.45.1 to 7.45.3.  This list names everything
OpenSoar 7.45.25 adds to or changes in XCSoar 7.45.3; the NEWS.txt
of this release is the one of XCSoar 7.45.3, unchanged.  The base
already contains the OpenSoar fixes merged upstream in August 2026
(startup exit code, migration-before-profile-load, CUPX binary mode).

* complete rebuild on XCSoar 7.45.3; all OpenSoar features below are
  re-applied as a clean patch stack on top
* startup: the profile dialog is the start screen - it always
  appears, with a countdown (4 s, configurable as "Startup timeout"
  in System > Site Files > System; any input stops it, zero waits);
  `-profile=X` preselects X instead of skipping the dialog; the
  fly/simulator prompt appears only on request (`-ask`; `-simulator`
  still works)
* leaving the program: one power dialog offering Quit / Restart /
  Reboot / Shutdown, each only where the target can actually do it -
  Restart also on Android, where the app launches a fresh instance
  of itself; quitting gives immediate feedback instead of a frozen
  screen, and the Windows system-menu close uses the same dialog.
  Wherever XCSoar asks the user to quit and restart by hand (profile
  switch, changed system settings), OpenSoar offers the internal
  restart directly.  On the OpenVario the dialog offers Shutdown
  first, because that is how the instrument is switched off after the
  flight; the keys Q and X open it with the focus on Quit or Shutdown
* device settings out of the profile:
  - system settings of the device (XCSoar behaviour on/off, devices
    in the profile on/off) live in a file outside the data folder,
    so they survive profile changes and do not travel when the data
    folder is copied to another device; settings page under
    System > System
  - the NMEA devices and their ports live in `device_ports.xcd`,
    which is now a JSON file with only the non-default fields; an old
    key=value `device_ports.xcd` is converted automatically, and a
    per-device switch brings back the XCSoar way (ports in the
    profile) for machines that fly with several instrument sets
  - on the OpenVario these settings live on the data partition
    (`~/data/.config/opensoar`), so a firmware upgrade keeps them; an
    installation that only has the old folder is moved over once
  - the cache (map tiles, weather files) is no longer kept inside the
    data folder: it goes to `~/data/.cache/OpenSoar` on the OpenVario
    and to `$XDG_CACHE_HOME/OpenSoar` or `~/.cache/OpenSoar` on a Linux
    PC, so it does not travel when the data folder is copied
* OpenVario: the system functions are one page, System > OpenVario,
  instead of the OpenVario menu of OpenSoar 7.44 and the separate
  OpenVarioBaseMenu; WiFi and the data transfer use the regular
  XCSoar pages (Setup > Network, Data Management), so switching
  between XCSoar and OpenSoar on the same device shows the same menus
  [upstream PR]
  - the program the image starts after boot (XCSoar or OpenSoar), each
    choice with the version installed
  - the running image and the firmware upgrade: a click opens the list
    of images in the download directory (`data/download`) and on the
    USB stick, with Download, Delete and a filter for the hardware of
    the device; OpenSoar saves the profile, quits, and the start script
    runs the upgrade on the console
  - sensord and variod as switches; switching a service on restarts
    it and enables it for the next boot, and if it does not stay
    running, the message says why
  - sensor calibration, and backup and restore of the whole system to
    a USB stick
  - Look > Display calibrates the touch screen and keeps the display
    orientation over a reboot; the SSH server is a switch on
    Setup > Network
  - a keyboard attached to the device types lower case letters and all
    symbols in the layout given by XKB_DEFAULT_LAYOUT [upstream PR]
  - its own key, menu and gesture configuration, in files by topic
    (`Data/Input/OpenVario/*.xci`), built in for the OpenVario targets
* map
  - the map is drawn in the Web Mercator projection, the projection of
    slippy map tiles, and the zoom steps are the tile zoom levels, one
    factor of two apart (the map title shows the level, e.g. "Z12"),
    so raster tiles appear pixel for pixel; topography no longer
    drifts away from waypoints and airspaces towards the screen edges
  - a base map per page: terrain and topography, topography only, an
    empty background, or OpenStreetMap tiles, which are downloaded
    while online and kept in the cache
  - much faster at large map scales on OpenGL: topography, airspaces
    and the topography labels are drawn from cached images and only
    redrawn when the view really changes, instead of with every GPS
    fix.  On an OpenVario zoomed out to 737 km the program took one
    full core before and about 9 % now, so the zoom limit for the
    Mali-400 GPU is no longer needed and has been removed (the one for
    the PowerVR GE8300, a driver crash, stays)
  - new airspace fill modes "Thin lines" (no fill, outlines one pixel
    wide) and "Thin lines, fill warnings" (thin outlines, filled only
    where a warning applies or the glider is inside); the altitude
    labels are drawn only up to 100 km screen width, where they can
    still be read
  - a tap on the final glide bar opens the flight setup, which starts
    with the MacCready setting
  - weather and map overlays (SkySight, MBTiles) take their tile level
    from the map, so their tiles are no longer stretched and blurred
  - the flight mode and GPS icons in the corners keep a minimum
    physical size (about 7 mm) on desktop monitors
* glide computer
  - a takeoff starts a new trace and contest: the second flight of a
    session (the next winch launch, a relaunch) no longer counts the
    one before
  - a jump back in time of less than three minutes, which some loggers
    write, no longer ends the flight in the air
  - the release from the tow is looked for only 100 m above the
    takeoff, and a takeoff forgets the final glide of the last flight,
    so the release time of a flight is not set too early
* input
  - InfoBoxGeometry, DarkMode and TextSize input events (from XCSoar
    7.46): change the InfoBox layout, night mode and text size in
    flight, default keys L, N, T and S
  - the Analysis event takes a page name, e.g. "Analysis barograph"
  - with -touchscreen the mouse cursor stays visible on desktops (SDL),
    and Android detects cursor keys at startup, so the first dialog
    already shows its focus
* quick menu: the style "Dynamic" shows a list of its own before the
  flight, in flight and after the landing, follows takeoff and
  landing, and can be switched to the complete list; as many columns
  as fit, every button at its location, scrolling row by row, Quit as
  the last button.  The map display switches moved into the dialog
  "Map Display", "Page settings" replaces the terrain and topography
  toggles.  Several input events files are read in the order of their
  names, one topic per file, on top of XCSoar's unchanged default.xci
* log book: every flight in the Info menu and in `logbook.csv` (a table
  for a spreadsheet) - takeoff and landing with time and place, launch
  and release, crew, aircraft, flight recorder, free and DMSt distance
  with speed and points from the release (the best part between engine
  runs), maximum altitude and the log files.  It is built from the
  recorded IGC files and NMEA logs, one entry per flight from the most
  complete recording, and the files of Recorder 1 and 2 (logger
  settings) win; two minutes after the landing the newest flight is
  downloaded from them.  "Export" copies the log book to a USB stick,
  the WeGlide flight id is noted after the upload.  IGC files go to
  the folder "igc", apart from the NMEA logs in "logs"
* repository and downloads: the download lists also offer the OpenSoar
  repository and the OpenVario images (both on opensoar.de), the
  indexes are fetched again when a download page opens (in test
  versions every time), and every file shows its date and further
  details in a second row; data management and file manager are also
  pages of System in the configuration
* download picker: the list can be filtered by country and by text
* file manager: downloads are checked - a repository entry whose
  name has no extension matching its type is refused with a message
  before the download (the file would never appear in any list), and
  after the download the content is compared with the declared type,
  so a "404" web page, an empty file or a wrong file type produces a
  clear error message instead of silent nothing
* frequency card: the OpenSoar frequency list is back as a second way
  next to the checklist links - one small file per competition
  (System > Site Files > Radio frequencies, `*.xcf`), one tap to the
  standby or active frequency.  The plain text list ("Stuttgart
  Info : 128.950") is read unchanged; alternatively the file can be
  JSON with comments shown as a second line.  Reachable via the new
  Info page 4/4, the RemoteStick menu and the `FrequencyCard` event
* vario display page: a big round vario as a page of its own
  (Config > Look > Pages, main area "Vario") - scale ring with
  climb needle, 30 s averager and MacCready markers, speed command
  arc, wind arrow (absolute with North mark while circling, heading
  relative with a glider silhouette in cruise), wind and thermal
  climb / speed to fly read-outs, plus the final glide bar, flight
  mode icon and GPS state known from the map.  All four views
  (centre, top row, bottom row, right margin) are configurable per
  flight state - circling and straight flight separately, stored in
  the profile - under Config > Look > Vario Display, also reachable
  through the quick menu button "Vario Display" and by a tap on the
  page itself; the choices match the original's view lists
  (arrows, thermal assistants, and eighteen info line values from
  average climb to UTC time).  The design follows
  the display of the Larus frontend project
  (github.com/larus-breeze/sw_frontend_rs) - thanks for the
  inspiration!  The values come from OpenSoar's own computations, so
  the page works with any connected vario - with a Larus sensor box
  it shows that box's values  [upstream PR planned]
* instantaneous wind: sensors that deliver the live wind next to the
  average (Larus PLARW "I" sentence, Anemoi) feed it to the map,
  which now shows two arrows - the average in light blue (gray when
  OpenSoar estimated it itself), the live wind in light green - and
  to the vario display page, whose centre arrow prefers it
* replay: flight sensor logs in the LRSX format (*.lrsx) play
  directly - pick the file in the replay dialog like an IGC or NMEA
  log, no external player needed.  The raw GNSS fixes, pressures,
  supply voltage, g load and (with a D-GNSS compass) the true
  heading feed the replay; vario, wind and circling come from the
  usual computations.  The replay dialog is a tape deck for all
  replay files, its buttons drawn as icons in one row: |< start,
  T/O takeoff, << back 10 minutes (jumps work backwards too), a
  play/pause toggle, >> forward 10 minutes, and >| plays the rest
  at once, so the whole flight lands in the trail and the
  statistics (barogram, distances, speeds).  "Hide" closes only the
  window and leaves the replay running; "Cancel" ends it
* InfoBoxes: OpenSoar's own InfoBoxes are back, numbered from 500 so
  that XCSoar updates never shift them (old OpenSoar profiles keep
  working), with groups of their own in the picker
* devices
  - SteFly device family: RemoteStick (auto-detected on its own
    device slot, never occupying a user-configurable port) and
    RotaryPanel, incl. Manage dialog (Send/Receive/Restart)
  - Anemoi (RS-Flight) realtime wind driver
  - Becker AR62xx radio driver
  - FreeVario driver
  - the RemoteStick is also found at startup on Android and on Linux
    without libudev (OpenVario)
  - a configured device that is not connected is retried once a minute
    and logged once an hour, instead of three log lines every 30
    seconds
  - a Windows serial write that moves no byte (a USB adapter still
    coming up) is retried instead of stopping a debug build
  - the System page lists the devices in use by their drivers and opens
    the device list
  - display resolutions "140 dpi (5.7", 640x480 px)" and "180 dpi
    (7.0", 1024x600 px)" for devices such as the SteFlyNav, and a
    display resolution for the device itself in the system settings;
    an Android display that reports the placeholder 213 dpi gets the
    density the device is set to
* branding / user interface
  - OpenSoar name, graphics and icons on all screens; start screen
    shows the full version number prominently
  - test versions (vX.Y.Z.tN) use the red "testing" flavor including
    red application icons; releases are green
  - the "what's new" quick guide only appears when the XCSoar base
    version (major.minor) changes - not for every OpenSoar update
  - the About page names OpenSoar and its web site, shows the sponsor
    logo next to the OpenSoar logo, and carries these OpenSoar news
    as a page of its own; logos scale with the display density
* fixes (also submitted to XCSoar)
  - status messages: the same message arriving many times in a row
    (a condition monitor firing behind a modal dialog, say) is shown
    once instead of filling the message window with a hundred
    identical lines
  - backup: a running logger no longer fails the whole backup - the
    files being written right now (the NMEA log, the IGC file) are
    left out of the archive and named afterwards; any other
    unreadable file still fails it, and backup and restore refuse
    to run while flying [upstream PR]
  - weather overlays stayed empty after a fresh CMake build: libtiff
    was configured without zlib, so the DEFLATE compression of the
    downloaded forecast images could neither be written nor read
    ("requested compression method is not configured"); the SkySight
    display now also says in the log why an overlay is missing
  - angle comparison broken by newer compilers: a too-strong
    attribute on Angle::CompareRoughly ("const" instead of "pure" on
    a member function) let gcc 15 drop the store into the compared
    temporary - TestAngle failed, and FAI quadrants from .cup files
    silently became plain sectors; same fix in the histogram and the
    RASP store [upstream PR]
  - SDL event queue: timers no longer starve when events keep
    arriving faster than the timer period - the cached clock was
    only refreshed on an event-free timeout, so on busy displays
    (e.g. Windows/ANGLE) every timer slower than the event stream
    stood still: replay stuck after pause, process timer stalled
  - fix undefined behaviour when a terminal/grid widget grows from
    empty (port monitor crash on MSVC debug builds) [upstream PR]
  - block images in rich text (checklists, About page) keep their
    physical size on high-density displays instead of shrinking to
    thumbnails; two images on one line are laid out side by side
* dialogs: progress bars show their text inside the bar with the
  percentage behind it; the on-screen keyboard has a symbol page, so
  passwords and file names can contain all printable ASCII characters
  [upstream PR]; read-only fields open their full content only where
  the field cuts it off; the takeoff button of the replay shows a
  clearer departure symbol
* fixes: the configuration dialog no longer crashes at the next
  download after it was closed (a page stayed registered with the
  download manager); the Kobo menu starts OpenSoar from the
  KoboRoot.tgz (it looked for a program called xcsoar)
* infrastructure
  - the log is OpenSoar.log, and the logs of the last nine runs are
    kept as OpenSoar-1.log to OpenSoar-9.log
  - map frames that take longer than 300 ms are logged, with the part
    of the drawing that took the time; the log also names the file of
    a TIFF read error
  - make: branded targets and package names from OpenSoar.config,
    OpenVario targets OPENVARIO and CUBIE2, `make install` under the
    program's own name; CI builds Android, Kobo, Linux and macOS as
    well; Android debug builds can be attached to by a debugger
  - Windows: native CMake/MSVC build (Visual Studio 2022/2026),
    OpenGL-only; third-party libraries build automatically
  - OpenGL everywhere, also in the make build: TARGET=WIN64 is the
    OpenGL/ANGLE build (upstream's WIN64OPENGL) - the GDI flavor and
    the *OPENGL alias targets are gone.  Without OpenGL there were
    no weather map overlays (SkySight forecasts downloaded fine but
    never appeared)
  - GitHub CI: every release tag builds and publishes the Windows
    package automatically; testing tags become pre-releases
* German translations for everything above
