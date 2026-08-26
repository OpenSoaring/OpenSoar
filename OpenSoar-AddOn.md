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
