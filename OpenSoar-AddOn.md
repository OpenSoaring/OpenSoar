# OpenSoar - differences to XCSoar

*Deutsche Fassung: siehe `OpenSoar-AddOn-de.md`.*

OpenSoar is XCSoar plus a small stack of additions.  This file is the
complete, current list of those differences - if something is not
listed here, it behaves exactly like XCSoar.

House rule: as soon as one of these items is merged into XCSoar, it
is no longer an add-on and gets REMOVED from this list.  Items marked
`[upstream PR]` are already submitted and expected to leave the list.

This version of the list belongs to OpenSoar 7.46.26, which is built
on XCSoar 7.46.

## Additional drivers

| Driver | Manufacturer | Remark |
|:------ |:------------ |:------ |
| **SteFly RemoteStick** | SteFly | stick remote control; auto-detected (USB 1209:8500) on a dedicated device slot - it never occupies one of the user-configurable ports; Manage dialog with key configuration, double click and long press time, mouse step, device information and Restart |
| **SteFly RotaryPanel** | SteFly | rotary control panel; auto-detected (USB 1209:8502) on a dedicated device slot of its own like the RemoteStick, not editable; same Manage dialog as the RemoteStick |
| **Anemoi** | RS-Flight | realtime wind measurement |
| **Becker AR62xx** | Becker | radio driver |
| **FreeVario** | Blaubart | FreeVario protocol |

## Branding and user interface

* OpenSoar name, logos and icons; the start screen shows the full
  version number prominently
* test versions (vX.Y.Z.tN) build the red "testing" flavor including
  red application icons - a test installation is recognizable at a
  glance; releases are green
* the "what's new" quick guide appears only when the underlying
  XCSoar base version (major.minor) changes, not for every OpenSoar
  update, and shows the OpenSoar news of the release
* the About page names OpenSoar and its web site, shows the sponsor
  logo next to the OpenSoar logo and carries the OpenSoar news as a
  page of its own

## Startup and leaving the program

* the profile dialog is the start screen: it always appears, with a
  configurable countdown ("Startup timeout"); any input stops the
  countdown, zero waits for the user; `-profile=X` preselects X
* the fly/simulator prompt appears only with the `-ask` command line
  option (`-simulator` works as before)
* one power dialog for leaving: Quit / Restart / Reboot / Shutdown,
  each only where the target can do it (Android: Quit and Restart;
  Kobo and OpenVario: all four; desktop: Quit and Restart); on the
  OpenVario Shutdown comes first, and the keys Q and X open the dialog
  on Quit or Shutdown
* wherever XCSoar asks the user to quit and restart by hand, OpenSoar
  offers the internal restart directly
* a per-device system setting restores the XCSoar behaviour for
  everybody who prefers it

## Device-specific settings outside the profile

* system settings (XCSoar behaviour, devices in the profile, display
  resolution of the device, test versions) live in a file outside the
  data folder - they belong to the device, not to a profile, and do
  not travel when the data folder is copied
* the NMEA devices and their ports live in `device_ports.xcd`, a JSON
  file carrying only non-default fields; an old key=value file is
  converted automatically; a switch brings back the XCSoar way
  (ports in the profile) per device
* on the OpenVario these files live on the data partition, so a
  firmware upgrade keeps them
* the cache (map tiles, weather files) is kept outside the data
  folder (`~/data/.cache/OpenSoar` on the OpenVario, the XDG cache
  folder on Linux)

## OpenVario

* the system functions are one configuration page, System >
  OpenVario, instead of a menu of their own and the separate
  OpenVarioBaseMenu; WiFi and data transfer use the regular XCSoar
  pages `[upstream PR]`:
  - which program the image starts after boot (XCSoar or OpenSoar)
  - the running image and the firmware upgrade from the download
    directory or a USB stick, with Download, Delete and a filter for
    the hardware of the device
  - sensord and variod as switches, sensor calibration
  - backup and restore of the system to a USB stick: the folders of
    the data partition to copy are chosen with their sizes, the free
    space on the stick is shown and checked, and the progress is
    shown; sticks without a partition table are found as well
* Setup > Screen turns the display and sets the brightness while the
  row changes, calibrates the touch screen and keeps the orientation
  over a reboot; a "Touch screen" switch tells the program whether a
  touch panel is connected; the SSH server is a switch on Setup >
  Network
* a keyboard attached to the device types lower case letters and all
  symbols in the layout of XKB_DEFAULT_LAYOUT `[upstream PR]`
* own key, menu and gesture configuration for the OpenVario, in input
  files by topic

## Map

* Web Mercator projection with the tile zoom levels as zoom steps (the
  map title shows the level), so raster tiles appear pixel for pixel
  and topography no longer drifts against waypoints and airspaces
* a base map per page: terrain and topography, topography only, an
  empty background, or OpenStreetMap tiles kept in the cache
* topography, airspaces and their labels are drawn from cached layers
  and redrawn only when the view changes - much faster at large map
  scales, which made the zoom limit for the Mali-400 GPU unnecessary
* airspace fill modes "Thin lines" and "Thin lines, fill warnings"
* a tap on the final glide bar opens the flight setup, which starts
  with the MacCready setting
* weather and map overlays take their tile level from the map; the
  flight mode and GPS icons keep a minimum physical size

