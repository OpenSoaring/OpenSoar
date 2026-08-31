# OpenSoar - differences to XCSoar

*Deutsche Fassung: siehe `OpenSoar-AddOn-de.md`.*

OpenSoar is XCSoar plus a small stack of additions.  This file is the
complete, current list of those differences - if something is not
listed here, it behaves exactly like XCSoar.

House rule: as soon as one of these items is merged into XCSoar, it
is no longer an add-on and gets REMOVED from this list.  Items marked
`[upstream PR]` are already submitted and expected to leave the list.

## Additional drivers

| Driver | Manufacturer | Remark |
|:------ |:------------ |:------ |
| **SteFly RemoteStick** | SteFly | stick remote control; auto-detected (USB 1209:8500) on a dedicated device slot - it never occupies one of the user-configurable ports; Manage dialog with Send / Receive / Restart |
| **SteFly RotaryPanel** | SteFly | rotary control panel |
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
  update

## Startup and leaving the program

* the profile dialog is the start screen: it always appears, with a
  configurable countdown ("Startup timeout"); any input stops the
  countdown, zero waits for the user; `-profile=X` preselects X
* the fly/simulator prompt appears only with the `-ask` command line
  option (`-simulator` works as before)
* one power dialog for leaving: Quit / Restart / Reboot / Shutdown,
  each only where the target can do it (Android: Quit and Restart;
  Kobo and OpenVario: all four; desktop: Quit and Restart)
* wherever XCSoar asks the user to quit and restart by hand, OpenSoar
  offers the internal restart directly
* a per-device system setting restores the XCSoar behaviour for
  everybody who prefers it

## Device-specific settings outside the profile

* system settings (XCSoar behaviour, devices in the profile) live in
  a file outside the data folder - they belong to the device, not to
  a profile, and do not travel when the data folder is copied
* the NMEA devices and their ports live in `device_ports.xcd`, a JSON
  file carrying only non-default fields; an old key=value file is
  converted automatically; a switch brings back the XCSoar way
  (ports in the profile) per device

## File manager

* repository entries are checked before and after the download: a
  name without an extension matching the declared type is refused
  with a message, and a downloaded file whose content does not match
  the type (a "404" web page, an empty file, the wrong format)
  produces a clear error message

## Frequency card

* a small frequency list file per competition (`*.xcf`, selected
  under System > Site Files > Radio frequencies), one tap to the
  standby or active frequency; plain text ("name : frequency") or
  JSON; reachable via Info page 4/4, the RemoteStick menu and the
  `FrequencyCard` input event

## Fixes ahead of upstream

* port monitor no longer crashes on MSVC debug builds (undefined
  behaviour in a grid container) `[upstream PR]`

(The startup exit-code fix and the migration-before-profile-load fix
were merged into XCSoar and left this list; the former CUPX
binary-mode workaround became obsolete when upstream fixed the
underlying problem centrally.)

## Build and release infrastructure

* native Windows build with CMake and Visual Studio (2022/2026),
  OpenGL rendering only; all third-party libraries are built
  automatically as part of the first build
* GitHub CI builds and publishes the Windows package for every
  release tag; `.tN` tags become pre-releases with the testing flavor
* versioning scheme: `MAJOR.MINOR` follow the XCSoar base version,
  the third number is the OpenSoar release counter, `.tN` marks the
  N-th test version, a numeric fourth field marks a bugfix release

## Planned (not yet active in this version)

* OpenVario: device-specific system settings (WiFi, display,
  rotation, shutdown/reboot) integrated into OpenSoar instead of a
  separate base-menu application - the code base is prepared, the
  user interface follows in one of the next versions