## Glide computer

* a takeoff starts a new trace and contest, so a relaunch no longer
  counts the flight before
* a short jump back in time in a logger's data no longer ends the
  flight, and the release from the tow is looked for only 100 m above
  the takeoff

## Input and quick menu

* several input events files, read in the order of their names, one
  topic per file, on top of XCSoar's unchanged default.xci
* the quick menu style "Dynamic": a list of its own before the flight,
  in flight and after the landing, following takeoff and landing; as
  many columns as fit, every button at its location; the map display
  switches in the dialog "Map Display"
* the Analysis event takes a page name; the key K steps back through
  the InfoBox layouts; with -touchscreen the mouse cursor stays
  visible on desktops

## Log book

* every flight in the Info menu and in `logbook.csv` in the data
  directory: takeoff and landing, launch and release, crew, aircraft,
  flight recorder, free and DMSt distance with speed and points,
  maximum altitude and the log files; built from the recorded IGC
  files and NMEA logs, with the files of Recorder 1 and 2 taking
  precedence, exportable to a USB stick; IGC files go to the folder
  "igc"

## Vario display page

* a big round vario as a page of its own, after the display of the
  Larus frontend project, with views configurable per flight state
  `[upstream PR planned]`

## Instantaneous wind

* the live wind of sensors such as Larus and Anemoi is shown on the
  map as a second arrow next to the average and used by the vario
  display page

## Replay

* LRSX flight sensor logs play directly; the replay dialog is a tape
  deck with jumps forward and backward, to the takeoff and to the end

## InfoBoxes

* OpenSoar's own InfoBoxes, numbered from 500 so that XCSoar updates
  never shift them, with groups of their own in the picker

## Devices

* a configured device that is not connected is retried once a minute
  and logged once an hour
* the System page lists the devices in use (without the RemoteStick
  and the RotaryPanel on their own slots) and opens the device list
* display resolutions for 5.7" and 7" devices such as the SteFlyNav,
  and a display resolution for the device itself in the system
  settings
* a Larus sensor box can be managed in the device list: the dialog
  shows the firmware version the box reported ($PLARF)

## Dialogs

* progress bars show their text and percentage inside the bar
* the on-screen keyboard has a symbol page for all printable ASCII
  characters `[upstream PR]`
* read-only fields open their full content only where the field cuts
  it off; changing a language, the display orientation, the
  brightness or the test versions switch takes effect at once

## Repository and downloads

* the download lists also offer the OpenSoar repository, the
  OpenVario images and the packages of the current OpenSoar release
  (with System > System > Test versions also the test versions); every
  file shows its date and details in a second row
* repository entries are checked before and after the download: a
  name without an extension matching the declared type is refused
  with a message, and a downloaded file whose content does not match
  the type (a "404" web page, an empty file, the wrong format)
  produces a clear error message
* the RASP file is chosen only on the RASP page of the weather
  settings, no longer in Site Files

## Frequency card

* a small frequency list file per competition (`*.xcf`, selected
  under System > Site Files > Radio frequencies), one tap to the
  standby or active frequency; plain text ("name : frequency") or
  JSON; reachable via Info page 4/4, the RemoteStick menu and the
  `FrequencyCard` input event

## Fixes ahead of upstream

* the same status message arriving many times in a row is shown once
* weather overlays: libtiff is built with zlib, so the compressed
  forecast images can be read
* block images in rich text keep their physical size on high-density
  displays
* the configuration dialog no longer crashes at a later download, the
  Kobo menu starts the program under its own name, and the Linux
  build finds its translations

## Already in XCSoar

These OpenSoar changes were merged into XCSoar and left this list: the
startup exit code and the migration before loading the profile (in
XCSoar 7.45.3), and in XCSoar 7.46 the port monitor fix for empty
grids, the angle comparison broken by gcc 15, the backup that leaves
out files in use, the retried Windows serial write, the download
picker filter by country and text, and the SDL timers that starved
while events kept arriving.  The InfoBoxGeometry, DarkMode and
TextSize input events come from XCSoar 7.46 as well.  The former CUPX
binary-mode workaround became obsolete when upstream fixed the
underlying problem centrally.

## Build and release infrastructure

* native Windows build with CMake and Visual Studio (2022/2026),
  OpenGL rendering only; all third-party libraries are built
  automatically as part of the first build; every branch builds in a
  directory and a solution of its own, and a build in the directory
  of another branch stops before anything is compiled
* OpenGL everywhere, also in the make build (TARGET=WIN64 is the
  OpenGL/ANGLE build)
* make: branded targets and package names from `OpenSoar.config`,
  OpenVario targets OPENVARIO and CUBIE2; `MakeAll.sh` builds all
  published targets and collects the packages (Windows zip with the
  ANGLE DLLs, Debian package, KoboRoot.tgz)
* the log is OpenSoar.log with the logs of the last nine runs kept;
  slow map frames are logged with the part that took the time
* GitHub CI builds and publishes the Windows package for every
  release tag; `.tN` tags become pre-releases with the testing flavor
* versioning scheme: `MAJOR.MINOR` follow the XCSoar base version,
  the third number is the OpenSoar release counter, `.tN` marks the
  N-th test version, a numeric fourth field marks a bugfix release
